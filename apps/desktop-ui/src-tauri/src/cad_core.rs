use std::io::{BufRead, BufReader, Write};
use std::path::PathBuf;
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Mutex,
};

use tauri::{path::BaseDirectory, AppHandle, Emitter, Manager};

use crate::protocol::{emit_core_error, emit_core_event, emit_core_log};

mod cad_core_build_config {
    include!(concat!(env!("OUT_DIR"), "/cad_core_build_config.rs"));
}

pub struct CadCoreState {
    pub child: Mutex<Option<CadCoreProcess>>,
}

pub struct CadCoreProcess {
    #[allow(dead_code)]
    pub child: Child,
    pub stdin: ChildStdin,
    pub exit_reported: Arc<AtomicBool>,
}

pub fn cad_core_path(app: &AppHandle) -> Result<PathBuf, String> {
    let path = match cad_core_build_config::CAD_CORE_PATH_KIND {
        "resource" => app
            .path()
            .resolve(
                cad_core_build_config::CAD_CORE_RESOURCE_PATH,
                BaseDirectory::Resource,
            )
            .map_err(|e| format!("failed to resolve bundled cad_core resource: {e}"))?,
        "workspace" => {
            let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
            path.push(cad_core_build_config::CAD_CORE_WORKSPACE_PATH);
            path
        }
        other => {
            return Err(format!(
                "unsupported cad_core path kind `{other}`; expected `workspace` or `resource`"
            ));
        }
    };

    #[cfg(target_os = "windows")]
    let path = {
        let mut path = path;
        path.set_extension("exe");
        path
    };

    if !path.exists() {
        return Err(format!("cad_core not found at {}", path.display()));
    }

    Ok(path)
}

pub fn start_cad_core_process(
    app: AppHandle,
    state: tauri::State<CadCoreState>,
) -> Result<String, String> {
    let mut guard = state.child.lock().map_err(|e| e.to_string())?;

    if let Some(process) = guard.as_mut() {
        match process.child.try_wait() {
            Ok(Some(status)) => {
                eprintln!("cad_core exited before restart with status: {status}");
                *guard = None;
            }
            Ok(None) => {
                return Ok("cad_core already running".to_string());
            }
            Err(error) => {
                eprintln!("failed to inspect cad_core process state: {error}");
                *guard = None;
            }
        }
    }

    let core_path = cad_core_path(&app)?;
    eprintln!("Starting cad_core from {}", core_path.display());

    let mut cmd = Command::new(&core_path);
    cmd.stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    // Prepend OCCT DLL directory to PATH so the child process finds TKernel.dll etc.
    #[cfg(target_os = "windows")]
    {
        let occt_bin = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../../../third_party/occt-install/win64/vc14/bin");
        if occt_bin.exists() {
            let separator = if cfg!(windows) { ";" } else { ":" };
            let existing_path = std::env::var("PATH").unwrap_or_default();
            cmd.env("PATH", format!("{}{}{}", occt_bin.display(), separator, existing_path));
        }
    }

    let mut child = cmd.spawn()
        .map_err(|e| format!("failed to start cad_core at {}: {e}", core_path.display()))?;

    let stdin = child
        .stdin
        .take()
        .ok_or_else(|| "failed to capture cad_core stdin".to_string())?;

    let stdout = child
        .stdout
        .take()
        .ok_or_else(|| "failed to capture cad_core stdout".to_string())?;

    let stderr = child
        .stderr
        .take()
        .ok_or_else(|| "failed to capture cad_core stderr".to_string())?;

    let exit_reported = Arc::new(AtomicBool::new(false));

    spawn_stdout_thread(app.clone(), stdout, Arc::clone(&exit_reported));
    spawn_stderr_thread(app.clone(), stderr);

    *guard = Some(CadCoreProcess {
        child,
        stdin,
        exit_reported,
    });

    Ok("started".to_string())
}

pub fn send_core_command(
    app: AppHandle,
    state: tauri::State<CadCoreState>,
    command: String,
) -> Result<(), String> {
    let mut guard = state.child.lock().map_err(|e| e.to_string())?;
    let result = {
        let process = guard
            .as_mut()
            .ok_or_else(|| "cad_core is not running".to_string())?;
        writeln!(process.stdin, "{command}")
            .map_err(|error| format!("failed to write command to cad_core: {error}"))
            .and_then(|_| {
                process
                    .stdin
                    .flush()
                    .map_err(|error| format!("failed to flush cad_core stdin: {error}"))
            })
    };
    if let Err(error) = &result {
        let should_emit = guard
            .as_ref()
            .map(|process| !process.exit_reported.swap(true, Ordering::SeqCst))
            .unwrap_or(true);
        *guard = None;
        if should_emit {
            let _ = app.emit(
                "cad-core-exited",
                format!("cad_core command pipe closed: {error}"),
            );
        }
    }
    result?;

    Ok(())
}

fn spawn_stdout_thread(
    app: AppHandle,
    stdout: impl std::io::Read + Send + 'static,
    exit_reported: Arc<AtomicBool>,
) {
    std::thread::spawn(move || {
        let reader = BufReader::new(stdout);

        for line_result in reader.lines() {
            match line_result {
                Ok(line) => {
                    if let Err(error) = emit_core_event(&app, &line) {
                        let _ = emit_core_error(
                            &app,
                            &format!("failed to parse cad_core stdout as JSON: {error}"),
                        );
                    }
                }
                Err(error) => {
                    let _ = emit_core_error(&app, &format!("stdout read error: {error}"));
                    break;
                }
            }
        }

        if !exit_reported.swap(true, Ordering::SeqCst) {
            let state = app.state::<CadCoreState>();
            if let Ok(mut guard) = state.child.lock() {
                let is_current_process = guard
                    .as_ref()
                    .map(|process| Arc::ptr_eq(&process.exit_reported, &exit_reported))
                    .unwrap_or(false);
                if is_current_process {
                    *guard = None;
                }
            }
            let _ = app.emit("cad-core-exited", "cad_core stdout closed");
        }
    });
}

fn spawn_stderr_thread(app: AppHandle, stderr: impl std::io::Read + Send + 'static) {
    std::thread::spawn(move || {
        let reader = BufReader::new(stderr);

        for line_result in reader.lines() {
            match line_result {
                Ok(line) => {
                    if !is_structured_core_log_line(&line) {
                        let _ = emit_core_log(&app, &line);
                    }
                }
                Err(error) => {
                    let _ = emit_core_error(&app, &format!("stderr read error: {error}"));
                    break;
                }
            }
        }
    });
}

fn is_structured_core_log_line(line: &str) -> bool {
    line.starts_with("[20")
        && (line.contains("] [debug] [")
            || line.contains("] [info] [")
            || line.contains("] [warn] [")
            || line.contains("] [error] ["))
}
