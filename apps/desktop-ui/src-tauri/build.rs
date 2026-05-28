use std::env;
use std::fs;
use std::path::PathBuf;

fn rust_string(value: &str) -> String {
    format!("{value:?}")
}

fn main() {
    println!("cargo:rerun-if-env-changed=POLYSMITH_CAD_CORE_PATH_KIND");
    println!("cargo:rerun-if-env-changed=POLYSMITH_CAD_CORE_WORKSPACE_PATH");
    println!("cargo:rerun-if-env-changed=POLYSMITH_CAD_CORE_RESOURCE_PATH");

    let path_kind =
        env::var("POLYSMITH_CAD_CORE_PATH_KIND").unwrap_or_else(|_| "workspace".to_string());
    let workspace_path = env::var("POLYSMITH_CAD_CORE_WORKSPACE_PATH")
        .unwrap_or_else(|_| "../../../native/cad-core/build/Debug/cad_core".to_string());
    let resource_path = env::var("POLYSMITH_CAD_CORE_RESOURCE_PATH")
        .unwrap_or_else(|_| "resources/cad_core".to_string());

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR is set by Cargo"));
    let generated_path = out_dir.join("cad_core_build_config.rs");
    let generated = format!(
        "pub const CAD_CORE_PATH_KIND: &str = {};\n\
         pub const CAD_CORE_WORKSPACE_PATH: &str = {};\n\
         pub const CAD_CORE_RESOURCE_PATH: &str = {};\n",
        rust_string(&path_kind),
        rust_string(&workspace_path),
        rust_string(&resource_path),
    );
    fs::write(generated_path, generated).expect("failed to write cad_core build config");

    tauri_build::build()
}
