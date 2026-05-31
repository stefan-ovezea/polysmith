// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod app_config;
mod cad_core;
mod orca_slicer;
mod project_metadata;
mod protocol;

use std::{
    fs,
    path::Path,
    sync::Mutex,
    thread,
    time::Duration,
};

use cad_core::{start_cad_core_process, CadCoreState};
use orca_slicer::OrcaSlicerState;
use serde::Serialize;
use serde_json::Value;
use tauri::Manager;

#[tauri::command]
fn start_cad_core(
    app: tauri::AppHandle,
    state: tauri::State<CadCoreState>,
) -> Result<String, String> {
    if let Err(error) = reveal_main_window(&app) {
        eprintln!("failed to reveal main window during core startup: {error}");
    }

    start_cad_core_process(app, state)
}

#[tauri::command]
fn send_core_command(
    app: tauri::AppHandle,
    state: tauri::State<CadCoreState>,
    command: String,
) -> Result<(), String> {
    cad_core::send_core_command(app, state, command)
}

#[tauri::command]
fn bootstrap_app_config(
    default_config: Value,
    default_themes: Vec<app_config::ThemeFile>,
) -> Result<app_config::ConfigBootstrap, String> {
    app_config::bootstrap_app_config(default_config, default_themes)
}

#[tauri::command]
fn save_app_config(config: Value) -> Result<(), String> {
    app_config::save_app_config(config)
}

#[tauri::command]
fn load_recent_projects() -> Result<Value, String> {
    project_metadata::load_recent_projects()
}

#[tauri::command]
fn save_recent_projects(document: Value) -> Result<(), String> {
    project_metadata::save_recent_projects(document)
}

#[tauri::command]
fn read_project_thumbnail(file_path: String) -> Result<Option<String>, String> {
    project_metadata::read_project_thumbnail(file_path)
}

#[tauri::command]
fn write_project_thumbnail(
    file_path: String,
    thumbnail_data_url: Option<String>,
) -> Result<(), String> {
    project_metadata::write_project_thumbnail(file_path, thumbnail_data_url)
}

#[tauri::command]
fn delete_project_file(file_path: String) -> Result<(), String> {
    project_metadata::delete_project_file(file_path)
}

#[tauri::command]
fn project_file_exists(file_path: String) -> Result<bool, String> {
    project_metadata::project_file_exists(file_path)
}

#[derive(Serialize)]
struct ImportAssetBytes {
    mime_type: String,
    bytes: Vec<u8>,
}

fn import_asset_mime_type(file_path: &str) -> &'static str {
    match Path::new(file_path)
        .extension()
        .and_then(|extension| extension.to_str())
        .map(|extension| extension.to_ascii_lowercase())
        .as_deref()
    {
        Some("png") => "image/png",
        Some("jpg") | Some("jpeg") => "image/jpeg",
        Some("webp") => "image/webp",
        Some("gif") => "image/gif",
        Some("svg") => "image/svg+xml",
        _ => "application/octet-stream",
    }
}

#[tauri::command]
fn read_import_asset(file_path: String) -> Result<ImportAssetBytes, String> {
    let bytes = fs::read(&file_path)
        .map_err(|error| format!("failed to read import asset {file_path}: {error}"))?;
    Ok(ImportAssetBytes {
        mime_type: import_asset_mime_type(&file_path).to_string(),
        bytes,
    })
}

fn center_window_over_window(
    window: &tauri::WebviewWindow,
    anchor: &tauri::WebviewWindow,
) -> Result<(), String> {
    let anchor_position = anchor
        .outer_position()
        .map_err(|error| format!("failed to read anchor window position: {error}"))?;
    let anchor_size = anchor
        .outer_size()
        .map_err(|error| format!("failed to read anchor window size: {error}"))?;
    let window_size = window
        .outer_size()
        .or_else(|_| window.inner_size())
        .map_err(|error| format!("failed to read splash window size: {error}"))?;

    let mut position = anchor_position;
    position.x += ((anchor_size.width as i32 - window_size.width as i32) / 2).max(0);
    position.y += ((anchor_size.height as i32 - window_size.height as i32) / 2).max(0);

    window
        .set_position(position)
        .map_err(|error| format!("failed to position splash window: {error}"))
}

