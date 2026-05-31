use serde_json::{json, Value};
use std::fs;
use std::path::{Path, PathBuf};

use crate::app_config;

const RECENT_PROJECTS_FILE_NAME: &str = "recent-projects.json";

fn recent_projects_path() -> Result<PathBuf, String> {
    Ok(app_config::config_dir()?.join(RECENT_PROJECTS_FILE_NAME))
}

fn ensure_parent_dir(path: &Path) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|error| error.to_string())?;
    }
    Ok(())
}

pub fn load_recent_projects() -> Result<Value, String> {
    let path = recent_projects_path()?;
    if !path.exists() {
        return Ok(json!({
            "version": 2,
            "rootProjectPaths": [],
            "folders": [],
            "projects": []
        }));
    }

    let text = fs::read_to_string(path).map_err(|error| error.to_string())?;
    if text.trim().is_empty() {
        return Ok(json!({
            "version": 2,
            "rootProjectPaths": [],
            "folders": [],
            "projects": []
        }));
    }
    serde_json::from_str(&text).map_err(|error| error.to_string())
}

pub fn save_recent_projects(document: Value) -> Result<(), String> {
    let path = recent_projects_path()?;
    ensure_parent_dir(&path)?;
    let text = serde_json::to_string_pretty(&document).map_err(|error| error.to_string())?;
    fs::write(path, format!("{text}\n")).map_err(|error| error.to_string())
}

pub fn delete_project_file(file_path: String) -> Result<(), String> {
    let path = PathBuf::from(&file_path);
    if path.exists() {
        fs::remove_file(&path).map_err(|error| error.to_string())?;
    }

    let asset_dir = path.with_file_name(format!(
        "{}.assets",
        path.file_stem()
            .and_then(|stem| stem.to_str())
            .unwrap_or_default()
    ));
    if asset_dir.exists() {
        fs::remove_dir_all(asset_dir).map_err(|error| error.to_string())?;
    }
    Ok(())
}

pub fn project_file_exists(file_path: String) -> Result<bool, String> {
    Ok(PathBuf::from(file_path).is_file())
}

pub fn read_project_thumbnail(file_path: String) -> Result<Option<String>, String> {
    let text = fs::read_to_string(file_path).map_err(|error| error.to_string())?;
    let payload: Value = serde_json::from_str(&text).map_err(|error| error.to_string())?;
    Ok(payload
        .get("metadata")
        .and_then(|metadata| metadata.get("thumbnailDataUrl"))
        .and_then(|thumbnail| thumbnail.as_str())
        .map(|thumbnail| thumbnail.to_string()))
}

pub fn write_project_thumbnail(
    file_path: String,
    thumbnail_data_url: Option<String>,
) -> Result<(), String> {
    let text = fs::read_to_string(&file_path).map_err(|error| error.to_string())?;
    let mut payload: Value = serde_json::from_str(&text).map_err(|error| error.to_string())?;
    if !payload.is_object() {
        return Err("Project file is not a JSON object".to_string());
    }

    if payload.get("metadata").is_none() || !payload["metadata"].is_object() {
        payload["metadata"] = json!({});
    }
    payload["metadata"]["thumbnailDataUrl"] = match thumbnail_data_url {
        Some(thumbnail) => Value::String(thumbnail),
        None => Value::Null,
    };

    let path = PathBuf::from(file_path);
    ensure_parent_dir(&path)?;
    let next_text = serde_json::to_string_pretty(&payload).map_err(|error| error.to_string())?;
    fs::write(path, format!("{next_text}\n")).map_err(|error| error.to_string())
}
