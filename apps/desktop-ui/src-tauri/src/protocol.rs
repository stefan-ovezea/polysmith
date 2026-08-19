use std::io::Write;

use base64::Engine;
use base64::engine::general_purpose::STANDARD as BASE64;
use flate2::Compression;
use flate2::write::GzEncoder;
use serde_json::Value;
use tauri::{AppHandle, Emitter};

// Lines larger than this are gzip-compressed before crossing the
// Tauri -> WebView2 event channel. Multi-megabyte JSON viewport events
// (a mesh body with thousands of pick faces plus the sketch) made the
// WebView2 message channel freeze the UI after large projections.
const COMPRESSION_THRESHOLD_BYTES: usize = 64 * 1024;

pub fn emit_core_event(app: &AppHandle, line: &str) -> Result<(), String> {
    if line.len() <= COMPRESSION_THRESHOLD_BYTES {
        let message: Value =
            serde_json::from_str(line).map_err(|error| format!("invalid JSON: {error}"))?;
        return app
            .emit("cad-core-event", message)
            .map_err(|error| error.to_string());
    }

    // Compressed path: the event carries a marker object whose payload
    // is the base64-encoded gzip of the original JSON line. The UI
    // decompresses before parsing (see cadCoreClient.ts).
    let mut encoder = GzEncoder::new(Vec::new(), Compression::fast());
    encoder
        .write_all(line.as_bytes())
        .map_err(|error| format!("gzip write failed: {error}"))?;
    let compressed = encoder
        .finish()
        .map_err(|error| format!("gzip finish failed: {error}"))?;
    let message = serde_json::json!({ "_gz": BASE64.encode(compressed) });
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
