// Local AI credentials. The DeepSeek connection settings are intentionally
// kept OUT of the repository and OUT of the persisted app config: they live
// only in the user-owned `~/.polysmith` file (JSON), e.g.:
//
//   {
//     "deepseek_api_key": "sk-...",
//     "deepseek_base_url": "https://api.deepseek.com/anthropic"
//   }
//
// A missing file or malformed content yields empty strings — callers surface
// their own "key not found" error.
use serde_json::Value;
use tauri::Manager;

const KEY_FIELD: &str = "deepseek_api_key";
const URL_FIELD: &str = "deepseek_base_url";

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct AiSettings {
    pub api_key: String,
    pub base_url: String,
    // The exact path this read attempted — surfaced in the Settings status so
    // a "not found" result is self-diagnosing instead of mysterious.
    pub source_path: String,
}

pub fn read_ai_settings(app: &tauri::AppHandle) -> Result<AiSettings, String> {
    // tauri's home_dir() can return an empty path on Windows when the OS
    // known-folder lookup fails — fall back to the env vars the shell always
    // sets for the user's profile.
    let home = app
        .path()
        .home_dir()
        .ok()
        .filter(|path| !path.as_os_str().is_empty())
        .or_else(|| std::env::var_os("USERPROFILE").map(std::path::PathBuf::from))
        .or_else(|| std::env::var_os("HOME").map(std::path::PathBuf::from))
        .ok_or_else(|| "could not resolve the user home directory".to_string())?;
    let source_path = home.join(".polysmith");
    eprintln!(
        "[ai_key] home_dir()={:?} USERPROFILE={:?} HOME={:?} reading {}",
        app.path().home_dir().ok(),
        std::env::var_os("USERPROFILE"),
        std::env::var_os("HOME"),
        source_path.display(),
    );
    let raw = match std::fs::read_to_string(&source_path) {
        Ok(contents) => contents,
        Err(error) => {
            eprintln!("[ai_key] read failed for {}: {error}", source_path.display());
            String::new()
        }
    };
    let parsed: Value = serde_json::from_str(&raw).unwrap_or_else(|error| {
        eprintln!("[ai_key] {} is not valid JSON: {error}", source_path.display());
        Value::Null
    });
    let api_key = parsed
        .get(KEY_FIELD)
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    eprintln!(
        "[ai_key] {} -> key {} ({} chars), url {}",
        source_path.display(),
        if api_key.is_empty() { "EMPTY" } else { "present" },
        api_key.len(),
        parsed
            .get(URL_FIELD)
            .and_then(Value::as_str)
            .unwrap_or(""),
    );
    Ok(AiSettings {
        api_key,
        base_url: parsed
            .get(URL_FIELD)
            .and_then(Value::as_str)
            .unwrap_or("")
            .to_string(),
        source_path: source_path.to_string_lossy().into_owned(),
    })
}
