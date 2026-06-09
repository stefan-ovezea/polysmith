# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PolySmith is a local-first desktop CAD application for hobbyist 3D-printing workflows — single-part parametric modeling with a strict UI/CAD boundary. Licensed AGPL-3.0-or-later.

## Build & Development Commands

```bash
# First-time bootstrap (submodules → JS deps → OCCT → CAD core)
pnpm bootstrap

# Run the desktop app (Vite + Tauri + CAD core)
pnpm dev

# Run UI only (no Tauri, no CAD core — browser dev)
pnpm ui:dev

# Rebuild the C++ CAD core after changes
pnpm core:rebuild                    # configure + build
pnpm core:build                      # build only (if CMake cache is current)

# Rebuild OpenCascade (rarely needed)
pnpm occt:rebuild

# Type-check the UI
pnpm --filter desktop-ui exec tsc --noEmit

# Release build (produces .app / .exe)
pnpm build:release
```

**Prerequisites:** pnpm 9.x, Node 20.x, Rust stable, CMake 3.20+, C++20 compiler. Linux also needs GTK3/WebKit2GTK dev headers. Windows needs Visual Studio 2022 with C++ workload and vcpkg for Boost + Eigen3.

## Architecture

Three layers with strict ownership boundaries:

| Layer | Tech | Owns |
|---|---|---|
| **UI** | React + TypeScript (Vite) | Presentation, user input, command dispatch |
| **Shell** | Tauri (Rust) | Window management, file dialogs, spawning the C++ core process |
| **CAD Core** | C++20 + OpenCascade 7.8 | All geometry, feature history, document state, modeling operations |

The CAD core runs as a **separate process** spawned by Tauri. Communication is via newline-delimited JSON over `stdin`/`stdout`. The core is the single source of truth; React never owns CAD state.

```
React UI  ──IPC (JSON)──>  Tauri (Rust)  ──stdin/stdout──>  C++ CAD Core
  (TS)       <──          spawns/manages      <──            (OCCT)
```

### Key Architectural Rules

- **React owns presentation and user intent only.** CAD state, geometry, feature history, and modeling logic live ONLY in the native C++ core.
- **All communication goes through the IPC protocol.** Never bypass it. Protocol schemas live in `protocol/schema/`.
- **Core sends DOCUMENT STATE, not INTERACTION STATE.** If it moves with the mouse, it's UI. If it saves to a file, it's core. Snap candidates, drag previews, and hover highlights are UI-side concerns.
- **TNP (Topological Naming Problem) is the project's mantra:** Never store a naked OCCT topology index and trust it across recomputes. Every feature referencing 3D geometry must re-resolve against live body shapes on every recompute. On failure, degrade with `dependency_broken` + warning — never crash.
- **Contextual modeling workflow** is the binding UX pattern for all features: select inputs → invoke action → floating context panel with real geometry preview → confirm (Enter) or cancel (Escape, with undo).

## Repository Layout

```
apps/desktop-ui/           React + TypeScript + Tauri desktop app
  src/
    app/                   Feature actions, lifecycle, tool logic, panel components
    layout/                Layout components (panels, toolbars, viewport)
    layout/viewport/       Viewport rendering (Three.js), draft previews, snap, selection
    state/                 Zustand store (cadCoreStore.ts)
    hooks/                 React hooks for core bridge, event handling
    lib/                   IPC protocol helpers, schema parsing, core client
    types/                 TypeScript types (IPC, geometry, viewport, scene)
    config/                App config, theme JSON files
    i18n/                  Translation strings (en.json)
  src-tauri/
    src/                   Rust: core process management, protocol bridge, OrcaSlicer integration
    Cargo.toml
native/cad-core/           C++ CAD core (CMake project)
  src/
    app.cpp/h              Main event loop, command dispatch
    protocol/              IPC JSON parsing, event construction
    core/
      document/            Document model, feature tree, undo/redo
      sketch/              Sketch geometry, constraints (planegcs solver), profiles, inference
      geometry/            Body compilation, edge/face geometry, feature shapes
      extrude/             Extrude feature
      revolve/             Revolve feature
      loft/ / sweep/       Loft and sweep features
      primitive/           Box and cylinder primitives
      construction/        Construction planes
      cam/                 CAM operations (face milling)
      viewport/            Viewport state generation (mesh data, primitives)
      export/              STEP/STL export
      diagnostics/         Structured logging
  tests/                   C++ test executables
protocol/schema/           IPC JSON schemas (commands, events)
wiki/                      Canonical documentation (mirrored to polysmith.wiki git submodule)
third_party/
  occt/                    Vendored OpenCascade 7.8 source (git submodule)
  freetype/                FreeType (git submodule)
  planegcs/                2D geometric constraint solver from FreeCAD (git submodule)
  planegcs-validation/     Planegcs test suite (git submodule)
scripts/                   Build scripts (configure-occt.mjs, build-release.mjs)
```

