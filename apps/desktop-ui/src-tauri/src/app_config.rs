use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::env;
use std::fs;
use std::path::PathBuf;

#[cfg(any(target_os = "linux", target_os = "macos"))]
const CONFIG_PATH: &str = ".config/polysmith";
#[cfg(target_os = "windows")]
const CONFIG_PATH: &str = "polysmith";

const CONFIG_FILE_NAME: &str = "config.json";
const THEMES_DIR_NAME: &str = "themes";

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ThemeFile {
    pub file_name: String,
    pub contents: Value,
}

#[derive(Debug, Serialize)]
pub struct ConfigBootstrap {
    pub config_path: String,
    pub themes_path: String,
    pub config: Value,
    pub themes: Vec<Value>,
}

pub fn config_dir() -> Result<PathBuf, String> {
    #[cfg(target_os = "windows")]
    {
        let app_data = env::var_os("APPDATA").ok_or_else(|| "APPDATA is not set".to_string())?;
        return Ok(PathBuf::from(app_data).join(CONFIG_PATH));
    }

    #[cfg(any(target_os = "linux", target_os = "macos"))]
    {
        let home = env::var_os("HOME").ok_or_else(|| "HOME is not set".to_string())?;
        return Ok(PathBuf::from(home).join(CONFIG_PATH));
    }

    #[allow(unreachable_code)]
    Err("unsupported operating system".to_string())
}

fn config_file_path() -> Result<PathBuf, String> {
    Ok(config_dir()?.join(CONFIG_FILE_NAME))
}

fn write_json(path: &PathBuf, value: &Value) -> Result<(), String> {
    let json = serde_json::to_string_pretty(value).map_err(|error| error.to_string())?;
    fs::write(path, format!("{json}\n")).map_err(|error| error.to_string())
}

pub fn bootstrap_app_config(
    default_config: Value,
    default_themes: Vec<ThemeFile>,
) -> Result<ConfigBootstrap, String> {
    let config_dir = config_dir()?;
    let config_path = config_dir.join(CONFIG_FILE_NAME);
    let themes_path = config_dir.join(THEMES_DIR_NAME);

    fs::create_dir_all(&themes_path).map_err(|error| error.to_string())?;

    if !config_path.exists() {
        write_json(&config_path, &default_config)?;
    }

    for theme in default_themes {
        let theme_path = themes_path.join(theme.file_name);
        if !theme_path.exists() {
            write_json(&theme_path, &theme.contents)?;
            continue;
        }
        // The stored file may predate tokens added to the bundled theme
        // (e.g. --cad-muted) — backfill the missing color keys so the
        // UI never renders against an unresolved CSS variable.  Only
        // MISSING keys are added: user edits to existing values stay.
        let stored = fs::read_to_string(&theme_path).map_err(|error| error.to_string())?;
        let mut stored_value: Value =
            serde_json::from_str(&stored).map_err(|error| error.to_string())?;
        let Some(stored_colors) = stored_value.get_mut("colors").and_then(|v| v.as_object_mut())
        else {
            continue;
        };
        let mut changed = false;
        if let Some(bundled_colors) = theme.contents.get("colors").and_then(|v| v.as_object()) {
            for (key, value) in bundled_colors {
                if !stored_colors.contains_key(key) {
                    stored_colors.insert(key.clone(), value.clone());
                    changed = true;
                }
            }
        }
        if changed {
            write_json(&theme_path, &stored_value)?;
        }
    }

    let config_text = fs::read_to_string(&config_path).map_err(|error| error.to_string())?;
    let config = serde_json::from_str(&config_text).map_err(|error| error.to_string())?;

    let mut themes = Vec::new();
    for entry in fs::read_dir(&themes_path).map_err(|error| error.to_string())? {
        let entry = entry.map_err(|error| error.to_string())?;
        let path = entry.path();
        if path.extension().and_then(|extension| extension.to_str()) != Some("json") {
            continue;
        }
        let theme_text = fs::read_to_string(&path).map_err(|error| error.to_string())?;
        let theme = serde_json::from_str(&theme_text).map_err(|error| error.to_string())?;
        themes.push(theme);
    }

    Ok(ConfigBootstrap {
        config_path: config_path.to_string_lossy().to_string(),
        themes_path: themes_path.to_string_lossy().to_string(),
        config,
        themes,
    })
}

pub fn save_app_config(config: Value) -> Result<(), String> {
    let config_path = config_file_path()?;
    if let Some(parent) = config_path.parent() {
        fs::create_dir_all(parent).map_err(|error| error.to_string())?;
    }
    write_json(&config_path, &config)
}
