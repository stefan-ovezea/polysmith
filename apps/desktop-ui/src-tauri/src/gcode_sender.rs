//! Direct GRBL streaming over serial or TCP.
//!
//! This module is the shell-side transport: it opens a serial port or
//! a TCP connection (FluidNC's text port 23), streams a program
//! line-by-line with GRBL's ok/error handshake inside a byte-sized
//! send window (GRBL's RX buffer is 128 bytes), polls `?` status, and
//! forwards jog/home/pause commands.  Programs arrive either as a
//! file (grbl_send_file) or as in-memory text from the CAM→GRBL
//! handoff (grbl_send_program) — both run the same pipeline.
//!
//! A single worker thread owns the port for its whole lifetime;
//! commands travel to it over an mpsc channel, state travels back to
//! the UI as `grbl-stream` Tauri events.  The preprocessing and the
//! send window are pure functions, unit-tested in the `tests` module.

use serde::Serialize;
use serialport::SerialPort;
use std::collections::VecDeque;
use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::sync::mpsc::{self, Receiver, Sender, TryRecvError};
use std::sync::Mutex;
use std::time::{Duration, Instant};
use tauri::{AppHandle, Emitter, State};
use tungstenite::{
    client::IntoClientRequest, handshake::client::ClientHandshake, Message, WebSocket,
};

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
    /// GRBL/FluidNC `ALARM:n` line — motion is locked and buffered
    /// lines will never execute.
    Alarm { code: u32 },
    Status { state: GrblMachineState, mpos: Option<[f64; 3]>, wpos: Option<[f64; 3]> },
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
    // FluidNC soft-limit / homing alarms arrive as `ALARM:n` lines;
    // GRBL 1.1 emits them too.
    if let Some(rest) = trimmed.strip_prefix("ALARM:") {
        let code = rest.trim().parse::<u32>().unwrap_or(0);
        return GrblResponse::Alarm { code };
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
        let mut wpos = None;
        let mut wco = None;
        for part in parts {
            if let Some(rest) = part.strip_prefix("MPos:") {
                let coords: Vec<f64> =
                    rest.split(',').filter_map(|v| v.parse().ok()).collect();
                if coords.len() >= 3 {
                    mpos = Some([coords[0], coords[1], coords[2]]);
                }
            }
            // WPos = MPos minus the G92/G54 offset — the job
            // coordinates the .nc uses. After "Zero XY" (G92) MPos
            // keeps the machine position while WPos drops to 0,0.
            if let Some(rest) = part.strip_prefix("WPos:") {
                let coords: Vec<f64> =
                    rest.split(',').filter_map(|v| v.parse().ok()).collect();
                if coords.len() >= 3 {
                    wpos = Some([coords[0], coords[1], coords[2]]);
                }
            }
            // FluidNC reports the work coordinate OFFSET instead of
            // the resulting work position (GRBL's WPos field) — derive
            // WPos = MPos - WCO so Zero XY reads 0,0 on both firmwares.
            if let Some(rest) = part.strip_prefix("WCO:") {
                let coords: Vec<f64> =
                    rest.split(',').filter_map(|v| v.parse().ok()).collect();
                if coords.len() >= 3 {
                    wco = Some([coords[0], coords[1], coords[2]]);
                }
            }
        }
        if wpos.is_none() {
            if let (Some(m), Some(offset)) = (mpos, wco) {
                wpos = Some([
                    m[0] - offset[0],
                    m[1] - offset[1],
                    m[2] - offset[2],
                ]);
            }
        }
        return GrblResponse::Status { state, mpos, wpos };
    }
    GrblResponse::Other
}

/// One GRBL $ setting (`key` keeps the dollar prefix, e.g. "$20").
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GrblSetting {
    pub key: String,
    pub value: String,
}

