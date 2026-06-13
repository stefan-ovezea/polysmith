use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};
use std::fs;
use std::path::PathBuf;

use crate::app_config;

const PLUGINS_DIR_NAME: &str = "plugins";
const PLUGIN_CONFIG_FILE_NAME: &str = "config.json";

#[derive(Debug, Deserialize)]
pub struct PluginConfigEntry {
    pub plugin_id: String,
    pub enabled: bool,
    pub config: Value,
}

#[derive(Debug, Serialize)]
pub struct PluginConfigBootstrap {
    pub config_path: String,
    pub config: Value,
}

fn plugin_config_dir() -> Result<PathBuf, String> {
    Ok(app_config::config_dir()?.join(PLUGINS_DIR_NAME))
}

fn plugin_config_file_path() -> Result<PathBuf, String> {
    Ok(plugin_config_dir()?.join(PLUGIN_CONFIG_FILE_NAME))
}

fn write_json(path: &PathBuf, value: &Value) -> Result<(), String> {
    let json = serde_json::to_string_pretty(value).map_err(|error| error.to_string())?;
    fs::write(path, format!("{json}\n")).map_err(|error| error.to_string())
}

fn default_plugin_config(entries: Vec<PluginConfigEntry>) -> Value {
    let mut plugins = Map::new();
    for entry in entries {
        plugins.insert(
            entry.plugin_id,
            serde_json::json!({
                "enabled": entry.enabled,
                "config": entry.config,
            }),
        );
    }

    Value::Object(Map::from_iter([("plugins".to_string(), Value::Object(plugins))]))
}

pub fn bootstrap_plugin_config(
    default_plugins: Vec<PluginConfigEntry>,
) -> Result<PluginConfigBootstrap, String> {
    let config_dir = plugin_config_dir()?;
    let config_path = plugin_config_file_path()?;
    fs::create_dir_all(&config_dir).map_err(|error| error.to_string())?;

    if !config_path.exists() {
        write_json(&config_path, &default_plugin_config(default_plugins))?;
    }

    let config_text = fs::read_to_string(&config_path).map_err(|error| error.to_string())?;
    let config = serde_json::from_str(&config_text).map_err(|error| error.to_string())?;

    Ok(PluginConfigBootstrap {
        config_path: config_path.to_string_lossy().to_string(),
        config,
    })
}

pub fn save_plugin_config(config: Value) -> Result<(), String> {
    let config_path = plugin_config_file_path()?;
    if let Some(parent) = config_path.parent() {
        fs::create_dir_all(parent).map_err(|error| error.to_string())?;
    }
    write_json(&config_path, &config)
}
