# PolySmith

PolySmith is a local-first desktop CAD application for hobbyists who want a clean, modern workflow for designing 3D-printable parts.

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

## Prerequisites

Before running PolySmith locally, make sure you have:

- `pnpm` 9.x
- CMake 3.20 or newer
- A working C++ toolchain
- Rust toolchain for Tauri
- Tauri system prerequisites for your platform

Current macOS note:

- `brew install freetype`

If Tauri system dependencies are missing, install the official prerequisites for Tauri v2 before running the desktop app.

## First-Time Setup

You can use the all-in-one bootstrap script:

```bash
pnpm bootstrap
```

That script performs these steps:

```bash
pnpm deps:sync
pnpm install
pnpm occt:configure
pnpm occt:build
pnpm occt:install
pnpm core:configure
pnpm core:build
```

If you want to run setup manually, use those commands in that order.

What this does:

- syncs git submodules
- installs JavaScript dependencies
- configures, builds, and installs OpenCascade into `third_party/occt-install`
- configures and builds the native `cad_core` executable in `native/cad-core/build`

## Local Development

### Run the desktop app

```bash
pnpm dev
```

This starts the Vite frontend and the Tauri desktop app together.

Important:

- `pnpm dev` expects the native `cad_core` binary to already exist at `native/cad-core/build/cad_core`
- if the CAD core has not been built yet, run the first-time setup steps first

### Run the UI only

```bash
pnpm ui:dev
```

Use this when working on frontend-only changes. This does not launch the Tauri shell or start the native CAD core.

### Rebuild the native CAD core

If you change C++ code in `native/cad-core`, rebuild it with:

```bash
pnpm core:rebuild
```

If OpenCascade needs to be rebuilt or reinstalled, use:

```bash
pnpm occt:rebuild
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
