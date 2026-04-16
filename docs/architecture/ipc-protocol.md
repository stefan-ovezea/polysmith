# IPC Protocol

PolySmith uses a JSON-based IPC protocol to communicate between the UI (React) and the CAD core (C++).

This document describes the architectural rules of that protocol. It should stay focused on contract and transport behavior, not feature planning. For near-term milestones, see the roadmap document.

## Goals

- clear separation between UI and core
- stable contract between components
- easy debugging without weakening boundaries
- language-agnostic communication
- predictable behavior for a solo-developer codebase

## Core Rules

- The UI sends commands that represent user intent
- The CAD core owns document state, geometry, feature history, and modeling behavior
- Tauri acts as the bridge between the UI and the native CAD core
- All cross-boundary communication must go through the IPC protocol
- No shared memory or direct bindings between UI and CAD logic

## Transport

Initial transport:

- `stdin` for commands sent to the CAD core
- `stdout` for protocol messages emitted by the CAD core
- `stderr` for human-readable logs and diagnostics

Protocol rule:

- `stdout` is reserved for newline-delimited JSON protocol messages only
- human-readable logs must go to `stderr`, never `stdout`

This distinction keeps protocol parsing reliable and makes debugging easier without weakening the contract.

## Message Structure

All protocol messages follow a common base shape.

~~~json
{
  "id": "string",
  "type": "string",
  "payload": {}
}
~~~

- `id` is used for request/response matching when applicable
- `type` identifies the command, event, or error
- `payload` contains the message-specific data

Not every message must include every field, but every message type must be documented and schema-backed.

## Commands (UI -> Core)

Commands represent explicit user intent.

Example:

~~~json
{
  "id": "123",
  "type": "ping",
  "payload": {}
}
~~~

Command rules:

- commands must be explicit and self-contained
- commands must not rely on hidden UI-side state
- every command type must be documented
- every command type must be represented in schema

## Events and Responses (Core -> UI)

The core replies with structured protocol messages.

Example:

~~~json
{
  "id": "123",
  "type": "pong",
  "payload": {
    "version": "0.1.0"
  }
}
~~~

Response rules:

- every handled command should produce at least one response or error
- responses tied to a command should include the original `id`
- the core may also emit independent events such as lifecycle or state updates
- response and event types must be documented and schema-backed

## Error Handling

Errors must be explicit protocol messages, not implied by missing output or mixed into free-form logs.

Example:

~~~json
{
  "id": "123",
  "type": "error",
  "payload": {
    "message": "Invalid command",
    "code": "INVALID_COMMAND"
  }
}
~~~

Error rules:

- errors should use a documented error type and payload shape
- invalid input should produce structured protocol errors
- logs may provide extra debugging detail, but protocol consumers must not depend on log text

## Lifecycle

### Startup

When the CAD core starts successfully, it should emit a `hello` message describing the service and version.

Example:

~~~json
{
  "type": "hello",
  "payload": {
    "service": "cad_core",
    "version": "0.1.0"
  }
}
~~~

### Shutdown

The UI requests shutdown through a documented protocol command.

Example:

~~~json
{
  "type": "shutdown"
}
~~~

The core should exit gracefully after handling the shutdown request.

## Schema and Validation

The schema files under `protocol/schema/` are the source of truth for message shape.

That means:

- new message types should be added to schema and docs together
- UI-side message handling should validate incoming core messages at the boundary
- Tauri bridge code should preserve the protocol cleanly and avoid undocumented reshaping
- core-side command handling should validate and reject malformed input explicitly

Planned message types may be documented ahead of implementation, but they must be clearly treated as planned until the code supports them.

## Initial Required Foundation Message Set

The first meaningful protocol slice for PolySmith foundation work should include:

- `hello`
- `ping`
- `shutdown`
- `create_document`
- `get_document_state`
- `error`

These message types are the minimum needed to move from process bootstrap to real document lifecycle work.

The current implementation may extend beyond that minimum slice as small feature-oriented commands are added. Those additions should still follow the same rules:

- document the message type
- update schema and code together
- keep modeling behavior in the native core

A viewport snapshot follows the same rule set. The core decides what renderable scene data exists, and the UI only visualizes that snapshot.

For renderer-oriented viewport data, the same ownership rule still applies:

- the core may provide primitive placement, centers, and scene bounds when that helps visualization
- the core may provide reference geometry such as origin planes and axes when those are selectable CAD targets
- the core may provide active sketch state and renderable sketch entities when sketching is in progress
- the UI may adapt that snapshot for a renderer, but it must not invent CAD state or modeling behavior

Sketch commands follow the same ownership boundary:

- the UI may send sketch intent such as `start_sketch_on_plane`, `set_sketch_tool`, `add_sketch_line`, `add_sketch_rectangle`, `add_sketch_circle`, `select_sketch_entity`, or `finish_sketch`
- the core owns the active sketch, active sketch tool including non-drawing selection mode, selected sketch entity, stored sketch entities, and their serialized viewport representation

## Versioning

- protocol versioning must be tracked deliberately
- breaking protocol changes require a version bump
- UI and core must agree on protocol version

## Logging and Debugging

- protocol traffic should be easy to inspect during development
- structured messages should remain machine-readable in all environments
- logs should help developers, but they must never become part of the contract

## Philosophy

The IPC protocol is the contract of the system.

If the protocol stays clean:

- the architecture stays clean
- the UI stays focused on presentation and user intent
- the core stays responsible for CAD behavior
- the codebase stays understandable and maintainable