/// UI-facing event payload (`grbl-stream`).
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GrblStreamEvent {
    pub kind: String, // connected|disconnected|progress|status|error|completed|paused|resumed|reset|settings
    pub message: String,
    pub port_name: Option<String>,
    pub baud_rate: Option<u32>,
    pub lines_sent: Option<usize>,
    pub lines_total: Option<usize>,
    pub percent: Option<f64>,
    pub state: Option<GrblMachineState>,
    pub mpos: Option<[f64; 3]>,
    pub wpos: Option<[f64; 3]>,
    pub settings: Option<Vec<GrblSetting>>,
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
    SendProgram { text: String },
    Pause,
    Resume,
    Reset,
    Home,
    Unlock,
    Jog { x: Option<f64>, y: Option<f64>, feed: f64 },
    ZeroXy,
    Raw { line: String },
    ConnectTcp { host: String, port: u16 },
    ConnectWs { host: String, port: u16 },
    LaserPower { percent: Option<f64> },
    WriteByte { byte: u8 },
    GetSettings,
}

/// The open transport to the machine: a USB serial port, FluidNC's
/// TCP text port (23), or FluidNC's WebSocket port (81).  All three
/// carry the identical line protocol (ok/error/status), so the worker
/// treats them the same; only the read/write plumbing differs.
enum GrblLink {
    Serial(Box<dyn SerialPort>),
    Tcp(TcpStream),
    Ws(WsLink),
}

/// WebSocket framing adapter.  tungstenite 0.26 deliberately dropped
/// the `std::io::Read`/`Write` impls from `WebSocket` (message
/// semantics never mapped cleanly onto byte streams), so this keeps
/// the worker's chunked read/write contract and translates it to one
/// message per write and per message on read.  FluidNC's WS endpoint
/// emits exactly one text message per response line (plus broadcast
/// WebUI chatter, which the line parser ignores).
struct WsLink {
    socket: WebSocket<TcpStream>,
    // Bytes of the current message not yet handed to the reader.
    pending: Vec<u8>,
    pos: usize,
}

fn ws_error(error: tungstenite::Error) -> std::io::Error {
    match error {
        tungstenite::Error::Io(io) => io,
        other => std::io::Error::new(std::io::ErrorKind::Other, other.to_string()),
    }
}

impl WsLink {
    fn new(socket: WebSocket<TcpStream>) -> Self {
        Self {
            socket,
            pending: Vec::new(),
            pos: 0,
        }
    }

    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        loop {
            if self.pos < self.pending.len() {
                let count = (self.pending.len() - self.pos).min(buffer.len());
                buffer[..count]
                    .copy_from_slice(&self.pending[self.pos..self.pos + count]);
                self.pos += count;
                if self.pos == self.pending.len() {
                    self.pending.clear();
                    self.pos = 0;
                }
                return Ok(count);
            }
            self.pending.clear();
            self.pos = 0;
            match self.socket.read() {
                Ok(Message::Text(text)) => {
                    self.pending = text.as_str().as_bytes().to_vec();
                }
                Ok(Message::Binary(bytes)) => self.pending = bytes.to_vec(),
                Ok(Message::Ping(_) | Message::Pong(_) | Message::Frame(_)) => continue,
                // Close frame = end of stream, same as EOF on the
                // serial/TCP links (the drain loop just stops).
                Ok(Message::Close(_)) => return Ok(0),
                Err(error) => return Err(ws_error(error)),
            }
        }
    }

    fn write_all(&mut self, buffer: &[u8]) -> std::io::Result<()> {
        // Every caller write is either a newline-terminated line or a
        // single real-time byte.  Text when it is valid UTF-8 (all
        // protocol traffic), Binary otherwise; FluidNC treats both
        // identically over WS.
        let message = match std::str::from_utf8(buffer) {
            Ok(text) => Message::Text(text.to_string().into()),
            Err(_) => Message::Binary(buffer.to_vec().into()),
        };
        self.socket.send(message).map_err(ws_error)
    }
}

impl GrblLink {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        match self {
            GrblLink::Serial(serial) => serial.read(buffer),
            GrblLink::Tcp(stream) => stream.read(buffer),
            GrblLink::Ws(link) => link.read(buffer),
        }
    }

    fn write_all(&mut self, buffer: &[u8]) -> std::io::Result<()> {
        match self {
            GrblLink::Serial(serial) => serial.write_all(buffer),
            GrblLink::Tcp(stream) => stream.write_all(buffer),
            GrblLink::Ws(link) => link.write_all(buffer),
        }
    }
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
    port: Option<GrblLink>,
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
    // Latest machine position from the status stream — Zero XY derives
    // the new WCO from it locally (see the ZeroXy handler).
    last_mpos: Option<[f64; 3]>,
    // In-flight `$$` dump collection (see GetSettings).
    collecting: Option<SettingsCollect>,
}

