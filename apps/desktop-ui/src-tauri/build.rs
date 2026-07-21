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
    let workspace_path = env::var("POLYSMITH_CAD_CORE_WORKSPACE_PATH").unwrap_or_else(|_| {
        #[cfg(target_os = "windows")]
        {
            "../../../native/cad-core/build/cad_core".to_string()
        }
        #[cfg(not(target_os = "windows"))]
        {
            "../../../native/cad-core/build/cad_core".to_string()
        }
    });
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

    // On Windows, deploy OCCT and 3rd-party DLLs next to cad_core.exe so the
    // child process finds them without needing PATH manipulation at runtime.
    #[cfg(target_os = "windows")]
    {
        let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        let build_dir = manifest_dir.join("../../../native/cad-core/build");
        let third_party = manifest_dir.join("../../../third_party");

        let dll_sources: &[PathBuf] = &[
            third_party.join("occt8-build/win64/vc14/bin"),
            third_party.join("3rdparty-vc14-64/freetype-2.13.3-x64/bin"),
            third_party.join("3rdparty-vc14-64/zlib-1.2.8-vc14-64/bin"),
            third_party.join("3rdparty-vc14-64/tbb-2021.13.0-x64/bin"),
            third_party.join("3rdparty-vc14-64/jemalloc-vc14-64/bin"),
            third_party.join("3rdparty-vc14-64/freeimage-3.18.0-x64/bin"),
            third_party.join("3rdparty-vc14-64/lzma-5.2.2-vc14-64/bin"),
        ];

        for src_dir in dll_sources {
            if let Ok(entries) = std::fs::read_dir(src_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().map_or(false, |e| e == "dll") {
                        let dest = build_dir.join(path.file_name().unwrap());
                        // Only copy if source is newer (preserves incremental builds)
                        let should_copy = match (std::fs::metadata(&path), std::fs::metadata(&dest))
                        {
                            (Ok(s), Ok(d)) => {
                                s.modified().ok() > d.modified().ok()
                            }
                            _ => true,
                        };
                        if should_copy {
                            let _ = std::fs::copy(&path, &dest);
                        }
                    }
                }
            }
        }
    }

    tauri_build::build()
}
