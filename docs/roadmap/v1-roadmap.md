# PolySmith V1 Roadmap

## Project Focus

PolySmith v1 is focused on a narrow, shippable product:

- local-first desktop CAD
- hobbyist 3D-printing workflows
- single-part parametric modeling
- a clean, modern workflow inspired by Fusion 360

This roadmap intentionally avoids expanding scope into CAM, cloud collaboration, simulation, enterprise features, or complex assemblies.

## Current Repo Status

The repository already includes the initial project skeleton:

- a React + TypeScript UI shell
- a Tauri desktop host
- a C++ CAD core built with CMake
- OpenCascade integrated well enough for a basic smoke test
- a minimal JSON IPC handshake between UI and core

This is a solid starting point, but it is still infrastructure-heavy and feature-light. The current codebase proves the process boundary and build setup more than it proves the CAD application model.

## Immediate Architectural Priorities

The next phase should focus on foundational pieces that make later feature work reliable:

- Stronger IPC contract
  - keep protocol traffic structured and explicit
  - separate machine-readable messages from human-readable logs
- Document lifecycle
  - create, identify, query, and manage a document in the native core
- Core-owned state model
  - make the CAD core the clear source of truth for document and feature state
- Save/load baseline
  - establish a local-first persistence strategy early
- Tests at the protocol boundary
  - verify that UI, Tauri, and core agree on message shapes and lifecycle behavior

## Milestone Roadmap

### Milestone 0: Foundation Hardening

- tighten the IPC contract and document its first real rules
- ensure protocol messages are structured and predictable
- introduce basic validation at the boundary
- add a minimal document model in the native core
- improve the desktop shell so it renders structured state instead of raw debug text

### Milestone 1: Document and History Skeleton

- support document creation and state retrieval through IPC
- define the initial feature history structure
- represent document state clearly in the UI
- keep the viewport simple while state flow and ownership are being established

### Milestone 2: First End-to-End Modeling Feature

- implement the first real modeling command through the full stack
- keep the feature history in the native core
- ensure recompute behavior is explicit and understandable
- validate error handling for invalid user input

### Milestone 3: Core Hobbyist Workflow

- expand from bootstrap geometry to a useful small set of modeling features
- support parameter editing of existing features
- add undo/redo at the core command/history layer
- add local save/load and STL export

### Milestone 4: Usable V1 Desktop Experience

- improve selection, inspection, and parameter editing workflows
- strengthen persistence and recovery behavior
- improve packaging and local development reliability
- make the app usable for designing simple printable parts without relying on debug affordances

## Key Decisions and Constraints

- The UI does not own CAD state
- Tauri acts as the bridge between UI and native systems, not as a second CAD logic layer
- The IPC protocol is the contract of the system
- V1 stays single-part and local-first
- Changes should remain minimal, readable, and reviewable
- Broad rewrites should be avoided unless clearly justified

## Near-Term Recommended Next Task

The recommended next implementation task is:

Document lifecycle plus strict protocol cleanup.

That work should include:

- documenting and enforcing structured IPC rules
- ensuring protocol JSON is isolated from human-readable logs
- adding `create_document` and `get_document_state` as the first meaningful protocol slice
- introducing a minimal native document model
- updating schemas, docs, and TypeScript/C++ message definitions together

This is the smallest next step that strengthens the architecture while also moving PolySmith toward a usable CAD application foundation.
