use std::{
    fs,
    path::{Path, PathBuf},
    process::{Child, Command},
    sync::Mutex,
    time::{SystemTime, UNIX_EPOCH},
};

use serde::{Deserialize, Serialize};

#[derive(Default)]
pub struct OrcaSlicerState {
    process: Mutex<Option<ManagedOrcaProcess>>,
}

struct ManagedOrcaProcess {
    binary_path: String,
    child: Child,
    window_handle: Option<i64>,
    display_handle: Option<usize>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
#[allow(dead_code)]
pub struct SlicerViewportBounds {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
    pub scale_factor: f64,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct OrcaEmbedRequest {
    pub binary_path: String,
    pub model_file_path: Option<String>,
    pub bounds: SlicerViewportBounds,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct OrcaEmbedResult {
    pub platform: String,
    pub process_id: u32,
    pub status: String,
    pub message: String,
}

pub fn configure_linux_windowing_environment() {
    #[cfg(target_os = "linux")]
    {
        // Wayland does not allow foreign toplevel reparenting. On Wayland
        // desktops this asks GTK/Wry and Qt-based OrcaSlicer to use XWayland
        // so both windows have X11 handles that can be embedded.
        std::env::set_var("GDK_BACKEND", "x11");
        std::env::set_var("GTK_BACKEND", "x11");
        std::env::set_var("WINIT_UNIX_BACKEND", "x11");
        std::env::set_var("QT_QPA_PLATFORM", "xcb");
        std::env::set_var("SDL_VIDEODRIVER", "x11");
    }
}

pub fn prepare_orca_export_path() -> Result<String, String> {
    let dir = std::env::temp_dir().join("polysmith-orca");
    fs::create_dir_all(&dir).map_err(|error| error.to_string())?;

    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|error| error.to_string())?
        .as_millis();
    Ok(dir
        .join(format!("polysmith-orca-{timestamp}.stl"))
        .to_string_lossy()
        .to_string())
}

pub fn embed_orca_window(
    window: tauri::WebviewWindow,
    state: tauri::State<OrcaSlicerState>,
    request: OrcaEmbedRequest,
) -> Result<OrcaEmbedResult, String> {
    let binary_path = request.binary_path.trim();
    if binary_path.is_empty() {
        return Err("OrcaSlicer binary path is not configured".to_string());
    }
    let model_file_path = request
        .model_file_path
        .as_deref()
        .map(str::trim)
        .filter(|path| !path.is_empty());

    let (process_id, cached_window_handle) =
        ensure_orca_process(&state, binary_path, model_file_path)?;

    let attach_result =
        platform_attach_orca_window(&window, process_id, cached_window_handle, &request.bounds)?;
    update_native_handles(
        &state,
        attach_result.window_handle,
        attach_result.display_handle,
    )?;

    Ok(OrcaEmbedResult {
        platform: platform_name().to_string(),
        process_id,
        status: attach_result.status,
        message: attach_result.message,
    })
}

pub fn resize_orca_window(
    state: tauri::State<OrcaSlicerState>,
    bounds: SlicerViewportBounds,
) -> Result<OrcaEmbedResult, String> {
    let (process_id, window_handle, display_handle) = current_orca_window(&state)?;
    let resize_result = platform_resize_orca_window(window_handle, display_handle, &bounds)?;
    Ok(OrcaEmbedResult {
        platform: platform_name().to_string(),
        process_id,
        status: resize_result.status,
        message: resize_result.message,
    })
}

pub fn hide_orca_window(state: tauri::State<OrcaSlicerState>) -> Result<OrcaEmbedResult, String> {
    let (process_id, window_handle, display_handle) = current_orca_window(&state)?;
    let hide_result = platform_hide_orca_window(window_handle, display_handle)?;
    Ok(OrcaEmbedResult {
        platform: platform_name().to_string(),
        process_id,
        status: hide_result.status,
        message: hide_result.message,
    })
}

pub fn set_orca_mapped_window(
    state: tauri::State<OrcaSlicerState>,
    mapped: bool,
) -> Result<OrcaEmbedResult, String> {
    let (process_id, window_handle, display_handle) = current_orca_window(&state)?;
    let result = platform_set_orca_mapped(window_handle, display_handle, mapped)?;
    Ok(OrcaEmbedResult {
        platform: platform_name().to_string(),
        process_id,
        status: result.status,
        message: result.message,
    })
}

fn ensure_orca_process(
    state: &tauri::State<OrcaSlicerState>,
    binary_path: &str,
    model_file_path: Option<&str>,
) -> Result<(u32, Option<i64>), String> {
    let launch_path = resolve_orca_launch_path(binary_path)?;
    let launch_path_string = launch_path.to_string_lossy().to_string();
    let mut process = state
        .process
        .lock()
        .map_err(|_| "OrcaSlicer process state lock poisoned".to_string())?;

    if let Some(managed) = process.as_mut() {
        if managed.binary_path == launch_path_string
            && managed
                .child
                .try_wait()
                .map_err(|error| error.to_string())?
                .is_none()
        {
            if let Some(path) = model_file_path {
                spawn_orca(&launch_path, Some(path))?;
            }
            return Ok((managed.child.id(), managed.window_handle));
        }
    }

    let child = spawn_orca(&launch_path, model_file_path)?;
    let process_id = child.id();
    *process = Some(ManagedOrcaProcess {
        binary_path: launch_path_string,
        child,
        window_handle: None,
        display_handle: None,
    });
    Ok((process_id, None))
}

fn resolve_orca_launch_path(binary_path: &str) -> Result<PathBuf, String> {
    let path = PathBuf::from(binary_path);
    if !path.exists() {
        return Err(format!("OrcaSlicer path does not exist: {binary_path}"));
    }

    #[cfg(target_os = "macos")]
    {
        if path.is_dir()
            && path
                .extension()
                .is_some_and(|extension| extension.eq_ignore_ascii_case("app"))
        {
            return resolve_macos_app_executable(&path);
        }
    }

    Ok(path)
}

#[cfg(target_os = "macos")]
fn resolve_macos_app_executable(app_path: &Path) -> Result<PathBuf, String> {
    let macos_dir = app_path.join("Contents").join("MacOS");
    let app_name = app_path
        .file_stem()
        .and_then(|name| name.to_str())
        .ok_or_else(|| format!("Invalid macOS app bundle path: {}", app_path.display()))?;
    let named_executable = macos_dir.join(app_name);
    if named_executable.is_file() {
        return Ok(named_executable);
    }

    let mut candidates = fs::read_dir(&macos_dir)
        .map_err(|error| format!("Failed to read {}: {error}", macos_dir.display()))?
        .flatten()
        .map(|entry| entry.path())
        .filter(|path| path.is_file())
        .collect::<Vec<_>>();
    candidates.sort();
    candidates.into_iter().next().ok_or_else(|| {
        format!(
            "No executable was found inside macOS app bundle: {}",
            app_path.display()
        )
    })
}

fn spawn_orca(launch_path: &Path, model_file_path: Option<&str>) -> Result<Child, String> {
    let mut command = Command::new(launch_path);
    if let Some(path) = model_file_path {
        command.arg(path);
    }
    configure_orca_environment(&mut command);
    command.spawn().map_err(|error| {
        format!(
            "Failed to launch OrcaSlicer at {}: {error}",
            launch_path.display()
        )
    })
}

fn configure_orca_environment(command: &mut Command) {
    #[cfg(target_os = "linux")]
    {
        command.env("GDK_BACKEND", "x11");
        command.env("GTK_BACKEND", "x11");
        command.env("WINIT_UNIX_BACKEND", "x11");
        command.env("QT_QPA_PLATFORM", "xcb");
        command.env("SDL_VIDEODRIVER", "x11");
    }
    #[cfg(not(target_os = "linux"))]
    {
        let _ = command;
    }
}

fn update_native_handles(
    state: &tauri::State<OrcaSlicerState>,
    window_handle: Option<i64>,
    display_handle: Option<usize>,
) -> Result<(), String> {
    if window_handle.is_none() && display_handle.is_none() {
        return Ok(());
    }
    let mut process = state
        .process
        .lock()
        .map_err(|_| "OrcaSlicer process state lock poisoned".to_string())?;
    if let Some(managed) = process.as_mut() {
        if window_handle.is_some() {
            managed.window_handle = window_handle;
        }
        if display_handle.is_some() {
            managed.display_handle = display_handle;
        }
    }
    Ok(())
}

fn current_orca_window(
    state: &tauri::State<OrcaSlicerState>,
) -> Result<(u32, Option<i64>, Option<usize>), String> {
    let mut process = state
        .process
        .lock()
        .map_err(|_| "OrcaSlicer process state lock poisoned".to_string())?;
    let Some(managed) = process.as_mut() else {
        return Err("OrcaSlicer is not running under PolySmith control".to_string());
    };
    if managed
        .child
        .try_wait()
        .map_err(|error| error.to_string())?
        .is_some()
    {
        *process = None;
        return Err("OrcaSlicer process has exited".to_string());
    }
    Ok((
        managed.child.id(),
        managed.window_handle,
        managed.display_handle,
    ))
}

struct PlatformWindowResult {
    status: String,
    message: String,
    window_handle: Option<i64>,
    display_handle: Option<usize>,
}

#[cfg(windows)]
fn platform_attach_orca_window(
    window: &tauri::WebviewWindow,
    process_id: u32,
    cached_window_handle: Option<i64>,
    bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    windows_impl::attach_orca_window(window, process_id, cached_window_handle, bounds)
}

#[cfg(target_os = "linux")]
fn platform_attach_orca_window(
    window: &tauri::WebviewWindow,
    process_id: u32,
    cached_window_handle: Option<i64>,
    bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    linux_x11_impl::attach_orca_window(window, process_id, cached_window_handle, bounds)
}

#[cfg(target_os = "macos")]
fn platform_attach_orca_window(
    _window: &tauri::WebviewWindow,
    process_id: u32,
    _cached_window_handle: Option<i64>,
    _bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "running".to_string(),
        message: "OrcaSlicer launched as a separate macOS app.".to_string(),
        window_handle: Some(i64::from(process_id)),
        display_handle: None,
    })
}