fn position_startup_splash(app: &tauri::AppHandle) {
    if let Some(splash_window) = app.get_webview_window("splashscreen") {
        if let Some(main_window) = app.get_webview_window("main") {
            if let Err(error) = center_window_over_window(&splash_window, &main_window) {
                eprintln!("{error}");
                if let Err(center_error) = splash_window.center() {
                    eprintln!("failed to center splash window: {center_error}");
                }
            }
        }
        if let Err(error) = splash_window.show() {
            eprintln!("failed to show splash window: {error}");
        }
        if let Err(error) = splash_window.set_focus() {
            eprintln!("failed to focus splash window: {error}");
        }
    }
}

fn reveal_main_window(app: &tauri::AppHandle) -> Result<(), String> {
    let main_window = app
        .get_webview_window("main")
        .ok_or_else(|| "main window was not found".to_string())?;

    let maximize_result = main_window.maximize();
    main_window
        .show()
        .map_err(|error| format!("failed to show main window: {error}"))?;
    let focus_result = main_window.set_focus();

    if let Some(splash_window) = app.get_webview_window("splashscreen") {
        let _ = splash_window.close();
    }

    if let Err(error) = maximize_result {
        eprintln!("failed to maximize main window: {error}");
    }
    if let Err(error) = focus_result {
        eprintln!("failed to focus main window: {error}");
    }

    Ok(())
}

#[tauri::command]
fn show_main_window(app: tauri::AppHandle) -> Result<(), String> {
    reveal_main_window(&app)
}

#[tauri::command]
fn prepare_orca_export_path() -> Result<String, String> {
    orca_slicer::prepare_orca_export_path()
}

#[tauri::command]
fn embed_orca_window(
    window: tauri::WebviewWindow,
    state: tauri::State<OrcaSlicerState>,
    request: orca_slicer::OrcaEmbedRequest,
) -> Result<orca_slicer::OrcaEmbedResult, String> {
    orca_slicer::embed_orca_window(window, state, request)
}

#[tauri::command]
fn resize_orca_window(
    state: tauri::State<OrcaSlicerState>,
    bounds: orca_slicer::SlicerViewportBounds,
) -> Result<orca_slicer::OrcaEmbedResult, String> {
    orca_slicer::resize_orca_window(state, bounds)
}

#[tauri::command]
fn hide_orca_window(
    state: tauri::State<OrcaSlicerState>,
) -> Result<orca_slicer::OrcaEmbedResult, String> {
    orca_slicer::hide_orca_window(state)
}

#[tauri::command]
fn set_orca_mapped(
    state: tauri::State<OrcaSlicerState>,
    mapped: bool,
) -> Result<orca_slicer::OrcaEmbedResult, String> {
    orca_slicer::set_orca_mapped_window(state, mapped)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    orca_slicer::configure_linux_windowing_environment();

    tauri::Builder::default()
        .manage(CadCoreState {
            child: Mutex::new(None),
        })
        .manage(OrcaSlicerState::default())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_opener::init())
        .setup(|app| {
            position_startup_splash(app.handle());
            let app_handle = app.handle().clone();
            thread::spawn(move || {
                // Startup normally reveals the main window when the renderer invokes
                // `start_cad_core`. This late fallback prevents a renderer failure from
                // leaving users staring at the splash screen forever.
                thread::sleep(Duration::from_millis(10000));
                if app_handle.get_webview_window("splashscreen").is_some() {
                    let _ = reveal_main_window(&app_handle);
                }
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            start_cad_core,
            send_core_command,
            bootstrap_app_config,
            save_app_config,
            load_recent_projects,
            save_recent_projects,
            read_project_thumbnail,
            write_project_thumbnail,
            delete_project_file,
            project_file_exists,
            read_import_asset,
            show_main_window,
            prepare_orca_export_path,
            embed_orca_window,
            resize_orca_window,
            hide_orca_window,
            set_orca_mapped
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

fn main() {
    run();
}