/// Collection state for a `$$` settings dump: the controller prints
/// one `$N=value` line per setting, terminated by `ok` on GRBL 1.1.
/// FluidNC may print the same lines without the trailing ok — the
/// deadline fallback emits whatever arrived.
struct SettingsCollect {
    entries: Vec<GrblSetting>,
    deadline: Instant,
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
        wpos: None,
        settings: None,
    }
}

/// Parses one `$N=value` settings line.  None for everything else
/// (status reports, comments, ok/error replies).
fn parse_settings_line(line: &str) -> Option<(String, String)> {
    let line = line.trim();
    let eq = line.find('=')?;
    let key = line[..eq].trim();
    if !key.starts_with('$') {
        return None;
    }
    let digits = &key[1..];
    if digits.is_empty() || !digits.chars().all(|c| c.is_ascii_digit()) {
        return None;
    }
    Some((key.to_string(), line[eq + 1..].trim().to_string()))
}

fn grbl_error_text(code: u32) -> String {
    format!("GRBL error {}: {}", code, grbl_error_message(code))
}

/// Human-readable GRBL alarm descriptions (GRBL 1.1 `ALARM:n`).
fn grbl_alarm_message(code: u32) -> &'static str {
    match code {
        1 => "hard limit triggered — machine position may be lost, re-home",
        2 => "soft limit — the job moved outside the configured work area",
        3 => "abort during cycle",
        4 => "probe failed — no contact before the target",
        5 => "probe failed — initial probe state wrong",
        6 => "homing failed — reset was issued during homing",
        7 => "homing failed — safety door opened during homing",
        8 => "homing failed — pull-off failed to clear the limit switch",
        9 => "homing failed — limit switch not found",
        _ => "unknown alarm",
    }
}

/// GRBL-safe preprocessing shared by file and in-memory jobs: the
/// parser's canonical filter (blank lines and `(`-prefixed full-line
/// comments stripped — the same rule `parse_gcode` uses, keeping
/// `linesSent` aligned with the preview's `move.line`), plus the
/// send-window guard.  Pure — unit-tested below.
fn prepare_job_lines(text: &str) -> Result<Vec<String>, String> {
    let lines = crate::gcode_parser::filter_lines(text);
    if lines.is_empty() {
        return Err("The program contains no G-code lines".to_string());
    }
    // A line that can never fit the send window would stall the job
    // at a fixed percent forever — reject the program up front.
    if let Some(long_line) = lines.iter().find(|line| line.len() + 1 > 127) {
        return Err(format!(
            "Line too long for GRBL ({} bytes, limit 127): {long_line}",
            long_line.len() + 1
        ));
    }
    Ok(lines)
}

impl Worker {
    fn emit(&self, mut payload: GrblStreamEvent) {
        payload.port_name = self.port_name.clone();
        payload.baud_rate = self.baud_rate;
        let _ = self.app.emit("grbl-stream", payload);
    }