#[cfg(all(not(windows), not(target_os = "linux"), not(target_os = "macos")))]
fn platform_attach_orca_window(
    _window: &tauri::WebviewWindow,
    _process_id: u32,
    _cached_window_handle: Option<i64>,
    _bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "unsupported".to_string(),
        message: format!(
            "OrcaSlicer was launched, but native window embedding is not implemented on {} yet.",
            platform_name()
        ),
        window_handle: None,
        display_handle: None,
    })
}

#[cfg(windows)]
fn platform_resize_orca_window(
    window_handle: Option<i64>,
    _display_handle: Option<usize>,
    bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    windows_impl::resize_orca_window(window_handle, bounds)
}

#[cfg(target_os = "linux")]
fn platform_resize_orca_window(
    window_handle: Option<i64>,
    display_handle: Option<usize>,
    bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    linux_x11_impl::resize_orca_window(window_handle, display_handle, bounds)
}

#[cfg(target_os = "macos")]
fn platform_resize_orca_window(
    window_handle: Option<i64>,
    _display_handle: Option<usize>,
    _bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "running".to_string(),
        message: "OrcaSlicer is running as a separate macOS app.".to_string(),
        window_handle,
        display_handle: None,
    })
}

#[cfg(all(not(windows), not(target_os = "linux"), not(target_os = "macos")))]
fn platform_resize_orca_window(
    _window_handle: Option<i64>,
    _display_handle: Option<usize>,
    _bounds: &SlicerViewportBounds,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "unsupported".to_string(),
        message: format!(
            "Native OrcaSlicer window resizing is not implemented on {} yet.",
            platform_name()
        ),
        window_handle: None,
        display_handle: None,
    })
}

