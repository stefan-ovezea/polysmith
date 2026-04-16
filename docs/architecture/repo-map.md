# Repository Map

This document describes the purpose of each top-level directory in PolySmith.

## Top-Level Structure

```text
apps/
native/
protocol/
docs/
third_party/
```

## apps/

Contains user-facing application code.

### `apps/desktop-ui`

React + TypeScript + Tauri desktop application.

Responsibilities:

- Render the application UI
- Capture user input
- Send commands to the CAD core
- Receive and display core events

The UI does not own CAD state.

---

## native/

Contains native code and performance-sensitive systems.

### `native/cad-core`

C++ CAD core.

Responsibilities:

- CAD state
- document state
- feature history
- modeling operations
- geometry ownership
- import/export
- recompute behavior

This is the source of truth for the model.

---

## protocol/

Shared communication contracts between UI and core.

### `protocol/schema`

JSON schema and protocol documentation.

Responsibilities:

- define command shapes
- define event shapes
- define error message shapes
- enforce clear communication boundaries

---

## docs/

Project documentation.

### `docs/architecture`

System design and structure.

### `docs/decisions`

Architecture decision records (ADRs).

### `docs/prompts`

AI workflow rules and task templates.

---

## third_party/

Vendored or externally built dependencies.

Examples:

- OpenCascade
- nlohmann/json
- future geometry or helper libraries

Do not mix application code into this directory.

---

## Ownership Rule

React owns presentation and user intent.

The native CAD core owns:

- CAD state
- geometry state
- modeling behavior
- document lifecycle
