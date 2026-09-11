//! Direct GRBL streaming over serial.
//!
//! PolySmith stays a file producer for the G-code itself (the CAD core
//! exports `.nc`); this module is the shell-side transport: it opens a
//! serial port, streams a file line-by-line with GRBL's ok/error
//! handshake inside a byte-sized send window (GRBL's RX buffer is 128
//! bytes), polls `?` status, and forwards jog/home/pause commands.
//!
//! A single worker thread owns the port for its whole lifetime;
//! commands travel to it over an mpsc channel, state travels back to
//! the UI as `grbl-stream` Tauri events.  The parser and the send
//! window are pure functions so they can be unit-tested when Rust
//! test infrastructure lands (none exists yet — `cargo check` + the
//! manual app checklist are today's gates).

use serde::Serialize;
use serialport::SerialPort;
use std::collections::VecDeque;
use std::io::{Read, Write};
use std::sync::mpsc::{self, Receiver, Sender, TryRecvError};
use std::sync::Mutex;
use std::time::{Duration, Instant};
use tauri::{AppHandle, Emitter, State};

/// GRBL's RX buffer is 128 bytes; the sender admits a line only while
/// unacked bytes + the line (plus its newline) fit inside 127.
pub struct ByteWindow {
    unacked: usize,
}

impl ByteWindow {
    pub fn new() -> Self {
        Self { unacked: 0 }
    }

    pub fn admit(&self, line_len_with_newline: usize) -> bool {
        self.unacked + line_len_with_newline <= 127
    }

    pub fn add(&mut self, bytes: usize) {
        self.unacked += bytes;
    }

    pub fn ack(&mut self, bytes: usize) {
        self.unacked = self.unacked.saturating_sub(bytes);
    }
}

/// Machine state as reported by GRBL's `?` status report.
#[derive(Debug, Clone, Copy, PartialEq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum GrblMachineState {
    Idle,
    Run,
    Hold,
    Door,
    Home,
    Sleep,
    Alarm,
    Check,
    Jog,
}

/// One parsed GRBL response line.
#[derive(Debug, Clone)]
pub enum GrblResponse {
    Ok,
    Error { code: u32 },
    Status { state: GrblMachineState, mpos: Option<[f64; 3]> },
    Other,
}

fn grbl_error_message(code: u32) -> &'static str {
    match code {
        1 => "G-code words consist of a letter and a value",
        2 => "Numeric value format is not valid or missing an expected value",
        3 => "GRBL '$' system command was not recognized or supported",
        4 => "Negative value received for an expected positive value",
        5 => "Homing cycle is not enabled via settings",
        6 => "Minimum step pulse time must be greater than 3usec",
        7 => "EEPROM read failed",
        8 => "GRBL '$' command only valid when idle",
        9 => "G-code locked out during alarm or jog state",
        10 => "Soft limits cannot be enabled without homing also enabled",
        11 => "Max characters per line exceeded",
        12 => "Compile option is enabled but line is too long",
        13 => "Feed rate has not yet been set or is undefined",
        14 => "G-code locked out during alarm or jog state",
        15 => "Jog target exceeds machine travel",
        16 => "Jog command with no '=' or contains prohibited g-code",
        17 => "Laser mode requires PWM output",
        18 => "Unsupported or invalid g-code command",
        19 => "Too many G-code axes for plane",
        20 => "Unsupported or invalid g-code command",
        21 => "Unsupported or invalid g-code command",
        22 => "Feed rate has not yet been set or is undefined",
        23 => "G-code command requires an integer value",
        24 => "More than one g-code command from same modal group",
        _ => "Unknown GRBL error",
    }
}

/// Parses one response line.  Pure — no port access.
pub fn parse_grbl_response(line: &str) -> GrblResponse {
    let trimmed = line.trim();
    if trimmed == "ok" {
        return GrblResponse::Ok;
    }
    if let Some(rest) = trimmed.strip_prefix("error:") {
        let code = rest.trim().parse::<u32>().unwrap_or(0);
        return GrblResponse::Error { code };
    }
    if trimmed.starts_with('<') && trimmed.ends_with('>') {
        let inner = &trimmed[1..trimmed.len() - 1];
        let mut parts = inner.split('|');
        let state = match parts.next().unwrap_or("") {
            "Idle" => GrblMachineState::Idle,
            "Run" => GrblMachineState::Run,
            "Hold:0" | "Hold:1" => GrblMachineState::Hold,
            "Door:0" | "Door:1" | "Door:2" | "Door:3" => GrblMachineState::Door,
            "Home" => GrblMachineState::Home,
            "Sleep" => GrblMachineState::Sleep,
            "Alarm" => GrblMachineState::Alarm,
            "Check" => GrblMachineState::Check,
            "Jog" => GrblMachineState::Jog,
            _ => GrblMachineState::Idle,
        };
        let mut mpos = None;
        for part in parts {
            if let Some(rest) = part.strip_prefix("MPos:") {
                let coords: Vec<f64> =
                    rest.split(',').filter_map(|v| v.parse().ok()).collect();
                if coords.len() >= 3 {
                    mpos = Some([coords[0], coords[1], coords[2]]);
                }
            }
        }
        return GrblResponse::Status { state, mpos };
    }
    GrblResponse::Other
}

