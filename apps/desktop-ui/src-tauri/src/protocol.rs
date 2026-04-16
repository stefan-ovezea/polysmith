use serde_json::Value;
use tauri::{AppHandle, Emitter};

pub fn emit_core_event(app: &AppHandle, line: &str) -> Result<(), String> {
    let message: Value =
        serde_json::from_str(line).map_err(|error| format!("invalid JSON: {error}"))?;

    app.emit("cad-core-event", message)
        .map_err(|error| error.to_string())
}

pub fn emit_core_log(app: &AppHandle, message: &str) -> Result<(), String> {
    app.emit("cad-core-log", message.to_string())
        .map_err(|error| error.to_string())
}

pub fn emit_core_error(app: &AppHandle, message: &str) -> Result<(), String> {
    app.emit("cad-core-error", message.to_string())
        .map_err(|error| error.to_string())
}
