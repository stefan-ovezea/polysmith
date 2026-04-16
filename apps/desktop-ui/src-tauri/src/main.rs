// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod cad_core;
mod protocol;

use std::sync::Mutex;

use cad_core::{start_cad_core_process, CadCoreState};

#[tauri::command]
fn start_cad_core(app: tauri::AppHandle, state: tauri::State<CadCoreState>) -> Result<String, String> {
    start_cad_core_process(app, state)
}

#[tauri::command]
fn send_core_command(
    state: tauri::State<CadCoreState>,
    command: String,
) -> Result<(), String> {
    cad_core::send_core_command(state, command)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(CadCoreState {
            child: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            start_cad_core,
            send_core_command
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

fn main() {
    run();
}
