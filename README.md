# PolySmith

PolySmith is a local-first desktop CAD application for hobbyists who want a clean, modern workflow for designing 3D-printable parts.

---

## 🚀 Local Development

> **Heads-up:** PolySmith bundles a vendored OpenCascade source tree as a git submodule. Always clone with submodules and bootstrap before running the app, otherwise the native CAD core will fail to build.

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/<your-org>/polysmith.git
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
brew install pnpm cmake freetype

# Rust toolchain (for Tauri)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

#### Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake git pkg-config \
  libfreetype6-dev libfontconfig1-dev \
  libgtk-3-dev libwebkit2gtk-4.1-dev \
  libayatana-appindicator3-dev librsvg2-dev \
  libssl-dev curl

# Node + pnpm
curl -fsSL https://get.pnpm.io/install.sh | sh -

# Rust toolchain (for Tauri)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

For other distributions, follow the [Tauri v2 prerequisites guide](https://v2.tauri.app/start/prerequisites/) and make sure CMake, a C++20 compiler, and FreeType development headers are present.

#### Windows

1. Install **Visual Studio 2022** with the _Desktop development with C++_ workload (provides MSVC + Windows SDK + CMake).
2. Install **Rust** via [rustup-init.exe](https://rustup.rs/) and select the `stable-x86_64-pc-windows-msvc` toolchain.
3. Install **Node.js 20** and enable Corepack:
   ```powershell
   corepack enable
   corepack prepare pnpm@latest --activate
   ```
4. Install **WebView2 Runtime** (Tauri requirement) — pre-installed on Windows 11.

Run all PolySmith commands from the **x64 Native Tools Command Prompt for VS 2022** so MSVC is on `PATH`.

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

## V1 Focus

PolySmith v1 is intentionally narrow:

- Single-part parametric modeling
- Desktop-first, offline-first workflows
- A familiar experience for users coming from Fusion 360
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

docs/
  architecture/    System design and boundaries
  decisions/       Architecture decision records
  prompts/         AI workflow rules and templates
  roadmap/         Near-term product and implementation roadmap

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

## Docs Index

- [Architecture Overview](docs/architecture/overview.md)
- [IPC Protocol](docs/architecture/ipc-protocol.md)
- [Repository Map](docs/architecture/repo-map.md)
- [V1 Roadmap](docs/roadmap/v1-roadmap.md)
- [ADR 0001: Initial Tech Stack](docs/decisions/0001-tech-stack.md)

## License

TBD