#[cfg(windows)]
fn platform_hide_orca_window(
    window_handle: Option<i64>,
    _display_handle: Option<usize>,
) -> Result<PlatformWindowResult, String> {
    windows_impl::hide_orca_window(window_handle)
}

#[cfg(target_os = "linux")]
fn platform_hide_orca_window(
    window_handle: Option<i64>,
    display_handle: Option<usize>,
) -> Result<PlatformWindowResult, String> {
    linux_x11_impl::hide_orca_window(window_handle, display_handle)
}

#[cfg(target_os = "macos")]
fn platform_hide_orca_window(
    window_handle: Option<i64>,
    _display_handle: Option<usize>,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "running".to_string(),
        message: "OrcaSlicer is running separately on macOS.".to_string(),
        window_handle,
        display_handle: None,
    })
}

#[cfg(all(not(windows), not(target_os = "linux"), not(target_os = "macos")))]
fn platform_hide_orca_window(
    _window_handle: Option<i64>,
    _display_handle: Option<usize>,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "unsupported".to_string(),
        message: format!(
            "Native OrcaSlicer window hiding is not implemented on {} yet.",
            platform_name()
        ),
        window_handle: None,
        display_handle: None,
    })
}

#[cfg(windows)]
fn platform_set_orca_mapped(
    window_handle: Option<i64>,
    _display_handle: Option<usize>,
    mapped: bool,
) -> Result<PlatformWindowResult, String> {
    if mapped {
        // Window is already visible from the embed step; show is a no-op.
        Ok(PlatformWindowResult {
            status: "embedded".to_string(),
            message: "OrcaSlicer window is visible on Windows.".to_string(),
            window_handle,
            display_handle: None,
        })
    } else {
        windows_impl::hide_orca_window(window_handle)
    }
}

#[cfg(target_os = "linux")]
fn platform_set_orca_mapped(
    window_handle: Option<i64>,
    display_handle: Option<usize>,
    mapped: bool,
) -> Result<PlatformWindowResult, String> {
    linux_x11_impl::set_orca_mapped(window_handle, display_handle, mapped)
}

#[cfg(target_os = "macos")]
fn platform_set_orca_mapped(
    window_handle: Option<i64>,
    _display_handle: Option<usize>,
    mapped: bool,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: if mapped { "running" } else { "hidden" }.to_string(),
        message: if mapped {
            "OrcaSlicer is visible as a separate macOS app.".to_string()
        } else {
            "OrcaSlicer hidden on macOS.".to_string()
        },
        window_handle,
        display_handle: None,
    })
}

#[cfg(all(not(windows), not(target_os = "linux"), not(target_os = "macos")))]
fn platform_set_orca_mapped(
    _window_handle: Option<i64>,
    _display_handle: Option<usize>,
    _mapped: bool,
) -> Result<PlatformWindowResult, String> {
    Ok(PlatformWindowResult {
        status: "unsupported".to_string(),
        message: format!(
            "Native OrcaSlicer window visibility toggling is not implemented on {} yet.",
            platform_name()
        ),
        window_handle: None,
        display_handle: None,
    })
}

fn platform_name() -> &'static str {
    if cfg!(target_os = "windows") {
        "windows"
    } else if cfg!(target_os = "macos") {
        "macos"
    } else if cfg!(target_os = "linux") {
        "linux"
    } else {
        "unknown"
    }
}

#[cfg(target_os = "linux")]
mod linux_x11_impl {
    use std::{
        collections::{HashSet, VecDeque},
        ffi::{c_char, c_int, c_long, c_uchar, c_uint, c_ulong, c_void, CString},
        fs, ptr, thread,
        time::Duration,
    };

    use raw_window_handle::{HasDisplayHandle, HasWindowHandle, RawDisplayHandle, RawWindowHandle};

    use super::{PlatformWindowResult, SlicerViewportBounds};

    type Display = c_void;
    type Window = c_ulong;
    type Atom = c_ulong;

    const ANY_PROPERTY_TYPE: Atom = 0;
    const PROP_MODE_REPLACE: c_int = 0;
    const MWM_HINTS_DECORATIONS: c_ulong = 1 << 1;

    const PROP_MODE_APPEND: c_int = 2;

    // X event masks
    const KEY_PRESS_MASK: c_long = 1;
    const KEY_RELEASE_MASK: c_long = 1 << 1;
    const BUTTON_PRESS_MASK: c_long = 1 << 2;
    const BUTTON_RELEASE_MASK: c_long = 1 << 3;
    const POINTER_MOTION_MASK: c_long = 1 << 6;
    const FOCUS_CHANGE_MASK: c_long = 1 << 21;
    const STRUCTURE_NOTIFY_MASK: c_long = 1 << 17;
    const EXPOSURE_MASK: c_long = 1 << 15;
    const ENTER_WINDOW_MASK: c_long = 1 << 4;
    const LEAVE_WINDOW_MASK: c_long = 1 << 5;

    const XA_ATOM: Atom = 4;

    const XEMBED_MAPPED: c_ulong = 1;

    #[repr(C)]
    struct MotifWmHints {
        flags: c_ulong,
        functions: c_ulong,
        decorations: c_ulong,
        input_mode: c_long,
        status: c_ulong,
    }