/// UI-facing event payload (`grbl-stream`).
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GrblStreamEvent {
    pub kind: String, // connected|disconnected|progress|status|error|completed|paused|resumed|reset
    pub message: String,
    pub port_name: Option<String>,
    pub baud_rate: Option<u32>,
    pub lines_sent: Option<usize>,
    pub lines_total: Option<usize>,
    pub percent: Option<f64>,
    pub state: Option<GrblMachineState>,
    pub mpos: Option<[f64; 3]>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GrblPortInfo {
    pub name: String,
    pub port_type: String,
}

enum WorkerMsg {
    Connect { port: String, baud_rate: u32 },
    Disconnect,
    SendFile { file_path: String },
    Pause,
    Resume,
    Reset,
    Home,
    Unlock,
    Jog { x: Option<f64>, y: Option<f64>, feed: f64 },
}

pub struct GrblState {
    sender: Mutex<Option<Sender<WorkerMsg>>>,
}

impl Default for GrblState {
    fn default() -> Self {
        Self {
            sender: Mutex::new(None),
        }
    }
}

impl GrblState {
    fn send(&self, message: WorkerMsg) -> Result<(), String> {
        let guard = self.sender.lock().map_err(|error| error.to_string())?;
        guard
            .as_ref()
            .ok_or_else(|| "GRBL worker is not running".to_string())?
            .send(message)
            .map_err(|error| error.to_string())
    }
}

struct Worker {
    rx: Receiver<WorkerMsg>,
    app: AppHandle,
    port: Option<Box<dyn SerialPort>>,
    port_name: Option<String>,
    baud_rate: Option<u32>,
    window: ByteWindow,
    // Job state: the queue holds (line, byte_length_with_newline) in
    // send order; sent_lengths pairs each unacknowledged line with its
    // byte count so an ok can shrink the window exactly.
    pending: VecDeque<(String, usize)>,
    sent_lengths: VecDeque<usize>,
    lines_sent: usize,
    lines_total: usize,
    completed_emitted: bool,
    paused: bool,
    last_status_emit: Instant,
    last_poll: Instant,
    rx_buffer: Vec<u8>,
}

fn event(kind: &str, message: &str) -> GrblStreamEvent {
    GrblStreamEvent {
        kind: kind.to_string(),
        message: message.to_string(),
        port_name: None,
        baud_rate: None,
        lines_sent: None,
        lines_total: None,
        percent: None,
        state: None,
        mpos: None,
    }
}

fn grbl_error_text(code: u32) -> String {
    format!("GRBL error {}: {}", code, grbl_error_message(code))
}

impl Worker {
    fn emit(&self, mut payload: GrblStreamEvent) {
        payload.port_name = self.port_name.clone();
        payload.baud_rate = self.baud_rate;
        let _ = self.app.emit("grbl-stream", payload);
    }

    fn close_port(&mut self) {
        self.port = None;
        self.port_name = None;
        self.baud_rate = None;
        self.pending.clear();
        self.sent_lengths.clear();
        self.window = ByteWindow::new();
        self.lines_sent = 0;
        self.lines_total = 0;
        self.completed_emitted = false;
        self.paused = false;
    }

    fn connect(&mut self, port: String, baud_rate: u32) {
        self.close_port();
        let opened = serialport::new(&port, baud_rate)
            .timeout(Duration::from_millis(50))
            .open();
        match opened {
            Ok(serial) => {
                self.port = Some(serial);
                self.port_name = Some(port.clone());
                self.baud_rate = Some(baud_rate);
                let mut payload = event("connected", "Connected to GRBL");
                payload.port_name = Some(port);
                payload.baud_rate = Some(baud_rate);
                self.emit(payload);
            }
            Err(error) => {
                self.emit(event(
                    "error",
                    &format!("Could not open {port}: {error}"),
                ));
            }
        }
    }

    fn write_line(&mut self, line: &str) {
        if let Some(serial) = self.port.as_mut() {
            let bytes = format!("{line}\n");
            let _ = serial.write_all(bytes.as_bytes());
        }
    }

    fn start_job(&mut self, file_path: &str) {
        if self.port.is_none() {
            self.emit(event("error", "Not connected — connect a port first"));
            return;
        }
        let text = match std::fs::read_to_string(file_path) {
            Ok(text) => text,
            Err(error) => {
                self.emit(event("error", &format!("Could not read {file_path}: {error}")));
                return;
            }
        };
        // GRBL-safe preprocessing: strip full-line comments and blanks.
        // Parenthesized comments are PolySmith's own post headers —
        // they only cost serial time, never correctness.
        let lines: Vec<String> = text
            .lines()
            .map(str::trim)
            .filter(|line| !line.is_empty() && !line.starts_with('('))
            .map(str::to_string)
            .collect();
        if lines.is_empty() {
            self.emit(event("error", "The file contains no G-code lines"));
            return;
        }
        // A line that can never fit the send window would stall the
        // job at a fixed percent forever — reject the file up front.
        if let Some(long_line) = lines.iter().find(|line| line.len() + 1 > 127) {
            self.emit(event(
                "error",
                &format!(
                    "Line too long for GRBL ({} bytes, limit 127): {long_line}",
                    long_line.len() + 1
                ),
            ));
            return;
        }
        self.pending.clear();
        self.sent_lengths.clear();
        self.window = ByteWindow::new();
        self.lines_sent = 0;
        self.lines_total = lines.len();
        self.completed_emitted = false;
        for line in lines {
            self.pending.push_back((line.clone(), line.len() + 1));
        }
        self.pump();
    }

    /// Sends as many pending lines as the byte window admits.
    fn pump(&mut self) {
        if self.port.is_none() || self.paused {
            return;
        }
        while let Some((line, len)) = self.pending.front().cloned() {
            if !self.window.admit(len) {
                break;
            }
            self.write_line(&line);
            self.pending.pop_front();
            self.sent_lengths.push_back(len);
            self.window.add(len);
            self.lines_sent += 1;
        }
        self.emit_progress();
    }

    /// Progress (and, once, completion) for the UI.
    fn emit_progress(&mut self) {
        if self.lines_total == 0 {
            return;
        }
        let finished = self.pending.is_empty() && self.sent_lengths.is_empty();
        let percent = self.lines_sent as f64 / self.lines_total as f64 * 100.0;
        if finished && !self.completed_emitted {
            self.completed_emitted = true;
            let mut payload = event("completed", "Job complete");
            payload.lines_sent = Some(self.lines_sent);
            payload.lines_total = Some(self.lines_total);
            payload.percent = Some(100.0);
            self.emit(payload);
            return;
        }
        if finished {
            return;
        }
        let mut payload = event("progress", "Streaming");
        payload.lines_sent = Some(self.lines_sent);
        payload.lines_total = Some(self.lines_total);
        payload.percent = Some(percent);
        self.emit(payload);
    }

    fn handle_response(&mut self, response: GrblResponse) {
        match response {
            GrblResponse::Ok => {
                if let Some(len) = self.sent_lengths.pop_front() {
                    self.window.ack(len);
                }
                // pump() emits progress, and completion when the last
                // ok drains the queue.
                self.pump();
            }
            GrblResponse::Error { code, .. } => {
                // Abort the job — GRBL already stopped executing.
                self.pending.clear();
                self.sent_lengths.clear();
                self.window = ByteWindow::new();
                self.lines_total = 0;
                self.emit(event("error", &grbl_error_text(code)));
            }
            GrblResponse::Status { state, mpos } => {
                // Throttle status events to 5 Hz.
                if self.last_status_emit.elapsed() >= Duration::from_millis(200) {
                    let mut payload = event("status", "");
                    payload.state = Some(state);
                    payload.mpos = mpos;
                    self.emit(payload);
                    self.last_status_emit = Instant::now();
                }
            }
            GrblResponse::Other => {}
        }
    }

    fn handle_message(&mut self, message: WorkerMsg) {
        match message {
            WorkerMsg::Connect { port, baud_rate } => self.connect(port, baud_rate),
            WorkerMsg::Disconnect => {
                if self.port.is_some() {
                    // GRBL keeps executing its buffered lines without
                    // the host — abort them before dropping the port
                    // so "Disconnect" never leaves the laser running.
                    if self.lines_total > 0 {
                        self.write_line("\u{18}");
                    }
                    self.close_port();
                    self.emit(event("disconnected", "Disconnected"));
                }
            }
            WorkerMsg::SendFile { file_path } => self.start_job(&file_path),
            WorkerMsg::Pause => {
                self.paused = true;
                self.write_line("!");
                self.emit(event("paused", "Feed hold"));
            }
            WorkerMsg::Resume => {
                self.paused = false;
                self.write_line("~");
                self.emit(event("resumed", "Cycle start"));
                self.pump();
            }
            WorkerMsg::Reset => {
                self.write_line("\u{18}");
                self.pending.clear();
                self.sent_lengths.clear();
                self.window = ByteWindow::new();
                self.lines_total = 0;
                // Not an error event — a user-initiated abort; the UI
                // clears job progress without a toast.
                self.emit(event("reset", "Reset sent to GRBL"));
            }
            WorkerMsg::Home => {
                self.write_line("$H");
            }
            WorkerMsg::Unlock => {
                self.write_line("$X");
            }
            WorkerMsg::Jog { x, y, feed } => {
                let mut jog = String::from("$J=G91");
                if let Some(value) = x {
                    jog.push_str(&format!(" X{value:.3}"));
                }
                if let Some(value) = y {
                    jog.push_str(&format!(" Y{value:.3}"));
                }
                jog.push_str(&format!(" F{feed:.0}"));
                self.write_line(&jog);
            }
        }
    }

    fn run(mut self) {
        loop {
            // Drain available serial bytes into response lines.  The
            // port borrow ends before responses are handled, so the
            // handler can freely mutate the job state.
            if let Some(serial) = self.port.as_mut() {
                let mut responses: Vec<GrblResponse> = Vec::new();
                loop {
                    let mut chunk = [0u8; 256];
                    match serial.read(&mut chunk) {
                        Ok(0) => break,
                        Ok(count) => {
                            self.rx_buffer.extend_from_slice(&chunk[..count]);
                            while let Some(pos) =
                                self.rx_buffer.iter().position(|byte| *byte == b'\n')
                            {
                                let line_bytes: Vec<u8> =
                                    self.rx_buffer.drain(..=pos).collect();
                                let line = String::from_utf8_lossy(
                                    &line_bytes[..line_bytes.len() - 1],
                                );
                                responses.push(parse_grbl_response(&line));
                            }
                        }
                        Err(error) if error.kind() == std::io::ErrorKind::TimedOut => break,
                        Err(_) => break,
                    }
                }
                // Status poll every 500 ms while connected.
                if self.last_poll.elapsed() >= Duration::from_millis(500) {
                    let _ = serial.write_all(b"?\n");
                    self.last_poll = Instant::now();
                }
                for response in responses {
                    self.handle_response(response);
                }
            }

            match self.rx.try_recv() {
                Ok(message) => self.handle_message(message),
                Err(TryRecvError::Empty) => {}
                Err(TryRecvError::Disconnected) => return,
            }
            std::thread::sleep(Duration::from_millis(10));
        }
    }
}

fn start_worker(app: AppHandle, state: &GrblState) -> Result<(), String> {
    let mut guard = state.sender.lock().map_err(|error| error.to_string())?;
    if guard.is_some() {
        return Ok(());
    }
    let (tx, rx) = mpsc::channel();
    let worker = Worker {
        rx,
        app,
        port: None,
        port_name: None,
        baud_rate: None,
        window: ByteWindow::new(),
        pending: VecDeque::new(),
        sent_lengths: VecDeque::new(),
        lines_sent: 0,
        lines_total: 0,
        completed_emitted: false,
        paused: false,
        last_status_emit: Instant::now(),
        last_poll: Instant::now(),
        rx_buffer: Vec::new(),
    };
    std::thread::spawn(move || worker.run());
    *guard = Some(tx);
    Ok(())
}

// ── Tauri commands ─────────────────────────────────────────────────

#[tauri::command]
pub fn grbl_list_ports() -> Result<Vec<GrblPortInfo>, String> {
    let ports = serialport::available_ports().map_err(|error| error.to_string())?;
    Ok(ports
        .into_iter()
        .map(|port| GrblPortInfo {
            name: port.port_name.clone(),
            port_type: format!("{:?}", port.port_type),
        })
        .collect())
}

#[tauri::command]
pub fn grbl_connect(
    app: AppHandle,
    state: State<'_, GrblState>,
    port: String,
    baud_rate: u32,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Connect { port, baud_rate })
}

#[tauri::command]
pub fn grbl_disconnect(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Disconnect)
}

#[tauri::command]
pub fn grbl_send_file(
    app: AppHandle,
    state: State<'_, GrblState>,
    file_path: String,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::SendFile { file_path })
}

#[tauri::command]
pub fn grbl_pause(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Pause)
}

#[tauri::command]
pub fn grbl_resume(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Resume)
}

#[tauri::command]
pub fn grbl_reset(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Reset)
}

#[tauri::command]
pub fn grbl_home(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Home)
}

#[tauri::command]
pub fn grbl_unlock(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Unlock)
}

#[tauri::command]
pub fn grbl_jog(
    app: AppHandle,
    state: State<'_, GrblState>,
    x: Option<f64>,
    y: Option<f64>,
    feed: f64,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::Jog { x, y, feed })
}
