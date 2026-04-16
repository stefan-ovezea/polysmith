# Architecture Overview

PolySmith is a local-first desktop CAD application built with a strict separation between UI and modeling logic.

For near-term implementation priorities and milestones, see `docs/roadmap/v1-roadmap.md`.

## High-Level Architecture

PolySmith consists of three main parts:

1. UI Layer (React + TypeScript)
2. Desktop Shell (Tauri)
3. CAD Core (C++ + OpenCascade)

These components communicate through a defined IPC protocol.

~~~
+----------------------+
|   React UI (TS)      |
|----------------------|
| - Panels             |
| - Toolbars           |
| - History Tree       |
| - Command Input      |
+----------+-----------+
           |
           | IPC (JSON)
           |
+----------v-----------+
|   CAD Core (C++)     |
|----------------------|
| - Geometry (OCCT)    |
| - Feature History    |
| - Modeling Ops       |
| - Document State     |
+----------------------+
~~~

Tauri acts as the bridge between the UI and the native environment.

## Responsibilities

### UI Layer (React)

Responsible for:
- Rendering the interface
- Capturing user input
- Sending commands to the CAD core
- Displaying results and state

NOT responsible for:
- Geometry calculations
- Modeling logic
- CAD state management

---

### CAD Core (C++)

Responsible for:
- All geometry and modeling operations
- Feature history (parametric model)
- Document state
- Recompute logic
- Import/export

This is the source of truth for all CAD data.

---

### IPC Layer

Responsible for:
- Passing commands from UI → core
- Returning results/events from core → UI
- Enforcing a strict contract between systems

---

## Core Principle

React owns presentation and user intent.  
The CAD core owns state, geometry, and behavior.

This separation must never be violated.

---

## Process Model

The CAD core runs as a separate native process.

Benefits:
- Crash isolation
- Clear boundaries
- Easier debugging
- Future extensibility (scripting, plugins, etc.)

---

## Data Flow

1. User performs an action (e.g. "Extrude")
2. UI sends command via IPC
3. CAD core processes the command
4. CAD core updates model state
5. CAD core emits result/event
6. UI updates representation

---

## Design Goals

- Predictable behavior
- Minimal coupling
- Clear ownership boundaries
- Easy to reason about
- Maintainable by a single developer

---

## Non-Goals

- Shared mutable state between UI and core
- Tight coupling between layers
- Hidden side effects