## IPC Protocol

Commands (UI → Core) and events (Core → UI) are newline-delimited JSON messages:

```json
{ "id": "<uuid>", "type": "command_name", "payload": { ... } }
```

- **Commands** arrive on core's stdin, dispatched by `app.cpp` to the appropriate handler.
- **Events** are written to core's stdout, picked up by Tauri's stdout reader thread, and emitted as Tauri events (`cad-core-event`).
- **Structured logs** from the core go to stderr; Tauri forwards them as `cad-core-log` events.
- Every command gets a response event (success/error/state update). Commands are async — the UI sends and waits for the matching event by `id`.
- Protocol schemas live in `protocol/schema/commands.schema.json` and `protocol/schema/events.schema.json`.
- When changing IPC messages, also update `wiki/IPC-Protocol.md` and `wiki/AI-CAD-Command-Language.md`.

## Testing

### C++ Tests

Tests are standalone executables built by CMake. Run from the build directory:

```bash
cd native/cad-core/build
./cad_core_sketch_profile_test
./cad_core_multi_profile_extrude_test
./cad_core_cam_face_reference_test
```

Rebuild tests with `pnpm core:rebuild` (they link against the full CAD core + OCCT).

### TypeScript

No formal test runner is configured yet. Type-check with:
```bash
pnpm --filter desktop-ui exec tsc --noEmit
```

## Key Dependencies

- **OpenCascade 7.8** — geometry kernel, vendored as a git submodule at `third_party/occt/`
- **planegcs** — 2D geometric constraint solver (ported from FreeCAD), built as a static library linked into the CAD core
- **Eigen3** — linear algebra (used by planegcs)
- **Boost** — graph and math libraries (used by planegcs constraint solving)
- **nlohmann/json** — JSON parsing in the C++ core (header-only, vendored)
- **Three.js** — WebGL viewport rendering in the UI
- **Zustand** — React state management
- **Tauri v2** — desktop shell

## Cross-Platform

Must compile on Windows (MSVC) and POSIX (Linux/macOS, GCC/Clang).

C++ notes:
- `M_PI` is not standard — define `_USE_MATH_DEFINES` before `<cmath>` on MSVC.
- MSVC treats `const char*` ↔ `unsigned char*` as an error. Match types exactly.
- OCCT DLLs on Windows live at `third_party/occt-install/win64/vc14/bin`. Tauri prepends this to `PATH` when spawning the core.

## Debugging

- This is a **Tauri desktop app** — there is no F12 DevTools console.
- `console.log` output is captured and discarded by Tauri. **Never use `console.log` for debugging.**
- Use `addMessage("...")` from `useCadCoreStore` to emit messages to the in-app **Logs panel** (toolbar icon). Structured `LogEntry` objects can be sent via `addLogEntry(entry)`.
- The CAD core writes structured logs to stderr (format: `[timestamp] [level] [source] message`). Tauri forwards unrecognized stderr lines as `cad-core-log` events.

## Wiki Documentation

All project documentation lives in `wiki/` and is mirrored to the GitHub wiki submodule at `polysmith.wiki/`. Edits go to `wiki/` first, then mirrored.

**After any edit to `wiki/`, sync the mirror:**
```bash
rsync -av --delete wiki/ polysmith.wiki/ --exclude .git
```
Then verify with:
```bash
diff -r wiki/ polysmith.wiki/ --exclude='.git' && echo "IDENTICAL"
```
Both directories must stay identical. Adding a new page to `wiki/` means also adding it to the mirror. Deleting from `wiki/` means also deleting from the mirror.

Key pages for understanding the system:
- `wiki/Architecture-Overview.md` — system layout
- `wiki/Core-UI-Design-Principles.md` — what belongs in core vs. UI (READ FIRST)
- `wiki/Contextual-Modeling-Workflow.md` — binding UX pattern
- `wiki/IPC-Protocol.md` — communication contract
- `wiki/Topological-Naming-Problem.md` — TNP strategy
- `wiki/Repository-Map.md` — directory layout
- `wiki/Implementation-Log.md` — shipped features and build fixes

## Branch Workflow

- `dev` is the default development branch; `main` is production/stable.
- Feature branches from latest `dev`, squash-merge back via PR.
- Git push/pull/fetch require user permission — never run them autonomously.
- Prefer `gh` CLI for PR operations when available.