    #[link(name = "X11")]
    extern "C" {
        fn XDefaultRootWindow(display: *mut Display) -> Window;
        fn XQueryTree(
            display: *mut Display,
            window: Window,
            root_return: *mut Window,
            parent_return: *mut Window,
            children_return: *mut *mut Window,
            nchildren_return: *mut c_uint,
        ) -> c_int;
        fn XFree(data: *mut c_void) -> c_int;
        fn XInternAtom(
            display: *mut Display,
            atom_name: *const c_char,
            only_if_exists: c_int,
        ) -> Atom;
        fn XGetGeometry(
            display: *mut Display,
            drawable: Window,
            root_return: *mut Window,
            x_return: *mut c_int,
            y_return: *mut c_int,
            width_return: *mut c_uint,
            height_return: *mut c_uint,
            border_width_return: *mut c_uint,
            depth_return: *mut c_uint,
        ) -> c_int;
        fn XGetWindowProperty(
            display: *mut Display,
            window: Window,
            property: Atom,
            long_offset: c_long,
            long_length: c_long,
            delete: c_int,
            req_type: Atom,
            actual_type_return: *mut Atom,
            actual_format_return: *mut c_int,
            nitems_return: *mut c_ulong,
            bytes_after_return: *mut c_ulong,
            prop_return: *mut *mut c_uchar,
        ) -> c_int;
        fn XChangeProperty(
            display: *mut Display,
            window: Window,
            property: Atom,
            property_type: Atom,
            format: c_int,
            mode: c_int,
            data: *const c_uchar,
            nelements: c_int,
        ) -> c_int;
        fn XReparentWindow(
            display: *mut Display,
            window: Window,
            parent: Window,
            x: c_int,
            y: c_int,
        ) -> c_int;
        fn XMoveResizeWindow(
            display: *mut Display,
            window: Window,
            x: c_int,
            y: c_int,
            width: c_uint,
            height: c_uint,
        ) -> c_int;
        fn XMapWindow(display: *mut Display, window: Window) -> c_int;
        fn XMapRaised(display: *mut Display, window: Window) -> c_int;
        fn XRaiseWindow(display: *mut Display, window: Window) -> c_int;
        fn XUnmapWindow(display: *mut Display, window: Window) -> c_int;
        fn XFlush(display: *mut Display) -> c_int;
        fn XSync(display: *mut Display, discard: c_int) -> c_int;
        fn XSelectInput(display: *mut Display, window: Window, event_mask: c_long) -> c_int;
        fn XClearArea(
            display: *mut Display,
            window: Window,
            x: c_int,
            y: c_int,
            width: c_uint,
            height: c_uint,
            exposures: c_int,
        ) -> c_int;
    }

    /// Number of pixels to clip from the top of the OrcaSlicer window so the
    /// menu bar and toolbar are hidden, leaving only the 3-D viewport and
    /// settings panel visible in the embedded area.
    const MENU_BAR_CLIP: c_int = 100;

    pub fn attach_orca_window(
        window: &tauri::WebviewWindow,
        process_id: u32,
        cached_window_handle: Option<i64>,
        bounds: &SlicerViewportBounds,
    ) -> Result<PlatformWindowResult, String> {
        let (display, parent) = x11_parent_handles(window)?;
        let child = match cached_window_handle {
            Some(handle) => handle as Window,
            None => wait_for_process_window(display, process_id)?,
        };
        let (x, y, width, height) = scaled_bounds(bounds);
        let root = unsafe { XDefaultRootWindow(display) };
        let old_parent = window_parent(display, child)?;

        // Offset the child window upward so the Orca menu bar and toolbar are
        // clipped by the parent boundary, and increase the height so the
        // remaining content still fills the viewport.
        let embed_y = y - MENU_BAR_CLIP;
        let embed_height = height + MENU_BAR_CLIP as c_uint;

        unsafe {
            XUnmapWindow(display, child);
            strip_window_chrome(display, child);
            XReparentWindow(display, child, parent, x, embed_y);
            XMoveResizeWindow(display, child, x, embed_y, width, embed_height);

            // Set XEmbed protocol, WM hints, and event selection *before*
            // mapping so the compositor sees them when the window first
            // appears — avoids a race where the compositor treats the
            // embedded window as a standalone toplevel.
            embed_protocol_setup(display, child)?;

            XMapWindow(display, child);
            XMapRaised(display, child);
            XRaiseWindow(display, child);

            // Force the first paint cycle now that the window is mapped.
            XClearArea(display, child, 0, 0, 0, 0, 1);

            XFlush(display);
            XSync(display, 0);
        }
        let new_parent = window_parent(display, child)?;
        if new_parent != parent {
            return Err(format!(
                "X11 reparenting did not attach OrcaSlicer. Selected window {child:#x} still has parent {new_parent:#x}; expected PolySmith parent {parent:#x}."
            ));
        }

        if old_parent != 0 && old_parent != root && old_parent != parent {
            unsafe {
                XUnmapWindow(display, old_parent);
                XFlush(display);
            }
        }

        Ok(PlatformWindowResult {
            status: "embedded".to_string(),
            message: format!("OrcaSlicer window {child:#x} embedded in the Slicer view."),
            window_handle: Some(child as i64),
            display_handle: Some(display as usize),
        })
    }

    pub fn resize_orca_window(
        window_handle: Option<i64>,
        display_handle: Option<usize>,
        bounds: &SlicerViewportBounds,
    ) -> Result<PlatformWindowResult, String> {
        let Some(handle) = window_handle else {
            return Err("No cached OrcaSlicer X11 window is available".to_string());
        };
        let Some(display_handle) = display_handle else {
            return Err("No cached X11 display handle is available".to_string());
        };
        let display = display_handle as *mut Display;
        let (x, y, width, height) = scaled_bounds(bounds);
        let embed_y = y - MENU_BAR_CLIP;
        let embed_height = height + MENU_BAR_CLIP as c_uint;
        unsafe {
            XMoveResizeWindow(display, handle as Window, x, embed_y, width, embed_height);
            XFlush(display);
        }
        Ok(PlatformWindowResult {
            status: "embedded".to_string(),
            message: "OrcaSlicer window resized to the Slicer view.".to_string(),
            window_handle,
            display_handle: Some(display_handle),
        })
    }