    fn emit_settings(&self, entries: Vec<GrblSetting>) {
        let mut payload = event("settings", &format!("{} settings", entries.len()));
        payload.settings = Some(entries);
        self.emit(payload);
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
                self.port = Some(GrblLink::Serial(serial));
                self.port_name = Some(port.clone());
                self.baud_rate = Some(baud_rate);
                self.emit(event("connected", "Connected to GRBL"));
            }
            Err(error) => {
                self.emit(event(
                    "error",
                    &format!("Could not open {port}: {error}"),
                ));
            }
        }
    }

    /// FluidNC's telnet port (23) carries the same line protocol as
    /// USB serial — connect over the network instead of a COM port.
    /// Resolves first so the connection itself can carry a timeout
    /// (a bare connect to an unreachable host can hang for minutes).
    fn connect_tcp(&mut self, host: String, port: u16) {
        self.close_port();
        let opened = (|| -> std::io::Result<TcpStream> {
            let mut addresses = (host.as_str(), port).to_socket_addrs()?;
            let address = addresses.next().ok_or_else(|| {
                std::io::Error::new(
                    std::io::ErrorKind::AddrNotAvailable,
                    "no address resolved",
                )
            })?;
            let stream = TcpStream::connect_timeout(&address, Duration::from_secs(5))?;
            // FluidNC's port is a raw text stream; nodelay keeps
            // status/ok round-trips immediate.
            stream.set_nodelay(true)?;
            // Match the serial port's 50 ms read timeout so the
            // worker loop stays responsive.
            stream.set_read_timeout(Some(Duration::from_millis(50)))?;
            Ok(stream)
        })();
        match opened {
            Ok(stream) => {
                self.port = Some(GrblLink::Tcp(stream));
                self.port_name = Some(format!("{host}:{port}"));
                self.baud_rate = None;
                self.emit(event("connected", "Connected to GRBL"));
            }
            Err(error) => {
                self.emit(event(
                    "error",
                    &format!("Could not connect to {host}:{port}: {error}"),
                ));
            }
        }
    }

    /// FluidNC's WebSocket endpoint: the same text protocol over
    /// `ws://host:port/`.  Verified against a real FluidNC v4.0.x
    /// board: the raw GRBL channel shares the HTTP/WebUI port (80)
    /// and the upgrade path is `/` — `/ws` and port 81 (the
    /// HTTP-port-plus-one convention of newer configs) are both
    /// rejected when not configured.  WebUI broadcasts (`currentID`
    /// lines) interleave with the GRBL replies; the parser ignores
    /// unknown lines.  The HTTP upgrade gets a 5 s window, then the
    /// read timeout returns to the worker's 50 ms cadence.
    fn connect_ws(&mut self, host: String, port: u16) {
        self.close_port();
        let opened = (|| -> Result<WsLink, String> {
            let mut addresses = (host.as_str(), port)
                .to_socket_addrs()
                .map_err(|error| error.to_string())?;
            let address = addresses
                .next()
                .ok_or_else(|| "no address resolved".to_string())?;
            let stream = TcpStream::connect_timeout(&address, Duration::from_secs(5))
                .map_err(|error| error.to_string())?;
            stream
                .set_nodelay(true)
                .map_err(|error| error.to_string())?;
            stream
                .set_read_timeout(Some(Duration::from_secs(5)))
                .map_err(|error| error.to_string())?;
            let url = format!("ws://{host}:{port}/");
            let request = url
                .into_client_request()
                .map_err(|error| error.to_string())?;
            // ClientHandshake::start + handshake() is the 0.26 spelling
            // of the old one-call `client(url, stream)` helper.
            let (socket, _response) = ClientHandshake::start(stream, request, None)
                .map_err(|error| error.to_string())?
                .handshake()
                .map_err(|error| error.to_string())?;
            socket
                .get_ref()
                .set_read_timeout(Some(Duration::from_millis(50)))
                .map_err(|error| error.to_string())?;
            Ok(WsLink::new(socket))
        })();
        match opened {
            Ok(link) => {
                self.port = Some(GrblLink::Ws(link));
                self.port_name = Some(format!("{host}:{port} (ws)"));
                self.baud_rate = None;
                self.emit(event("connected", "Connected to GRBL"));
            }
            Err(error) => {
                self.emit(event(
                    "error",
                    &format!("Could not connect to {host}:{port}: {error}"),
                ));
            }
        }
    }

    fn write_line(&mut self, line: &str) {
        if let Some(link) = self.port.as_mut() {
            let bytes = format!("{line}\n");
            let _ = link.write_all(bytes.as_bytes());
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
        match prepare_job_lines(&text) {
            Ok(lines) => self.begin_job(lines),
            Err(message) => self.emit(event("error", &message)),
        }
    }

    /// In-memory program entry point (the CAM→GRBL handoff): the same
    /// pipeline as start_job, no file on disk.
    fn start_program(&mut self, text: &str) {
        if self.port.is_none() {
            self.emit(event("error", "Not connected — connect a port first"));
            return;
        }
        match prepare_job_lines(text) {
            Ok(lines) => self.begin_job(lines),
            Err(message) => self.emit(event("error", &message)),
        }
    }

    /// Queue a prepared, validated line list and start streaming.
    fn begin_job(&mut self, lines: Vec<String>) {
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
                if let Some(collect) = self.collecting.take() {
                    // GRBL terminates the `$$` dump with ok — emit
                    // the collected settings right away.
                    self.emit_settings(collect.entries);
                }
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
            GrblResponse::Alarm { code } => {
                // Motion is locked — the buffered tail will never
                // execute.  Abort instead of letting the queue drain
                // into a fake "completed".
                self.pending.clear();
                self.sent_lengths.clear();
                self.window = ByteWindow::new();
                self.lines_total = 0;
                self.emit(event(
                    "error",
                    &format!(
                        "Machine alarm {code} ({}) — motion is locked. Press Reset, then Unlock ($X).",
                        grbl_alarm_message(code)
                    ),
                ));
            }
            GrblResponse::Status { state, mpos, wpos } => {
                // Track the position even between throttled emits so
                // Zero XY can derive the new WCO at press time.
                self.last_mpos = mpos;
                // Throttle status events to 5 Hz.
                if self.last_status_emit.elapsed() >= Duration::from_millis(200) {
                    let mut payload = event("status", "");
                    payload.state = Some(state);
                    payload.mpos = mpos;
                    payload.wpos = wpos;
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
            WorkerMsg::ConnectTcp { host, port } => self.connect_tcp(host, port),
            WorkerMsg::ConnectWs { host, port } => self.connect_ws(host, port),
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
            WorkerMsg::SendProgram { text } => self.start_program(&text),
            WorkerMsg::LaserPower { percent } => match percent {
                Some(percent) => {
                    let scaled = crate::grbl_utilities::scale_power(percent);
                    self.write_line(&format!("M3 S{scaled}"));
                }
                None => self.write_line("M5"),
            },
            WorkerMsg::WriteByte { byte } => {
                // Real-time override commands are bare bytes — NO
                // newline, and they bypass the RX window (the
                // controller processes them out-of-band).  The
                // allowlist lives in the command.
                if let Some(link) = self.port.as_mut() {
                    if let Err(error) = link.write_all(&[byte]) {
                        self.emit(event(
                            "error",
                            &format!("Could not send real-time command: {error}"),
                        ));
                    }
                }
            }
            WorkerMsg::GetSettings => {
                // Ask for a `$$` dump; the run loop collects $k=v
                // lines until ok (GRBL) or the deadline (FluidNC).
                self.collecting = Some(SettingsCollect {
                    entries: Vec::new(),
                    deadline: Instant::now() + Duration::from_millis(1500),
                });
                self.write_line("$$");
            }
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
            WorkerMsg::ZeroXy => {
                // Set the WCS origin to the current position — the
                // LightBurn-style "set origin": the job's (0,0) lands
                // where the head sits now.  G92 is the universally
                // supported form (GRBL 1.1 and FluidNC alike; G10 L20
                // is GRBL-only) and WPos drops to 0,0 immediately.
                // $X first makes the button self-sufficient: FluidNC
                // locks G-code out in alarm state, and $X is a no-op
                // when already unlocked (GRBL clears G92 offsets on
                // $X, so G92 is sent after).
                self.write_line("$X");
                self.write_line("G92 X0 Y0 Z0");
                // Update the UI locally instead of waiting for the
                // board: FluidNC can take seconds to publish a G92 in
                // its status reports, which made Zero XY feel like it
                // hung and the preview jump later out of nowhere.
                // G92 X0 Y0 Z0 ⇒ WPos = (0,0,0) at the current machine
                // position, so the new WCO equals the last known MPos.
                // The next real status report confirms (or corrects, if
                // the board rejected the G92) these values.
                let mut payload = event("zeroed", "WCS origin set to current position");
                payload.mpos = self.last_mpos;
                payload.wpos = Some([0.0, 0.0, 0.0]);
                self.emit(payload);
            }
            WorkerMsg::Raw { line } => {
                // Mini console — arbitrary $-settings / g-code sent
                // verbatim (e.g. "$20=0" to disable soft limits).
                self.write_line(&line);
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
            if let Some(link) = self.port.as_mut() {
                let mut responses: Vec<GrblResponse> = Vec::new();
                loop {
                    let mut chunk = [0u8; 256];
                    match link.read(&mut chunk) {
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
                                // Settings collection: capture $k=v
                                // lines verbatim while a `$$` dump is
                                // in flight (parse_grbl_response treats
                                // them as Other and discards the text).
                                if let Some(collect) = self.collecting.as_mut() {
                                    if let Some((key, value)) =
                                        parse_settings_line(&line)
                                    {
                                        collect
                                            .entries
                                            .push(GrblSetting { key, value });
                                    }
                                }
                                responses.push(parse_grbl_response(&line));
                            }
                        }
                        Err(error) if error.kind() == std::io::ErrorKind::TimedOut => break,
                        Err(_) => break,
                    }
                }
                // Status poll every 500 ms while connected.
                if self.last_poll.elapsed() >= Duration::from_millis(500) {
                    let _ = link.write_all(b"?\n");
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

            // FluidNC may print the `$$` lines without a trailing ok —
            // emit whatever arrived once the collection deadline
            // passes.
            let deadline_passed = match self.collecting.as_ref() {
                Some(collect) => Instant::now() >= collect.deadline,
                None => false,
            };
            if deadline_passed {
                if let Some(collect) = self.collecting.take() {
                    self.emit_settings(collect.entries);
                }
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
        last_mpos: None,
        collecting: None,
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
pub fn grbl_connect_tcp(
    app: AppHandle,
    state: State<'_, GrblState>,
    host: String,
    port: u16,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::ConnectTcp { host, port })
}

/// FluidNC's WebSocket endpoint — same line protocol as TCP 23, one
/// text message per line.  Verified on a real v4.0.x board: the raw
/// GRBL channel shares the HTTP/WebUI port (default 80), upgrade
/// path "/" (newer configs may use 81 = HTTP port + 1).
#[tauri::command]
pub fn grbl_connect_ws(
    app: AppHandle,
    state: State<'_, GrblState>,
    host: String,
    port: u16,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::ConnectWs { host, port })
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

/// Streams an in-memory program (the CAM→GRBL handoff) — same worker
/// pipeline as grbl_send_file, no file on disk.
#[tauri::command]
pub fn grbl_send_program(
    app: AppHandle,
    state: State<'_, GrblState>,
    text: String,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::SendProgram { text })
}

/// Laser test fire: `Some(percent)` sends `M3 S{scaled}` at the given
/// power, `None` sends `M5` (beam off) — hold-to-fire in the UI.
#[tauri::command]
pub fn grbl_laser_power(
    app: AppHandle,
    state: State<'_, GrblState>,
    percent: Option<f64>,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::LaserPower { percent })
}

/// Requests a `$$` settings dump — the result arrives as a
/// `grbl-stream` event of kind "settings".
#[tauri::command]
pub fn grbl_get_settings(
    app: AppHandle,
    state: State<'_, GrblState>,
) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::GetSettings)
}

/// Sends one GRBL real-time override byte (no newline).  Allowlisted
/// to the override set only — the UI never writes arbitrary bytes to
/// the controller.  Success is silent by design: real-time commands
/// get no ok/error reply, and FluidNC's override support is partial.
#[tauri::command]
pub fn grbl_write_byte(
    app: AppHandle,
    state: State<'_, GrblState>,
    byte: u8,
) -> Result<(), String> {
    const ALLOWED: [u8; 8] = [0x90, 0x91, 0x92, 0x93, 0x94, 0x99, 0x9A, 0x9B];
    if !ALLOWED.contains(&byte) {
        return Err(format!(
            "Byte 0x{byte:02X} is not an allowed override command"
        ));
    }
    start_worker(app, &state)?;
    state.send(WorkerMsg::WriteByte { byte })
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

#[tauri::command]
pub fn grbl_zero_xy(app: AppHandle, state: State<'_, GrblState>) -> Result<(), String> {
    start_worker(app, &state)?;
    state.send(WorkerMsg::ZeroXy)
}

#[tauri::command]
pub fn grbl_send_raw(
    app: AppHandle,
    state: State<'_, GrblState>,
    line: String,
) -> Result<(), String> {
    start_worker(app, &state)?;
    let trimmed = line.trim();
    if trimmed.is_empty() {
        return Ok(());
    }
    state.send(WorkerMsg::Raw {
        line: trimmed.to_string(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    // The sender's RX window is 127 bytes: a line fits only while
    // unacked bytes + line (+ newline) stay inside it.  A line that
    // can never fit would stall the job forever — rejected up front.
    #[test]
    fn prepare_rejects_blank_and_comment_only_programs() {
        assert!(prepare_job_lines("").is_err());
        assert!(prepare_job_lines("\n\n").is_err());
        assert!(prepare_job_lines("(header only)\n(another)").is_err());
    }

    #[test]
    fn prepare_strips_blanks_and_full_line_comments() {
        let lines = prepare_job_lines("G21\n\n(header)\nG90\n").expect("valid program");
        assert_eq!(lines, vec!["G21".to_string(), "G90".to_string()]);
    }

    #[test]
    fn prepare_rejects_lines_over_the_window() {
        // 126 content bytes + newline = 127: admitted.  127 content
        // bytes + newline = 128: never fits, rejected.
        let fits = "G1 X".to_string() + &"1".repeat(122); // 4 + 122 = 126
        let too_long = "G1 X".to_string() + &"1".repeat(123); // 127 content bytes
        assert_eq!(fits.len(), 126);
        assert_eq!(too_long.len(), 127);
        assert!(prepare_job_lines(&fits).is_ok());
        let error = prepare_job_lines(&too_long).expect_err("must reject");
        assert!(error.contains("127"), "message names the limit: {error}");
    }

    #[test]
    fn byte_window_admits_until_full() {
        let mut window = ByteWindow::new();
        assert!(window.admit(127));
        window.add(127);
        assert!(!window.admit(1), "no room past 127 unacked bytes");
        window.ack(100);
        assert!(window.admit(1), "ack frees room");
    }

    #[test]
    fn byte_window_ack_never_underflows() {
        let mut window = ByteWindow::new();
        window.add(10);
        window.ack(20);
        assert_eq!(window.unacked, 0);
    }

    // The settings collector accepts `$N=value` lines and rejects
    // everything else that streams past (status, comments, ok).
    #[test]
    fn settings_lines_parse_and_junk_is_rejected() {
        assert_eq!(
            parse_settings_line("$20=0"),
            Some(("$20".to_string(), "0".to_string()))
        );
        assert_eq!(
            parse_settings_line("  $130=200.000  "),
            Some(("$130".to_string(), "200.000".to_string()))
        );
        assert_eq!(
            parse_settings_line("$32=1\r"),
            Some(("$32".to_string(), "1".to_string()))
        );
        assert_eq!(parse_settings_line("ok"), None);
        assert_eq!(parse_settings_line("<Idle|MPos:0.000,0.000,0.000|FS:0,0>"), None);
        assert_eq!(parse_settings_line("(comment)"), None);
        assert_eq!(parse_settings_line("$H"), None);
        assert_eq!(parse_settings_line("$=1"), None);
        assert_eq!(parse_settings_line("G0 X10=2"), None);
    }

    // The alarm decoder turns `ALARM:n` into actionable text — the
    // table covers the full GRBL 1.1 range.
    #[test]
    fn alarm_messages_decode_known_and_unknown_codes() {
        assert_eq!(grbl_alarm_message(1), "hard limit triggered — machine position may be lost, re-home");
        assert_eq!(grbl_alarm_message(2), "soft limit — the job moved outside the configured work area");
        assert_eq!(grbl_alarm_message(3), "abort during cycle");
        assert_eq!(grbl_alarm_message(4), "probe failed — no contact before the target");
        assert_eq!(grbl_alarm_message(5), "probe failed — initial probe state wrong");
        assert_eq!(grbl_alarm_message(6), "homing failed — reset was issued during homing");
        assert_eq!(grbl_alarm_message(7), "homing failed — safety door opened during homing");
        assert_eq!(grbl_alarm_message(8), "homing failed — pull-off failed to clear the limit switch");
        assert_eq!(grbl_alarm_message(9), "homing failed — limit switch not found");
        assert_eq!(grbl_alarm_message(42), "unknown alarm");
    }
}
