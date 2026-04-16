# ADR 0001: Initial Tech Stack

## Status

Accepted

## Context

PolySmith is a local-first desktop CAD application focused on hobbyist 3D-printing workflows.

The project needs:

- a modern and maintainable UI
- a native geometry/modeling core
- a clear architectural boundary
- good long-term maintainability for human and AI-assisted development

## Decision

PolySmith uses the following initial stack:

- UI: React + TypeScript
- Desktop shell: Tauri
- Native core: C++
- Geometry kernel: OpenCascade
- Native build system: CMake
- UI/Core communication: JSON IPC over stdin/stdout
- Frontend state: local React state initially, with lightweight state management if needed later

## Rationale

### React + TypeScript

Chosen for:

- fast UI iteration
- contributor familiarity
- strong ecosystem
- good AI-assisted development ergonomics

React is used for interface only, not CAD logic.

### Tauri

Chosen for:

- desktop-first application model
- lightweight shell
- native integration
- local-first/offline-friendly design

### C++

Chosen for:

- direct compatibility with OpenCascade
- performance-sensitive native workloads
- long-term suitability for CAD and geometry systems

### OpenCascade

Chosen for:

- established open-source geometry kernel
- modeling and data exchange capabilities
- better long-term foundation than building a CAD kernel from scratch

### JSON IPC over stdin/stdout

Chosen for:

- simplicity
- inspectability
- language-agnostic boundary
- easy debugging during early development

This may evolve later, but it is the correct bootstrap transport.

## Consequences

### Positive

- strong separation between UI and CAD core
- maintainable architecture
- easy to debug protocol traffic
- lower risk of state leakage into the frontend

### Negative

- more boilerplate than a tightly coupled app
- IPC layer must be maintained deliberately
- initial process integration is more work than direct bindings

## Explicit Non-Decisions

Not chosen at bootstrap:

- WASM-first core
- browser-first app delivery
- FreeCAD fork as the main foundation
- Electron
- plugin system
- cloud dependencies