    pub fn hide_orca_window(
        window_handle: Option<i64>,
        display_handle: Option<usize>,
    ) -> Result<PlatformWindowResult, String> {
        let Some(handle) = window_handle else {
            return Err("No cached OrcaSlicer X11 window is available".to_string());
        };
        let Some(display_handle) = display_handle else {
            return Err("No cached X11 display handle is available".to_string());
        };
        let display = display_handle as *mut Display;
        unsafe {
            XUnmapWindow(display, handle as Window);
            XFlush(display);
        }
        Ok(PlatformWindowResult {
            status: "hidden".to_string(),
            message: "OrcaSlicer window hidden; process state preserved.".to_string(),
            window_handle,
            display_handle: Some(display_handle),
        })
    }

    /// Set up XEmbed protocol, compositor hints, and event forwarding on the
    /// child window so the compositor treats it as an embedded window rather
    /// than a separate toplevel.
    fn embed_protocol_setup(display: *mut Display, child: Window) -> Result<(), String> {
        let xembed_atom = intern_atom(display, "_XEMBED_INFO")?;
        let net_wm_state = intern_atom(display, "_NET_WM_STATE")?;
        let skip_taskbar = intern_atom(display, "_NET_WM_STATE_SKIP_TASKBAR")?;
        let skip_pager = intern_atom(display, "_NET_WM_STATE_SKIP_PAGER")?;
        let wm_protocols = intern_atom(display, "WM_PROTOCOLS")?;
        let wm_take_focus = intern_atom(display, "WM_TAKE_FOCUS")?;

        unsafe {
            // _XEMBED_INFO — version 0, XEMBED_MAPPED flag.
            // This tells the compositor (Mutter, KWin, etc.) that the window
            // is an embedded plug, not a standalone toplevel.
            let xembed_data: [u32; 2] = [0, XEMBED_MAPPED as u32];
            XChangeProperty(
                display,
                child,
                xembed_atom,
                xembed_atom,
                32,
                PROP_MODE_REPLACE,
                xembed_data.as_ptr() as *const c_uchar,
                2,
            );

            // _NET_WM_STATE — remove from taskbar and pager so the embedded
            // Orca window doesn't appear as a separate application entry.
            let net_wm_data: [u32; 2] = [skip_taskbar as u32, skip_pager as u32];
            XChangeProperty(
                display,
                child,
                net_wm_state,
                XA_ATOM,
                32,
                PROP_MODE_REPLACE,
                net_wm_data.as_ptr() as *const c_uchar,
                2,
            );

            // WM_PROTOCOLS — append WM_TAKE_FOCUS so the window manager
            // sends a client message when the embedded window should receive
            // keyboard focus, rather than calling XSetInputFocus directly.
            let protocols_data: [u32; 1] = [wm_take_focus as u32];
            XChangeProperty(
                display,
                child,
                wm_protocols,
                XA_ATOM,
                32,
                PROP_MODE_APPEND,
                protocols_data.as_ptr() as *const c_uchar,
                1,
            );

            // XSelectInput — subscribe to all the events Qt needs so its
            // event loop receives keyboard, mouse, focus, expose, and
            // structure-notify events on the reparented window.
            XSelectInput(
                display,
                child,
                KEY_PRESS_MASK
                    | KEY_RELEASE_MASK
                    | BUTTON_PRESS_MASK
                    | BUTTON_RELEASE_MASK
                    | POINTER_MOTION_MASK
                    | FOCUS_CHANGE_MASK
                    | STRUCTURE_NOTIFY_MASK
                    | EXPOSURE_MASK
                    | ENTER_WINDOW_MASK
                    | LEAVE_WINDOW_MASK,
            );

        }

        Ok(())
    }

    /// Temporarily hide or show the embedded Orca window without destroying
    /// the process. Used when HTML dropdowns need to render above the native
    /// X11 child window.
    pub fn set_orca_mapped(
        window_handle: Option<i64>,
        display_handle: Option<usize>,
        mapped: bool,
    ) -> Result<PlatformWindowResult, String> {
        let Some(handle) = window_handle else {
            return Err("No cached OrcaSlicer X11 window is available".to_string());
        };
        let Some(display_handle) = display_handle else {
            return Err("No cached X11 display handle is available".to_string());
        };
        let display = display_handle as *mut Display;
        unsafe {
            if mapped {
                XMapWindow(display, handle as Window);
                XMapRaised(display, handle as Window);
            } else {
                XUnmapWindow(display, handle as Window);
            }
            XFlush(display);
        }
        Ok(PlatformWindowResult {
            status: if mapped { "embedded" } else { "hidden" }.to_string(),
            message: if mapped {
                "OrcaSlicer window shown.".to_string()
            } else {
                "OrcaSlicer window hidden; process state preserved.".to_string()
            },
            window_handle,
            display_handle: Some(display_handle),
        })
    }

    fn x11_parent_handles(window: &tauri::WebviewWindow) -> Result<(*mut Display, Window), String> {
        let display_handle = window.display_handle().map_err(|error| error.to_string())?;
        let window_handle = window.window_handle().map_err(|error| error.to_string())?;

        let display = match display_handle.as_raw() {
            RawDisplayHandle::Xlib(handle) => handle
                .display
                .ok_or_else(|| "X11 display handle is unavailable".to_string())?
                .as_ptr() as *mut Display,
            RawDisplayHandle::Wayland(_) => {
                return Err(
                    "Linux OrcaSlicer embedding requires XWayland. PolySmith requested the X11 backend at startup, but the window is still running on native Wayland. Make sure XWayland is installed/enabled, then restart PolySmith.".to_string(),
                );
            }
            other => {
                return Err(format!(
                    "Linux OrcaSlicer embedding requires an Xlib window; got {other:?}."
                ));
            }
        };

        let parent = match window_handle.as_raw() {
            RawWindowHandle::Xlib(handle) => handle.window,
            RawWindowHandle::Wayland(_) => {
                return Err(
                    "Linux OrcaSlicer embedding requires XWayland. PolySmith requested the X11 backend at startup, but the window is still running on native Wayland. Make sure XWayland is installed/enabled, then restart PolySmith.".to_string(),
                );
            }
            other => {
                return Err(format!(
                    "Linux OrcaSlicer embedding requires an Xlib parent window; got {other:?}."
                ));
            }
        };

        if display.is_null() || parent == 0 {
            return Err("X11 parent window handle is unavailable".to_string());
        }

        Ok((display, parent))
    }

