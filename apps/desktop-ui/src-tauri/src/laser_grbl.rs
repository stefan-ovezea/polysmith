//! LaserGRBL external-launch handoff.
//!
//! LaserGRBL is a Windows G-code streamer for GRBL laser controllers.
//! PolySmith stays a file producer here: the exported `.nc` is passed
//! as an argument so LaserGRBL opens it directly — the same external
//! launch pattern as the OrcaSlicer bridge (orca_slicer.rs).

use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::{Child, Command};

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LaserGrblLaunchRequest {
    pub binary_path: String,
    pub gcode_file_path: Option<String>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct LaserGrblLaunchResult {
    pub platform: String,
    pub process_id: u32,
    pub status: String,
    pub message: String,
}

/// Launches LaserGRBL as a standalone application with the exported
/// G-code file.  The spawned process is intentionally not tracked —
/// it lives in its own window and is managed by the user.
pub fn launch_laser_grbl(
    request: LaserGrblLaunchRequest,
) -> Result<LaserGrblLaunchResult, String> {
    let binary_path = request.binary_path.trim();
    if binary_path.is_empty() {
        return Err("LaserGRBL binary path is not configured".to_string());
    }
    let launch_path = PathBuf::from(binary_path);
    if !launch_path.exists() {
        return Err(format!("LaserGRBL path does not exist: {binary_path}"));
    }
    let gcode_file_path = request
        .gcode_file_path
        .as_deref()
        .map(str::trim)
        .filter(|path| !path.is_empty());

    let mut command = Command::new(&launch_path);
    if let Some(path) = gcode_file_path {
        command.arg(path);
    }
    let child: Child = command.spawn().map_err(|error| {
        format!(
            "Failed to launch LaserGRBL at {}: {error}",
            launch_path.display()
        )
    })?;

    Ok(LaserGrblLaunchResult {
        platform: std::env::consts::OS.to_string(),
        process_id: child.id(),
        status: "running".to_string(),
        message: "LaserGRBL launched as a separate application.".to_string(),
    })
}
