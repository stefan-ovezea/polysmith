# PolySmith

**What it is:** A local-first desktop CAD application for hobbyist 3D-printing workflows. Single-part parametric modeling, built with a strict UI/CAD boundary.

## Architecture

Three layers with clear ownership:

| Layer | Tech | Owns |
|---|---|---|
| **UI** | React + TypeScript | Presentation, user input, command dispatch |
| **Shell** | Tauri (Rust) | Window management, file dialogs, spawning the C++ core |
| **CAD Core** | C++ + OpenCascade | All geometry, feature history, document state, modeling operations |

Communication is via a JSON IPC protocol over `stdin`/`stdout`. The CAD core is a **separate process** — crash isolation, clean boundaries. The core is the single source of truth; React never owns CAD state.

## What's Built (shipped)

- JSON IPC bridge with versioned schema
- Document model: feature history tree, undo/redo, core-owned selection
- 2D sketch system: lines, rectangles, circles, arcs, points, dimensions, constraints (H/V, coincident, parallel, perpendicular, equal-length, tangent), closed-profile detection
- Contextual modeling workflow: select → invoke → floating panel → real geometry preview → confirm/cancel
- Extrude (new body) with live depth editing
- Edge/vertex selection, **Fillet & Chamfer** on edges with live preview panels
- Sketches on origin planes or solid faces; sketch re-entry
- 2D sketch fillets (line-line corners)
- Project tool (face/edge/vertex projection into active sketch)
- Save/load `.polysmith` documents (core-owned JSON format)
- STEP + STL export
- OrcaSlicer integration (native window embedding via X11/XWayland)
- Offset construction planes
- Unified selection filter panel: selection, snapping, and constraints controlled by one checkbox panel
- Comprehensive snap system (endpoint, midpoint, center, quadrant, intersection, nearest, tangent, perpendicular, parallel, polar, grid)
- Sketches on offset planes from faces/reference planes

## The Project's Mantra: Topological Naming Problem (TNP)

"Never store a naked OCCT topology index and trust it across recomputes." Every feature that references 3D geometry must **re-resolve** its references against live body shapes on every recompute. When resolution fails, degrade gracefully (`dependency_broken` + warning), never crash. This is handled for edges (fillet/chamfer), faces (construction planes, face-based sketches), and sketch projections.

## Binding UX Pattern: Contextual Modeling Workflow

Every modeling feature follows: **select inputs → invoke action → floating context panel → real geometry preview (core-recomputed) → confirm or cancel.** No fake previews, no modal dialogs. Enter confirms, Escape cancels with `undo`.

## What's Next (V1 Roadmap)

The project is roughly mid-way through Milestone 3. The next big items:

1. **Cut/subtract extrude** — needs a new viewport mesh primitive for boolean'd bodies (single largest UX unlock)
2. **Hole feature** — simple/counterbore/countersink on a face
3. **Pattern features** — linear and circular
4. **Mirror** — body/feature mirror about a plane
5. **Measure tool**, named parameters, view cube, display units toggle

## Cross-Platform

Must compile on Windows (MSVC) and POSIX (Linux/macOS, GCC/Clang). OCCT DLLs vendored, FreeType as git submodule.

## Rules for Contributions

- React does NOT own CAD state
- IPC protocol is the contract — schema, types, C++ dispatch, and docs move together
- All features follow the contextual modeling workflow
- Live previews are real geometry from the core
- Changes stay minimal and scoped — no vibe-coded rewrites
- `dev` is the default branch; feature branches from latest `dev`

---

## 🚀 Local Development

> **Heads-up:** PolySmith bundles a vendored OpenCascade source tree as a git submodule. Always clone with submodules and bootstrap before running the app, otherwise the native CAD core will fail to build.

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/stefan-ovezea/polysmith.git
cd polysmith
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

### 2. Install prerequisites

PolySmith needs a JavaScript toolchain, a Rust toolchain (for Tauri), a C++ toolchain, and CMake.

| Tool   | Minimum version          |
| ------ | ------------------------ |
| `pnpm` | 9.x                      |
| `node` | 20.x                     |
| Rust   | stable (`rustup` latest) |
| CMake  | 3.20 or newer            |
| C++    | C++20-capable compiler   |

Install them on your platform:

#### macOS

```bash
# Xcode command-line tools (clang + make)
xcode-select --install

# Homebrew dependencies
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.4/install.sh | bash
nvm install 24
corepack enable pnpm

# Rust toolchain (for Tauri)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

#### Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake git pkg-config \
  libeigen3-dev \
  libboost-dev \
  libfreetype6-dev libfontconfig1-dev \
  libgtk-3-dev libwebkit2gtk-4.1-dev \
  libayatana-appindicator3-dev librsvg2-dev \
  libssl-dev curl libjavascriptcoregtk-4.1-dev \
  libsoup-3.0-dev \
sudo apt install -y tcl-dev tk-dev libfreetype-dev libx11-dev

# Node + pnpm
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.4/install.sh | bash
nvm install 24
corepack enable pnpm

# Rust toolchain (for Tauri)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

For other distributions, follow the [Tauri v2 prerequisites guide](https://v2.tauri.app/start/prerequisites/) and make sure CMake, a C++20 compiler, and FreeType development headers are present.

#### Windows

1. Install **Visual Studio 2022** with the _Desktop development with C++_ workload (provides MSVC + Windows SDK + CMake).
2. Install **Rust** via [rustup-init.exe](https://rustup.rs/) and select the `stable-x86_64-pc-windows-msvc` toolchain.
3. Install C++ library dependencies via **vcpkg**:
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   .\vcpkg install boost eigen3
   ```
   Both are header-only for PolySmith's needs (the geometric constraint solver uses Boost.Graph and Boost.Math; Eigen3 for linear algebra).