    fn wait_for_process_window(display: *mut Display, process_id: u32) -> Result<Window, String> {
        let mut best_fallback = None;
        for attempt in 0..80 {
            let target_pids = process_family(process_id);
            if let Some(candidate) = find_process_window(display, &target_pids, attempt >= 16)? {
                if candidate.reason == WindowMatchReason::ManagedProcess {
                    return Ok(candidate.window);
                }
                best_fallback = Some(candidate.window);
                if attempt >= 32 {
                    return Ok(candidate.window);
                }
            }
            thread::sleep(Duration::from_millis(125));
        }
        best_fallback.ok_or_else(|| {
            "Timed out waiting for OrcaSlicer to create an X11 window. No managed process window or Orca-branded X11 fallback window was found.".to_string()
        })
    }

    fn find_process_window(
        display: *mut Display,
        target_pids: &HashSet<u32>,
        allow_orca_identity_fallback: bool,
    ) -> Result<Option<WindowCandidate>, String> {
        let root = unsafe { XDefaultRootWindow(display) };
        let atoms = WindowAtoms {
            pid: intern_atom(display, "_NET_WM_PID")?,
            wm_state: intern_atom(display, "WM_STATE")?,
            wm_class: intern_atom(display, "WM_CLASS")?,
            wm_name: intern_atom(display, "WM_NAME")?,
            net_wm_name: intern_atom(display, "_NET_WM_NAME")?,
        };
        let mut candidates = Vec::new();
        collect_window_candidates(
            display,
            root,
            &atoms,
            target_pids,
            allow_orca_identity_fallback,
            &mut candidates,
        )?;
        Ok(candidates.into_iter().max_by_key(WindowCandidate::score))
    }

    fn collect_window_candidates(
        display: *mut Display,
        parent: Window,
        atoms: &WindowAtoms,
        target_pids: &HashSet<u32>,
        allow_orca_identity_fallback: bool,
        candidates: &mut Vec<WindowCandidate>,
    ) -> Result<(), String> {
        if let Some(candidate) = window_candidate(
            display,
            parent,
            atoms,
            target_pids,
            allow_orca_identity_fallback,
        )? {
            candidates.push(candidate);
        }

        let mut root = 0;
        let mut parent_return = 0;
        let mut children: *mut Window = ptr::null_mut();
        let mut child_count = 0;
        let query_ok = unsafe {
            XQueryTree(
                display,
                parent,
                &mut root,
                &mut parent_return,
                &mut children,
                &mut child_count,
            )
        };
        if query_ok == 0 {
            return Ok(());
        }

        let child_slice = if children.is_null() || child_count == 0 {
            &[][..]
        } else {
            unsafe { std::slice::from_raw_parts(children, child_count as usize) }
        };
        for child in child_slice.iter().copied().rev() {
            collect_window_candidates(
                display,
                child,
                atoms,
                target_pids,
                allow_orca_identity_fallback,
                candidates,
            )?;
        }

        if !children.is_null() {
            unsafe {
                XFree(children as *mut c_void);
            }
        }
        Ok(())
    }

    #[derive(Clone, Copy, PartialEq, Eq)]
    enum WindowMatchReason {
        ManagedProcess,
        OrcaIdentity,
    }

    struct WindowCandidate {
        window: Window,
        area: u64,
        has_wm_state: bool,
        reason: WindowMatchReason,
    }

    impl WindowCandidate {
        fn score(&self) -> u64 {
            let reason_bonus = match self.reason {
                WindowMatchReason::ManagedProcess => 2_000_000_000_000,
                WindowMatchReason::OrcaIdentity => 1_000_000_000_000,
            };
            let state_bonus = if self.has_wm_state {
                500_000_000_000
            } else {
                0
            };
            reason_bonus + state_bonus + self.area
        }
    }

    struct WindowAtoms {
        pid: Atom,
        wm_state: Atom,
        wm_class: Atom,
        wm_name: Atom,
        net_wm_name: Atom,
    }

    fn window_candidate(
        display: *mut Display,
        window: Window,
        atoms: &WindowAtoms,
        target_pids: &HashSet<u32>,
        allow_orca_identity_fallback: bool,
    ) -> Result<Option<WindowCandidate>, String> {
        let managed_process_match =
            window_pid(display, window, atoms.pid)?.is_some_and(|pid| target_pids.contains(&pid));
        let orca_identity_match =
            allow_orca_identity_fallback && window_matches_orca_identity(display, window, atoms);
        if !managed_process_match && !orca_identity_match {
            return Ok(None);
        }

        let Some((width, height)) = window_size(display, window) else {
            return Ok(None);
        };
        let area = u64::from(width) * u64::from(height);
        if area == 0 {
            return Ok(None);
        }

        Ok(Some(WindowCandidate {
            window,
            area,
            has_wm_state: window_has_property(display, window, atoms.wm_state),
            reason: if managed_process_match {
                WindowMatchReason::ManagedProcess
            } else {
                WindowMatchReason::OrcaIdentity
            },
        }))
    }

    fn window_size(display: *mut Display, window: Window) -> Option<(c_uint, c_uint)> {
        let mut root = 0;
        let mut x = 0;
        let mut y = 0;
        let mut width = 0;
        let mut height = 0;
        let mut border_width = 0;
        let mut depth = 0;
        let ok = unsafe {
            XGetGeometry(
                display,
                window,
                &mut root,
                &mut x,
                &mut y,
                &mut width,
                &mut height,
                &mut border_width,
                &mut depth,
            )
        };
        (ok != 0).then_some((width, height))
    }

    fn window_parent(display: *mut Display, window: Window) -> Result<Window, String> {
        let mut root = 0;
        let mut parent = 0;
        let mut children: *mut Window = ptr::null_mut();
        let mut child_count = 0;
        let query_ok = unsafe {
            XQueryTree(
                display,
                window,
                &mut root,
                &mut parent,
                &mut children,
                &mut child_count,
            )
        };
        if !children.is_null() {
            unsafe {
                XFree(children as *mut c_void);
            }
        }
        if query_ok == 0 {
            return Err(format!("Failed to query X11 parent for window {window:#x}"));
        }
        Ok(parent)
    }

    fn window_has_property(display: *mut Display, window: Window, property_atom: Atom) -> bool {
        property_bytes(display, window, property_atom)
            .map(|bytes| !bytes.is_empty())
            .unwrap_or(false)
    }

    fn window_matches_orca_identity(
        display: *mut Display,
        window: Window,
        atoms: &WindowAtoms,
    ) -> bool {
        [atoms.wm_class, atoms.wm_name, atoms.net_wm_name]
            .into_iter()
            .filter_map(|atom| property_text(display, window, atom))
            .any(|text| {
                let normalized = text.to_ascii_lowercase();
                normalized.contains("orca") && normalized.contains("slicer")
            })
    }

    fn property_text(display: *mut Display, window: Window, property_atom: Atom) -> Option<String> {
        let bytes = property_bytes(display, window, property_atom)?;
        Some(
            String::from_utf8_lossy(&bytes)
                .replace('\0', " ")
                .trim()
                .to_string(),
        )
    }

    fn property_bytes(
        display: *mut Display,
        window: Window,
        property_atom: Atom,
    ) -> Option<Vec<u8>> {
        let mut actual_type = 0;
        let mut actual_format = 0;
        let mut item_count = 0;
        let mut bytes_after = 0;
        let mut property: *mut c_uchar = ptr::null_mut();

        let status = unsafe {
            XGetWindowProperty(
                display,
                window,
                property_atom,
                0,
                4096,
                0,
                ANY_PROPERTY_TYPE,
                &mut actual_type,
                &mut actual_format,
                &mut item_count,
                &mut bytes_after,
                &mut property,
            )
        };
        if status != 0 || property.is_null() || item_count == 0 {
            if !property.is_null() {
                unsafe {
                    XFree(property as *mut c_void);
                }
            }
            return None;
        }

        let byte_count = match actual_format {
            8 => item_count as usize,
            16 => item_count as usize * 2,
            32 => item_count as usize * std::mem::size_of::<c_ulong>(),
            _ => 0,
        };
        let bytes = if byte_count == 0 {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(property, byte_count).to_vec() }
        };
        unsafe {
            XFree(property as *mut c_void);
        }
        Some(bytes)
    }

    fn process_family(root_pid: u32) -> HashSet<u32> {
        let mut result = HashSet::new();
        let mut queue = VecDeque::new();
        result.insert(root_pid);
        queue.push_back(root_pid);

        while let Some(parent_pid) = queue.pop_front() {
            for child_pid in child_processes(parent_pid) {
                if result.insert(child_pid) {
                    queue.push_back(child_pid);
                }
            }
        }

        result
    }

    fn child_processes(parent_pid: u32) -> Vec<u32> {
        let Ok(entries) = fs::read_dir("/proc") else {
            return Vec::new();
        };
        entries
            .flatten()
            .filter_map(|entry| {
                let pid = entry.file_name().to_str()?.parse::<u32>().ok()?;
                let stat = fs::read_to_string(entry.path().join("stat")).ok()?;
                (process_parent_pid(&stat) == Some(parent_pid)).then_some(pid)
            })
            .collect()
    }

    fn process_parent_pid(stat: &str) -> Option<u32> {
        // /proc/[pid]/stat wraps the command name in parentheses, and the name
        // may contain spaces. The parent pid is the second field after that.
        let close_paren = stat.rfind(')')?;
        let mut fields = stat.get(close_paren + 1..)?.split_whitespace();
        fields.next()?;
        fields.next()?.parse().ok()
    }

    fn window_pid(
        display: *mut Display,
        window: Window,
        pid_atom: Atom,
    ) -> Result<Option<u32>, String> {
        let mut actual_type = 0;
        let mut actual_format = 0;
        let mut item_count = 0;
        let mut bytes_after = 0;
        let mut property: *mut c_uchar = ptr::null_mut();

        let status = unsafe {
            XGetWindowProperty(
                display,
                window,
                pid_atom,
                0,
                1,
                0,
                ANY_PROPERTY_TYPE,
                &mut actual_type,
                &mut actual_format,
                &mut item_count,
                &mut bytes_after,
                &mut property,
            )
        };
        if status != 0 || property.is_null() || item_count == 0 {
            if !property.is_null() {
                unsafe {
                    XFree(property as *mut c_void);
                }
            }
            return Ok(None);
        }

        let pid = if actual_format == 32 {
            Some(unsafe { *(property as *const c_ulong) as u32 })
        } else {
            None
        };
        unsafe {
            XFree(property as *mut c_void);
        }
        Ok(pid)
    }

    unsafe fn strip_window_chrome(display: *mut Display, window: Window) {
        let hints_atom = match intern_atom(display, "_MOTIF_WM_HINTS") {
            Ok(atom) => atom,
            Err(_) => return,
        };
        let hints = MotifWmHints {
            flags: MWM_HINTS_DECORATIONS,
            functions: 0,
            decorations: 0,
            input_mode: 0,
            status: 0,
        };
        XChangeProperty(
            display,
            window,
            hints_atom,
            hints_atom,
            32,
            PROP_MODE_REPLACE,
            &hints as *const MotifWmHints as *const c_uchar,
            5,
        );
    }

    fn intern_atom(display: *mut Display, name: &str) -> Result<Atom, String> {
        let name = CString::new(name).map_err(|error| error.to_string())?;
        let atom = unsafe { XInternAtom(display, name.as_ptr(), 0) };
        if atom == 0 {
            return Err("Failed to intern X11 atom".to_string());
        }
        Ok(atom)
    }

    fn scaled_bounds(bounds: &SlicerViewportBounds) -> (c_int, c_int, c_uint, c_uint) {
        let scale = if bounds.scale_factor.is_finite() && bounds.scale_factor > 0.0 {
            bounds.scale_factor
        } else {
            1.0
        };
        let x = (bounds.x * scale).round() as c_int;
        let y = (bounds.y * scale).round() as c_int;
        let width = (bounds.width * scale).round().max(1.0) as c_uint;
        let height = (bounds.height * scale).round().max(1.0) as c_uint;
        (x, y, width, height)
    }
}