4. Install **Node.js 20** and enable Corepack:
   ```powershell
   powershell -c "irm https://community.chocolatey.org/install.ps1|iex"
   choco install nodejs --version="24.15.0"
   corepack enable pnpm
   corepack prepare pnpm@latest --activate
   ```
5. Install **WebView2 Runtime** (Tauri requirement) — pre-installed on Windows 11.

Run all PolySmith commands from the **x64 Native Tools Command Prompt for VS 2022** so MSVC is on `PATH`. The CMake configure step (`pnpm core:configure`) needs the vcpkg toolchain — pass it manually on first configure:

```powershell
cmake -S native/cad-core -B native/cad-core/build `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 3. Bootstrap (first-time only)

The first build compiles OpenCascade locally, so it can take 10–30 minutes depending on your machine. You only need to do this once.

```bash
pnpm bootstrap
```

This single command runs:

```bash
pnpm deps:sync         # sync git submodules
pnpm install           # install JS deps
pnpm occt:configure    # configure OpenCascade
pnpm occt:build        # build OpenCascade
pnpm occt:install      # install OpenCascade to third_party/occt-install
pnpm core:configure    # configure native CAD core
pnpm core:build        # build native CAD core (native/cad-core/build/cad_core)
```

You can run those steps individually if a single phase fails and you want to retry from there.

### 4. Run the desktop app

```bash
pnpm dev
```

This starts the Vite frontend and the Tauri desktop shell. `pnpm dev` expects `native/cad-core/build/cad_core` to already exist — make sure step 3 completed.

### 5. Iterate

| Task                                   | Command                                      |
| -------------------------------------- | -------------------------------------------- |
| Run UI only (no Tauri, no CAD core)    | `pnpm ui:dev`                                |
| Rebuild the C++ CAD core after changes | `pnpm core:rebuild`                          |
| Rebuild OpenCascade (rare)             | `pnpm occt:rebuild`                          |
| Type-check the UI                      | `pnpm --filter desktop-ui exec tsc --noEmit` |

---

## Release Build

After completing the bootstrap step, build a release executable with:

```bash
pnpm build:release
```

Which runs this command:

```bash
cmake -S native/cad-core -B native/cad-core/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build native/cad-core/build-release --config Release
pnpm --filter desktop-ui exec tauri build --bundles app
```

The script copies the release `cad_core` binary into the Tauri resources folder before packaging, so the built app uses the bundled CAD core instead of the workspace development path.

On macOS, the main outputs are:

```text
apps/desktop-ui/src-tauri/target/release/polysmith
apps/desktop-ui/src-tauri/target/release/bundle/macos/polysmith.app
```

---

## V1 Focus

PolySmith v1 is intentionally narrow:

- Single-part parametric modeling
- Desktop-first, offline-first workflows
- A familiar, modern parametric CAD experience
- A strong architecture boundary between UI and native CAD logic

## Non-Goals

PolySmith does not currently aim to support:

- CAM / CNC workflows
- Simulation / FEA
- Cloud collaboration
- Enterprise features
- Complex assemblies

## Architecture Snapshot

PolySmith is built as a desktop application with three main layers:

- UI: React + TypeScript
- Desktop shell: Tauri
- CAD core: C++ + OpenCascade

Communication between the UI and CAD core happens over a JSON IPC protocol.

Architecture rule:

- React owns presentation and user intent only
- The native CAD core owns CAD state, document state, geometry, feature history, and modeling behavior

## Repository Layout

```text
apps/
  desktop-ui/      React + Tauri application

native/
  cad-core/        C++ CAD core built with CMake

protocol/
  schema/          IPC message schemas

wiki/
  polysmith.wiki/  GitHub wiki submodule — all documentation

third_party/
  occt/            Vendored OpenCascade source
```

## Current Status

PolySmith is in early development.

The current focus is:

- hardening the IPC boundary between UI and CAD core
- establishing document lifecycle and core-owned state flow
- building the smallest useful modeling foundation for a narrow v1

At the moment, the repository contains:

- a React + Tauri desktop shell
- a native CAD core bootstrap
- an OpenCascade smoke test
- a minimal IPC handshake and ping flow

## Wiki

All project documentation has moved to the [GitHub wiki](wiki/polysmith.wiki/Home.md).

Key pages:

- [Architecture Overview](wiki/polysmith.wiki/Architecture-Overview.md)
- [IPC Protocol](wiki/polysmith.wiki/IPC-Protocol.md)
- [Repository Map](wiki/polysmith.wiki/Repository-Map.md)
- [V1 Roadmap](wiki/polysmith.wiki/V1-Roadmap.md)
- [ADR 0001: Initial Tech Stack](wiki/polysmith.wiki/ADR-0001-Tech-Stack.md)

## License

PolySmith is licensed under the GNU Affero General Public License v3.0 or later.
See [LICENSE](LICENSE) for the full license text.