#[cfg(windows)]
mod windows_impl {
    use std::{thread, time::Duration};

    use super::{PlatformWindowResult, SlicerViewportBounds};
    use windows_sys::{
        core::BOOL,
        Win32::{
            Foundation::{HWND, LPARAM},
            UI::WindowsAndMessaging::{
            EnumWindows, GetWindowLongPtrW, GetWindowThreadProcessId, IsWindowVisible, SetParent,
            SetWindowLongPtrW, SetWindowPos, ShowWindow, GWL_STYLE, SWP_FRAMECHANGED, SWP_NOZORDER,
            SW_HIDE, SW_SHOW, WS_CAPTION, WS_CHILD, WS_MAXIMIZEBOX, WS_MINIMIZEBOX,
             WS_OVERLAPPEDWINDOW, WS_THICKFRAME,
            },
        },
    };

    pub fn attach_orca_window(
        window: &tauri::WebviewWindow,
        process_id: u32,
        cached_window_handle: Option<i64>,
        bounds: &SlicerViewportBounds,
    ) -> Result<PlatformWindowResult, String> {
        let parent = window.hwnd().map_err(|error| error.to_string())?.0 as HWND;
        let child = match cached_window_handle {
            Some(handle) => handle as HWND,
            None => wait_for_process_window(process_id)?,
        };

        unsafe {
            SetParent(child, parent);
            strip_window_chrome(child);
            move_child_window(child, bounds)?;
            ShowWindow(child, SW_SHOW);
        }

        Ok(PlatformWindowResult {
            status: "embedded".to_string(),
            message: "OrcaSlicer window embedded in the Slicer view.".to_string(),
            window_handle: Some(child as i64),
            display_handle: None,
        })
    }

    pub fn resize_orca_window(
        window_handle: Option<i64>,
        bounds: &SlicerViewportBounds,
    ) -> Result<PlatformWindowResult, String> {
        let Some(handle) = window_handle else {
            return Err("No cached OrcaSlicer window handle is available".to_string());
        };
        unsafe {
            move_child_window(handle as HWND, bounds)?;
        }
        Ok(PlatformWindowResult {
            status: "embedded".to_string(),
            message: "OrcaSlicer window resized to the Slicer view.".to_string(),
            window_handle,
            display_handle: None,
        })
    }

    pub fn hide_orca_window(window_handle: Option<i64>) -> Result<PlatformWindowResult, String> {
        let Some(handle) = window_handle else {
            return Err("No cached OrcaSlicer window handle is available".to_string());
        };
        unsafe {
            ShowWindow(handle as HWND, SW_HIDE);
        }
        Ok(PlatformWindowResult {
            status: "hidden".to_string(),
            message: "OrcaSlicer window hidden; process state preserved.".to_string(),
            window_handle,
            display_handle: None,
        })
    }

    unsafe fn strip_window_chrome(hwnd: HWND) {
        let style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        let stripped = (style as u32
            & !(WS_OVERLAPPEDWINDOW
                | WS_CAPTION
                | WS_THICKFRAME
                | WS_MINIMIZEBOX
                | WS_MAXIMIZEBOX))
            | WS_CHILD;
        SetWindowLongPtrW(hwnd, GWL_STYLE, stripped as isize);
    }

    unsafe fn move_child_window(hwnd: HWND, bounds: &SlicerViewportBounds) -> Result<(), String> {
        let scale = if bounds.scale_factor.is_finite() && bounds.scale_factor > 0.0 {
            bounds.scale_factor
        } else {
            1.0
        };
        let x = (bounds.x * scale).round() as i32;
        let y = (bounds.y * scale).round() as i32;
        let width = (bounds.width * scale).round().max(1.0) as i32;
        let height = (bounds.height * scale).round().max(1.0) as i32;
        let ok = SetWindowPos(
            hwnd,
            std::ptr::null_mut(),
            x,
            y,
            width,
            height,
            SWP_NOZORDER | SWP_FRAMECHANGED,
        );
        if ok == 0 {
            return Err("SetWindowPos failed for OrcaSlicer window".to_string());
        }
        Ok(())
    }

    fn wait_for_process_window(process_id: u32) -> Result<HWND, String> {
        for _ in 0..80 {
            if let Some(hwnd) = find_process_window(process_id) {
                return Ok(hwnd);
            }
            thread::sleep(Duration::from_millis(125));
        }
        Err("Timed out waiting for OrcaSlicer to create a visible window".to_string())
    }

    fn find_process_window(process_id: u32) -> Option<HWND> {
        let mut search = WindowSearch {
            process_id,
            window: std::ptr::null_mut(),
        };
        unsafe {
            EnumWindows(
                Some(enum_windows_proc),
                &mut search as *mut WindowSearch as LPARAM,
            );
        }
        if search.window.is_null() {
            None
        } else {
            Some(search.window)
        }
    }

    struct WindowSearch {
        process_id: u32,
        window: HWND,
    }

    unsafe extern "system" fn enum_windows_proc(hwnd: HWND, lparam: LPARAM) -> BOOL {
        let search = &mut *(lparam as *mut WindowSearch);
        let mut window_process_id = 0;
        GetWindowThreadProcessId(hwnd, &mut window_process_id);
        if window_process_id == search.process_id && IsWindowVisible(hwnd) != 0 {
            search.window = hwnd;
            return 0;
        }
        1
    }
}
