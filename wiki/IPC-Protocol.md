# IPC Protocol

PolySmith uses a JSON-based IPC protocol to communicate between the UI (React) and the CAD core (C++).

This document describes the architectural rules of that protocol. It should stay focused on contract and transport behavior, not feature planning. For near-term milestones, see the roadmap document.

For an AI-oriented command reference that teaches agents how to create CAD
objects through the app, see
[`ai-cad-command-language.md`](./ai-cad-command-language.md).

The in-app AI assistant is a protocol client. It validates model-generated
command envelopes in the UI and then forwards approved commands through the
existing Tauri `send_core_command` path; it does not bypass the IPC protocol or
write directly to a running `cad_core` process.

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

```json
{
  "id": "string",
  "type": "string",
  "payload": {}
}
```

- `id` is used for request/response matching when applicable
- `type` identifies the command, event, or error
- `payload` contains the message-specific data

Not every message must include every field, but every message type must be documented and schema-backed.

## Commands (UI -> Core)

Commands represent explicit user intent.

Example:

```json
{
  "id": "123",
  "type": "ping",
  "payload": {}
}
```

Command rules:

- commands must be explicit and self-contained
- commands must not rely on hidden UI-side state
- every command type must be documented
- every command type must be represented in schema

## Events and Responses (Core -> UI)

The core replies with structured protocol messages.

Example:

```json
{
  "id": "123",
  "type": "pong",
  "payload": {
    "version": "0.1.0"
  }
}
```

Response rules:

- every handled command should produce at least one response or error
- responses tied to a command should include the original `id`
- the core may also emit independent events such as lifecycle or state updates
- response and event types must be documented and schema-backed

## Error Handling

Errors must be explicit protocol messages, not implied by missing output or mixed into free-form logs.

Example:

```json
{
  "id": "123",
  "type": "error",
  "payload": {
    "message": "Invalid command",
    "code": "INVALID_COMMAND"
  }
}
```

Error rules:

- errors should use a documented error type and payload shape
- invalid input should produce structured protocol errors
- logs may provide extra debugging detail, but protocol consumers must not depend on log text

## Logging

The core emits structured logs as protocol events while also writing the
same entries to `stderr` for the native console.

Example:

```json
{
  "type": "log",
  "payload": {
    "level": "error",
    "source": "cad_core",
    "message": "Command payload is missing numeric field 'radius'",
    "timestamp": "2026-05-15T10:30:00Z"
  }
}
```

Log rules:

- `level` is one of `debug`, `info`, `warn`, or `error`
- `source` identifies the subsystem that emitted the log
- `message` is human-readable diagnostic text
- `timestamp` is an ISO-8601 UTC timestamp supplied by the emitter
- UI consumers may collect and display log events, but CAD behavior must not depend on log contents

## Lifecycle

### Startup

When the CAD core starts successfully, it should emit a `hello` message describing the service and version.

Example:

```json
{
  "type": "hello",
  "payload": {
    "service": "cad_core",
    "version": "0.1.0"
  }
}
```

### Shutdown

The UI requests shutdown through a documented protocol command.

Example:

```json
{
  "type": "shutdown"
}
```

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

The current implementation now also includes a focused export boundary:

- the UI may send `export_document` (STEP) or `export_document_stl` (binary STL) with a destination file path
- the CAD core rebuilds exportable solids from core-owned feature history and writes the file
- the UI may send `export_body_stl` with a destination file path and a body id to export one compiled body as binary STL
- the UI may send `export_body_step` with a destination file path and a body id to export one compiled body as STEP (B-rep); mesh-import bodies have no B-rep and are rejected with an error
- user-facing mesh export is body-scoped; the File menu exposes STEP only, while body context menus expose mesh export for the selected body, and the Send to Slicer submenu exports one body as STL or STEP
- the STL exporter triangulates the requested body or compound with a fixed linear/angular deflection before writing; the UI does not generate any tessellation itself
- the core replies with `document_exported` when the export succeeds; the payload's `format` field reflects the writer that ran (`step` or `stl`)
- the UI must not reconstruct geometry or write CAD files itself

The same boundary now imports STL meshes into the live document:

- `import_stl { file_path, scale? }` creates a `mesh_import` body feature and replies with `document_state`. Only the source path and scale are persisted — the core re-reads the STL from disk on every compile; a missing file degrades the feature with `dependency_broken` + a timeline warning instead of crashing.
- `convert_mesh_to_body { body_id }` creates a `mesh_to_body` feature that converts the referenced mesh into a regular solid body alongside it (sew → make solid → heal → merge coplanar facets). The converted solid is snapshotted at creation (independent body copy pattern), so the mesh body can be deleted afterwards without losing it. Requires a watertight mesh; a structured error is returned otherwise.
- `project_body_into_sketch { body_id, mode }` projects a mesh body onto the active sketch plane as fixed-endpoint sketch lines, recorded as a live `SketchProjection` (`source_kind = "body"`). `mode = "section"` intersects the mesh with the sketch plane; `mode = "silhouette"` projects the outline seen along the plane normal (Fusion "Project" semantics). Both modes also work on origin ref-plane sketches.
- `remove_sketch_projections { feature_id, keep_geometry }` heals a sketch's projection clutter. `keep_geometry = false` ("Remove projections") deletes every entity generated by any projection record on the sketch (the records, their lines/circles/arcs/projected points, and referencing dimensions/constraints go too); `keep_geometry = true` ("Unlink projections") keeps the entities and drops only the live links, clearing the projected vertex styling so they render as plain sketch geometry. Both modes clear `projected_sources` (re-projection allowed again) and the `dependency_broken` alarm raised when a partial delete leaves a projection record whose entity count no longer matches its source.
- v1 limitations: curved STL edges project as polylines (no arc recovery); mesh bodies are shells — booleans/fillets/chamfers/holes are gated off them (converted solids get all body ops); STL is assumed mm (the `scale` parameter is available on the command/AI path); meshes above 50k faces skip the coplanar-facet merge when converting.

The same boundary also imports STEP solids:

- `import_step { file_path }` creates a `step_import` body feature and replies with `document_state`. The file is parsed ONCE at import time (unlike `import_stl`, the core never re-reads it): the translated shape (units converted to mm, original unit reported in `parameters_summary`) is kept in memory and a B-rep snapshot is persisted into the saved part file (gated by `include_opaque` — event payloads strip it). The part stays self-contained — moving or deleting the source .step never breaks it. A missing or invalid file throws before any document mutation (parse-before-mutate).
- Multi-solid STEP files (e.g. assemblies) become ONE body holding a compound — the `CompiledBody.id == feature_id` invariant means no per-solid explosion in v1.
- Imported STEP bodies are real solids: booleans, fillets, chamfers, shell and hole all work on them, and they re-export to STEP through the normal `export_document` path. Shells-only files are sewn into solids on import (same sew+orientation normalization as the IGES import); faces that don't sew (open shells) import but solid-only modifiers no-op. v1 limitations: assembly structure, names, colors and layers are not read.

The same boundary handles IGES with identical semantics:

- `import_iges { file_path }` creates an `iges_import` body feature and replies with `document_state` — parse-once, self-contained B-rep snapshot in the saved part file, mm conversion (same `xstep.cascade.unit` static as STEP; the original unit from the IGES global section is reported in `parameters_summary`), one compound body per import, parse-before-mutate on missing/invalid files. Faces-only files (the common real-world IGES shape) are sewn into solids on import with per-solid orientation normalization, so booleans/fillets work on them; faces that don't sew (open shells) import but solid-only modifiers no-op.
- `export_document_iges { file_path }` writes every body as an IGES MSBO (186) solid (BRep write mode) and replies with `document_exported` (`format: "iges"`).

Embedded OrcaSlicer integration uses this same export boundary. Switching to
Slicer view only opens/embeds the configured native OrcaSlicer process. It does
not export the active document. The separate Export to Slicer action is enabled
only when the viewport snapshot contains at least one body, asks the native core
to export the active document as a temporary STL, waits for `document_exported`,
then hands that STL path to the Tauri-native OrcaSlicer lifecycle command:

- `prepare_orca_export_path` returns a temporary STL destination owned by the app
- `embed_orca_window { binaryPath, modelFilePath?, bounds }` launches or reuses the configured OrcaSlicer binary under PolySmith control and attempts to attach the native window to the Slicer view bounds. `modelFilePath` is omitted/null when merely opening the Slicer view.
- `resize_orca_window { bounds }` updates the attached native window to match the DOM placeholder
- `hide_orca_window` hides/unparents the managed window without killing the slicer process

On Linux, native embedding uses X11 handles. PolySmith requests the X11 backend
at startup (`GDK_BACKEND=x11`, `WINIT_UNIX_BACKEND=x11`) so the same path works
on Wayland desktops through XWayland when XWayland is installed/enabled. It then
finds OrcaSlicer's X11 window by `_NET_WM_PID`, strips Motif window decorations,
reparents the window, and maps/resizes it into the Slicer placeholder. Native
Wayland foreign-window reparenting is not available by design; Wayland-session
support is provided through XWayland.

These Tauri commands are outside the CAD command language. They must not carry
CAD geometry, feature state, or UI-reconstructed mesh data.

The CAM workspace rides the same document/command boundary. CAM state lives in
`document_state.cam` (`setups`, `tool_library`, `operations`, `post_processor`,
`machine_settings`) parallel to `feature_history`; operations consume geometry
and produce toolpaths, never B-rep. All CAM commands reply with
`document_state`:

- `cam_machine_settings_set` stores the `LaserMachineSettings` block
  (bed `work_area_x_mm` / `work_area_y_mm`, red-pointer
  `pointer_offset_x_mm` / `pointer_offset_y_mm`).  The refresh pass
  subtracts the pointer offset from the laser WCS origin — parts framed
  under the red dot cut where the dot was.
- `cam_capture_face_reference {face_id}` captures a TNP-safe
  `FaceAttestation` witness from a body face (`"<body_id>:face:<index>"`)
  and replies `cam_face_attestation_result {persistent_id, attestation}`.
  The UI never fabricates witness geometry.
- `cam_capture_edge_reference {edge_id}` captures a TNP-safe
  `EdgeAttestation` witness from a body edge (`"<body_id>:edge:<index>"`)
  and replies `cam_edge_attestation_result {persistent_id, attestation}`.
  Lines capture endpoints/length/tangent; full circles additionally
  carry the `center`/`axis`/`radius` circle witness (drilling hole
  rims); partial arcs are REJECTED (an arc is not a stable drilling
  input).  Errors: `BAD_EDGE_ID` / `EDGE_NOT_FOUND` /
  `EDGE_CAPTURE_FAILED`.
- `cam_capture_point {x, y, z}` mints a `PointAttestation` for a world
  coordinate picked in the viewport (drilling free picks) and replies
  `cam_attestation_result {persistent_id, attestation: {point: [x,y,z]}}`
  — a coordinate, not topology (TNP doctrine: coordinates never go
  stale, they just stop being where the user clicked).  The core mints
  `pt-N` ids and restores the counter on load.
- `cam_wcs_set_face {face_id, setup_id?}` anchors the WCS origin to a body
  face: the witness lands on the target setup's `wcs_origin.face_reference`
  and the refresh pass resolves the machine origin from the LIVE face
  (mid-UV point) on every recompute — a face-anchored WCS is TNP-safe.
  `face_id` may also be `"stock:<face>"` (top/bottom/front/back/left/right):
  instead of a witness the core stores `wcs_origin.anchor = "stock_face"` +
  `wcs_origin.stock_face = <face>` and resolves the origin from the LIVE
  stock extents (see `cam_stock`), degrading to the stock origin with a
  warning when the stock is unresolvable.
  `setup_id` defaults to the first setup when absent (backward compatible);
  an unknown id replies `SETUP_NOT_FOUND`.
- `wcs_origin` carries an `anchor` discriminator (`""` derived |
  `"face"` | `"stock_face"` | `"point"` | `"stock_origin"`).  `"point"`
  pins an authoritative position that the refresh pass never overwrites;
  the laser pointer-offset shift applies only to non-`"point"` anchors.
- `CamOperation` carries `setup_id` (empty = the first setup, legacy
  documents); generation, export, and refresh resolve each operation
  through ITS setup — multi-setup support.  The setup panel edits the
  setup of the selected operation.

- `cam_setup_create` / `cam_setup_update` take a serialized `CamSetup`
  (machine type, stock definition, WCS, safety/retract heights, units).
  `cam_setup_get` reads the first setup.
- `cam_stock_set` updates `setups[0].stock` from a serialized
  `StockDefinition`; `cam_stock_get` reads it back.
- `cam_tool_add` / `cam_tool_update` / `cam_tool_delete` / `cam_tool_list`
  manage the tool library (`ToolEntry`; `cam_tool_update` payload carries the
  lookup key `tool_id`). Deleting a tool degrades referencing operations to
  `status: "error"` with a message instead of dangling.
- Tool library exchange: `cam_tool_library_list` / `cam_tool_library_save`
  read/write the shared on-disk library (`POLYSMITH_TOOLS_DIR`;
  `cam_tool_list_result` replies). `cam_tool_parse_text` / `cam_tool_parse_file`
  parse LinuxCNC `.tbl` or PolySmith JSON without touching the document
  (`cam_tool_parse_result` `{format, tools, warnings}`).
  `cam_tool_import_file` `{source_path, mode}` applies a file to the
  document library in one batch (mode `renumber` | `overwrite` | `skip`,
  reconciled by guid then number) and replies `document_state`.
  `cam_tool_export_text` / `cam_tool_export_file` serialize the document
  library (`cam_tool_export_text_result` / `cam_tool_export_file_result`).
- `cam_operation_create` takes a serialized `CamOperation` (string `type`;
  `op_id` assigned by the core). For `laser_cut`, an empty
  `geometry_references` makes the core capture TNP-safe profile witnesses
  from `selected_sketch_profile_ids`; `face_milling` takes a
  `FaceAttestation` witness in `machining_regions`. `cam_operation_update`
  is a merge patch (`{op_id, ...fields}`) that invalidates generated paths;
  an explicitly empty `geometry_references` on `laser_cut` / `contour_2d` /
  `engrave` re-captures from the profile selection (or from an explicit
  `selected_profile_ids` array carried in the same payload — the panel's
  Apply), replying `NO_PROFILE_SELECTION` when there is nothing to capture;
  `cam_operation_delete` removes an operation.  `CamOperation` carries a
  persisted `geometry_scope` (`"sketch"` = whole-sketch capture,
  `"selected"` = explicit profile subset; absent in old documents →
  `"sketch"`) set by create/set_scope/update and preserved through
  generate — the scope dropdown reads it to survive save/load.
- `cam_operation_generate` / `cam_operation_preview` run the generator
  registry synchronously (both v1 generators finish in well under 100 ms)
  and emit `cam_generation_progress` events (`{op_id, percent}`) during the
  run; generation stores the toolpath in the memory-only runtime cache and
  marks the operation `generated`. Toolpaths never serialize — the
  document's `ToolpathCache` carries metadata only.
- After a GENERATE (never preview), the core emits `cam_generation_result`
  `{op_id, ok, error_message, warnings: [string]}` after the
  `document_state` reply — the UI's result popup reads it (success /
  warnings list / failure), while the warnings also remain in the
  structured log.
- `LaserCutParameters` (the `laser` block of `cam_operation_create` /
  `cam_operation_update` payloads) carries the v2 model: `mode`
  (`cut|score|engrave`, validated), `power_percent`, `speed_mm_per_s`
  (laser-native speed; absent → the legacy `feedrate_mm_per_min` fallback),
  `passes`, `pass_power_step_percent` (per-pass power ramp: pass p cuts
  at `max(1, power_percent − p × step)`; leads and the pierce keep the
  base power; 0 = no ramp), `dynamic_power`, `air_assist`, `kerf_width_mm`, `kerf_side`
  (`auto|outside|inside|none`), `lead_in_mm` / `lead_out_mm` +
  `lead_in_style` / `lead_out_style` (`line|arc`) +
  `lead_in_angle_deg` / `lead_out_angle_deg`, `lead_in_arc_angle_deg` /
  `lead_out_arc_angle_deg` (arc-style lead roll sweep, default 90°),
  `overcut_mm`,
  `pierce_dwell_seconds` (default 0.1), `pierce_position`,
  `pierce_angle_deg` (nullable), `tabs_enabled` / `tab_width_mm` /
  `tab_spacing_mm` / `tab_power_percent` / `tabs_on_holes`,
  `engrave_style` (`line|fill`) / `line_spacing_mm` / `fill_angle_deg` /
  `fill_bidirectional`, `material_thickness_mm`, `cut_plane_offset_mm`,
  `arc_segments_per_circle` (0 = auto), and `cut_order`
  (`inner_first|nearest_neighbor|by_area`).  Documents saved before v2
  load unchanged — absent keys take the v2 defaults.
- `pierce_angle_deg` picks the lead/pierce side explicitly: a ray from
  each loop's centroid at the given angle (loop-local sketch plane, CCW
  from +X) finds the first contour crossing — that point becomes the
  pierce, the contour walk starts and ends there, and the leads attach
  tangentially.  When set it **overrides `pierce_position`** (the UI
  disables that dropdown while an angle is set); when absent the
  automatic placement rules apply.  Mirrored sketch frames mirror the
  visual direction — the angle is loop-local, and the lead tangents
  always follow the contour walk, so both stay consistent.
- Lead side follows the kerf offset side.  `kerf_side: auto` offsets the
  cut OUTWARD on outer loops (scrap outside the disc) and INWARD on holes
  (scrap inside the hole), and the pierce + leads land on that same side:
  exterior offsets keep the tangent lead-in/out, interior offsets turn
  the lead into a spoke along the pierce→centroid ray (perpendicular to
  the contour on circles) — a straight tangent line cannot lie inside a
  closed contour.  `kerf_side: inside|outside` overrides force that side
  (and the lead side) on every loop; `none` and engrave have no offset
  and keep tangent leads.
- `arc_segments_per_circle` pins how many chords approximate each full
  circle in the polyline viewport render and in the `use_arcs=false`
  G-code post (proportional counts for partial arcs).  `0` (default)
  keeps the legacy chord-tolerance paths; the toolpath IR always keeps
  true G2/G3 arcs regardless of the display count.
- `cam_post_processor_set` stores the `PostProcessor` — `type` names a post
  DEFINITION.  Post processors are first-class files: one `<name>.json` per
  machine in the user's posts directory (`POLYSMITH_POSTS_DIR`, resolved by
  the shell from the app-data path, created and seeded with the built-ins on
  first use).  The file's line templates drive the output shape; the core
  re-reads it on every export, so edits apply immediately.
- `cam_post_list` replies `cam_post_list_result {posts: [{name, path}]}`;
  `cam_post_import {source_path}` validates a definition JSON, copies it into
  the posts directory, and replies the updated list (broken definitions are
  rejected).
- `cam_machine_list` replies `cam_machine_list_result
  {machines: [MachineDefinition]}` — the machine library, built-ins first,
  then user files.  A `MachineDefinition` is `{name, machine_type,
  post_processor {type, filename}, work_area_x_mm, work_area_y_mm,
  pointer_offset_x_mm, pointer_offset_y_mm}`.  Like posts, machines are
  first-class files: one `<slug>.json` per machine in the user's machines
  directory (`POLYSMITH_MACHINES_DIR`, resolved by the shell from the
  app-data path, seeded with `grbl-laser`, `smoothieware-laser`, and
  `generic-3-axis-mill` on first use), re-read on every list, so external
  edits apply immediately.
- `cam_machine_save {MachineDefinition}` validates (non-empty name, supported
  machine type, positive work area for lasers) and writes the definition as
  `<slug>.json`, replying the refreshed library via `cam_machine_list_result`;
  invalid definitions are rejected with `CAM_MACHINE_SAVE_FAILED`.
- `cam_export_gcode { file_path }` generates stale toolpaths on demand,
  serializes every enabled operation through the selected post definition,
  and replies `document_exported` with `format: "gcode"`.
- `cam_export_gcode_text {}` is the same posting pipeline without the file —
  replies `cam_export_gcode_text_result` with `{text, format: "gcode",
  exported_feature_count}`.  The GRBL workspace handoff consumes this so the
  posted program never touches disk.
- Built-in posts now include `lasergrbl` (GRBL 1.1 laser dialect: M4
  dynamic / M3 constant, `power_max` 1000, M8/M9 air assist,
  `laser_off_with_power` so every laser-off emits `S0` before `M5`,
  Z-free footer, no line numbers) and the machine library seeds a
  **LaserGRBL** machine (430×430, post `lasergrbl`).  The
  `laser_off_with_power` flag is opt-in per post definition — the
  built-in `grbl` post output is unchanged.
- **Shell-side GRBL transport (no core IPC):** the desktop shell can
  stream an exported `.nc` directly over serial (see the GRBL panel
  under the Setup panel's Machine tab, and the standalone GRBL
  workspace).  Rust commands
  (`grbl_list_ports`, `grbl_connect {port, baudRate}`, `grbl_send_file
  {filePath}`, `grbl_pause`/`grbl_resume`/`grbl_reset`/`grbl_home`/
  `grbl_unlock`/`grbl_jog {x?, y?, feed}`, `grbl_zero_xy` — sends
  `$X` + `G92 X0 Y0` — and `grbl_send_raw {line}` for arbitrary
  commands) drive a worker that owns
  the port; state returns as `grbl-stream` Tauri events
  (`connected|disconnected|progress|status|error|completed|paused|
  resumed|reset|zeroed` with lines/percent/state/MPos/WPos).  Sending respects
  GRBL's 128-byte RX buffer via a 127-byte window and the ok/error:N
  handshake; the status poll runs at 500 ms, emitted ≤ 5 Hz.
  FluidNC quirks handled: `WCO:` reports the work-coordinate offset
  (WPos is derived), `ALARM:n` lines abort the active job, and
  `$20=0`/`G10 L20 P0` are rejected/no-op on FluidNC v4 (soft limits
  live in the board's config.yaml).
- **Shell-side G-code parsing (no core IPC):** `grbl_parse_file
  {filePath}` returns structured moves for the GRBL workspace preview
  (`fileName`, `moves[]` with `index`/`line`/`kind`/`start`/`end`/
  `center`/`radius`/`feed`/`power`/`laserOn`/`dwellSeconds`, `bounds`,
  `unitsMm`, `absolute`, `warnings[]`).  Line filtering matches the
  sender byte-for-byte, so `move.line` equals the sender's
  `linesSent` progress counter.
- `LaserTestPatternParameters` (the `test_pattern` block of
  `cam_operation_create` / `cam_operation_update` for
  `type: "laser_test_pattern"` ops) drives LightBurn-style material test
  cards: `pattern` (`engrave_grid` filled squares | `cut_grid`
  through-cut squares | `kerf_gauge` calibration square),
  `power_min_percent` / `power_max_percent` / `power_steps` (columns,
  ascending left→right), `speed_min_mm_per_s` / `speed_max_mm_per_s` /
  `speed_steps` (rows, ascending top→bottom), `cell_size_mm`,
  `cell_spacing_mm`, `start_x_mm`, `start_y_mm`, `line_spacing_mm`
  (fill density), `kerf_width_mm` / `power_percent` / `speed_mm_per_s`
  (gauge cut), and `cell_labels` (engraved "P… S…" labels under every
  cell via the core text engine).  Cells live directly in MACHINE
  coordinates; a grid exceeding the machine settings' work area warns.
- Viewport toolpath points carry `pierce` (laser on + dwell > 0) —
  the UI renders pierce markers, and the payload carries no other
  interaction state.

Operation status semantics: `pending` → `generated` → `needs_regenerate`
(every document mutation invalidates the revision-keyed cache) → `error`
with a human-readable `status_message` when a geometry reference no longer
resolves (TNP doctrine: degrade, never guess).

The protocol also covers native document persistence and the Project sketch
tool:

- `create_plugin_feature { plugin_id, feature_type, display_name, parameters_summary, parameters, geometry }`,
  `update_plugin_feature { feature_id, plugin_id, feature_type, display_name, parameters_summary, parameters, geometry }`,
  and `confirm_plugin_feature { feature_id }` add the trusted plugin feature
  boundary. Plugins own UI, defaults, domain validation, and the serialized
  `parameters` JSON. The native core owns the resulting CAD state by storing
  `feature_history[].plugin_feature_parameters` and interpreting the ordered
  generic `geometry` recipe during recompute. Geometry operations are `add` or
  `subtract`; primitives are `box`, `rounded_box`, `tapered_rounded_box`,
  `cylinder`, `profile_extrude`, or `rounded_rect_profile_sweep`; coordinates
  use plugin-local X/Y for the footprint plane and Z for height.
  `tapered_rounded_box` accepts optional `top_width`, `top_depth`, and
  `top_radius` fields for lofted profiles. Optional `top_offset_x` and
  `top_offset_y` shift the top profile from its centered position.
  `profile_extrude` accepts `profile_plane` (`xy`, `xz`, or `yz`),
  `profile_points: [{u, v}]`, and `extrude_x` / `extrude_y` / `extrude_z` for
  generic profile solids. `rounded_rect_profile_sweep` accepts a `yz` profile
  plus `path_width`, `path_depth`, and `path_radius` for generic rounded-path
  sweeps. The core must not contain plugin-specific modeling code.
- `create_offset_plane { source_plane_id, offset }` adds a parametric offset construction plane to the document. `source_plane_id` may be one of the three origin planes (`ref-plane-xy/yz/xz`), an existing construction plane's feature id, a sketch profile id, or a planar body face id of the form `<body_id>:face:<index>`. `offset` is a signed distance (mm) along the source's normal. For sketch profiles, the core uses the owning sketch plane and centers the source frame on the profile region. The core resolves the source's frame, slides it along the normal, stores the result on a new `construction_plane` feature, and emits the updated document.
- `create_midplane { source_plane_ids: [first, second] }` adds a construction plane halfway between two parallel plane-like sources. Sources use the same ids as `create_offset_plane` except both must resolve to parallel plane frames. The core stores both source ids on the construction-plane feature and re-resolves both during dependency refresh.
- `create_tangent_plane { source_face_id }` adds a construction plane tangent to a body face. The source must be a body face id from `viewport_state.solid_faces[]`; curved faces are sampled at their representative midpoint and planar faces resolve to a coincident tangent plane.
- `create_angle_plane { source_plane_id, source_axis_id, angle_degrees }` adds a construction plane rotated from a plane-like source around a linear axis. `source_plane_id` follows the same rules as `create_offset_plane`; `source_axis_id` may be a sketch line id or a linear body edge id from `viewport_state.edges[]`. The axis must be parallel to the source plane. The core stores the plane source, axis source, and angle, and re-resolves both sources during dependency refresh.
- `create_construction_axis { source_id }` adds a construction axis from a sketch line id or a straight body edge id from `viewport_state.edges[]`. The feature stores the source id plus cached world-space endpoints and re-resolves the source during dependency refresh.
- `create_construction_point { source_id }` adds a construction point from a sketch point id or a body vertex id from `viewport_state.vertices[]`. The feature stores the source id plus cached world-space position and re-resolves the source during dependency refresh.
- `create_hole { face_id, center_x, center_y, center_z, ...parameters }` creates a semantic `hole` feature on a planar body face. The core stores the source face id, target body id, local center, type (`simple` / `counterbore` / `countersink` / `spotface`), extent (`blind` / `through_all`), size parameters, selected standard metadata (`standard`, `standard_size`, `hole_fit`), and thread metadata (`thread_spec`, pitch, major/minor diameter, depth, representation). Cosmetic threaded holes emit a lightweight viewport helix. Modeled threaded holes are experimental/known-buggy and should not be treated as reliable production geometry until the native thread path is reworked. `update_hole_parameters { feature_id, parameters }` drives live preview and `confirm_hole { feature_id }` flips the pending flag off.
- `create_helix { axis_source_id, radius, pitch, height, handedness, start_angle_degrees }` creates a core-owned construction helix from a sketch line, construction axis, or straight body edge source. The cached sampled points are re-derived during dependency refresh and emitted through `viewport_state.helices[]`.
- `create_thread`, `update_thread_parameters`, and `confirm_thread` add the semantic thread feature contract for a target body plus a sketch-line / construction-axis / straight-edge axis source. Cosmetic representation emits a lightweight cosmetic helix through `viewport_state.helices[]`; modeled representation is experimental/known-buggy and should not be treated as reliable production geometry until the native thread path is reworked. Dependency refresh re-resolves both references and marks the feature `dependency_broken` when either target disappears.
- `create_fastener` and `update_fastener_parameters` add the semantic fastener feature contract with standard, size, diameter, minor diameter, pitch, length, thread length, head type, drive type, and thread representation parameters. Cosmetic thread representation emits a lightweight viewport helix; modeled representation is experimental/known-buggy and can produce invalid or incomplete threaded shafts in viewport/export. Hex-socket and Phillips drive options cut simple recess geometry into the generated head.
- `create_move { target_body_id, parameters? }`, `update_move_parameters { feature_id, parameters }`, and `confirm_move { feature_id }` add a core-owned `move` timeline feature for one body. Parameters round-trip on `feature_history[].move_parameters` with `target_body_id`, local-frame translation components, local-frame rotation components, and `is_pending`. The body compiler resolves the target body during replay, transforms that body in place around its current pre-move bounding-box center, preserves the body id, updates its emitted local frame, and marks the feature `dependency_broken` if the target body can no longer be resolved.
- `create_body_copy { source_body_id, copy_mode? }` adds a core-owned `body_copy` timeline feature. `copy_mode: "linked"` (default) resolves the source body during replay, emits a new body under the copy feature id, preserves the source local frame, and marks the copy `dependency_broken` if the source body can no longer be resolved. `copy_mode: "standalone"` stores a frozen core shape snapshot and local frame so the copy survives later source edits independently.
- `unlink_body_copy { feature_id }` converts a linked `body_copy` into a standalone copy by resolving the copy at its current feature-history position, storing that shape snapshot plus local frame, and flipping `copy_mode` to `"standalone"`. The change is undoable through the normal undo stack but intentionally removes the future source-body dependency.
- `set_body_color { body_id, color }`, `set_face_color { face_id, color }`, `clear_body_color { body_id }`, `clear_face_color { face_id }`, and `clear_appearance_overrides {}` maintain document-scoped appearance overrides. Colors are opaque `#RRGGBB` strings stored under `document_state.appearance`. Body overrides are keyed by body/root feature id. Face overrides store the emitted `face_id`, owner body id, and a face geometry signature so the core only reapplies them when the face still resolves to the same topology; semantic legacy face ids use their stable face id as the signature.
- `update_offset_plane { feature_id, offset }` rewrites the offset on an existing construction plane and re-derives its cached frame from the source's current frame, so chained planes / face-source planes update correctly under upstream edits.
- `update_angle_plane { feature_id, angle_degrees }` rewrites the angle on an existing angle construction plane and re-derives its cached frame from the current source plane and axis.
- `viewport_state.reference_planes[]` gained an optional `plane_frame` field. Origin planes leave it null and the renderer keeps using the legacy `orientation` rotation; construction planes ship a real world-space frame and the renderer positions the quad with that frame instead.
- `viewport_state.reference_axes[]` now also carries construction-axis features using `axis: "custom"` and explicit endpoints. `viewport_state.reference_points[]` carries construction-point features with a world-space position. `viewport_state.helices[]` carries sampled construction helix polylines.
- `save_document` writes the live document state as a JSON `.polysmith` file at the supplied `file_path`; the core replies with `document_saved`
- `load_document` parses a `.polysmith` file, replaces the live document, restores ID counters by scanning the loaded ids, clears undo/redo stacks, and replies with `document_state`
- `set_timeline_cursor { included_action_count }` moves the core-owned parametric history cursor. The count is measured in non-root timeline actions; the core clamps it to the valid range, stores `null` when the cursor is at the end, and subsequent `get_viewport_state` calls rebuild the viewport from the feature-history prefix at that cursor without deleting later features.
- `undo` / `redo` restore the previous / next snapshot on the undo stack and run the full refresh pipeline (selection re-validation, sketch rebuild, toolpath invalidation) before replying with `document_state`. On an empty stack the core replies with a structured `error` carrying code `EMPTY_UNDO_STACK` / `EMPTY_REDO_STACK` — an expected no-op condition, not a failure; the UI treats it silently (no error toast). `undo_many { count }` undoes (negative count = redoes) several steps in ONE refresh; the count is clamped to the available steps and, inside an open undo group, to the session-local steps.
- Named grouped steps (D2): `undo_begin_group { name }` opens a group; pushes inside it collapse into ONE entry — the group's start snapshot carrying the group's name — when `undo_end_group` closes it. Groups nest (a dimension draft group inside the sketch session); ending a changed group folds its change into the enclosing frame. `undo_abort_group` restores the group's start snapshot and drops every session-local step with no trace (cancel semantics). `undo_abort_all_groups` aborts the WHOLE group stack — the "Cancel Sketch" contract: the entire edit session rolls back to the snapshot taken when the sketch was entered, nested groups included, leaving nothing in the history.
- Sketch edit sessions (D4) are themselves a group: entering a sketch (`start_sketch_on_plane` / `start_sketch_on_face` / `reenter_sketch`) opens a session group; while it is open, per-action undo/redo walks only session-local steps; `finish_sketch` closes the group so the whole session appears as ONE history step (no step appears when nothing changed); Cancel Sketch sends `undo_abort_all_groups` so the session leaves no trace.
- `set_undo_limit { limit }` caps the undo history depth (default 30); the oldest entries are dropped on push.
- Every `document_state` payload carries `can_undo` / `can_redo` booleans plus `undo_step_names[]` / `redo_step_names[]` (most recent first) so the UI can render named history menus without reading request-only session flags. Step names are derived from the calling function's name at each push site ("Add Sketch Line", "Extrude Profile", …).
- `project_face_into_sketch` projects the outline of a selected solid face onto the active sketch's plane, creating fixed-endpoint sketch lines or sketch circles for circular caps and annular circular loops. Annular planar circular faces project as concentric sketch circles instead of sampled polygon segments. Legacy box/cylinder features are not yet supported by the projection helper and produce a structured error.
- `project_profile_into_sketch` projects a sketch profile boundary into the active sketch, creating fixed-endpoint projected lines for polygon loops and projected circles for circular profiles; profile inner loops are included.
- `project_edge_into_sketch { edge_id }` projects a single body edge onto the active sketch's plane. Linear edges become sketch lines; circular edges become sketch circles or arcs when the edge's plane is parallel to the sketch's. Edges that would project to ellipses (non-parallel circle plane) and other curve types (B-splines, etc.) are rejected with a structured error so the UI can surface a transient message. Repeated clicks on the same edge are no-ops (idempotency now walks `sketch_parameters.projections[*].source_id`).
- `project_vertex_into_sketch { vertex_id }` projects a single body vertex onto the active sketch's plane as a fixed standalone sketch point (`points[]` entry with `kind = "projected"`). Recorded in `sketch_parameters.projected_points[]` for the cached coords plus a `sketch_parameters.projections[]` entry for the live link. Repeated clicks on the same vertex are no-ops.
- Project commands append a `SketchProjection` record to `sketch_parameters.projections[]`. Each record carries `source_id`, `source_kind` ("face" / "edge" / "vertex" / "profile"), and the ids of every entity the projection generated (`generated_line_ids`, `generated_circle_ids`, `generated_arc_ids`, `generated_point_id`). Body face / edge / vertex projections are re-resolved by the core's `refresh_sketch_projections` pass on every recompute and patch the matching generated entities in place, so editing upstream body geometry moves the projected sketch entities in lockstep (contextual modeling live link). Profile projections are recorded for identity and UI treatment. When a live body source can't be re-resolved (body deleted, curve type changed) the projection's `dependency_broken` flag is set and the parent sketch surfaces a feature-level warning; the generated entities stay frozen at their last-known coords until the user re-projects.

For the current spike, export is intentionally narrow:

- format: STEP
- exported content: all solid-producing document features that can be rebuilt from feature parameters
- skipped content: non-solid sketch-only features
- viewport-only presentation data such as primitive spacing is not part of the export contract

A viewport snapshot follows the same rule set. The core decides what renderable scene data exists, and the UI only visualizes that snapshot.

For renderer-oriented viewport data, the same ownership rule still applies:

- the core may provide primitive placement, centers, and scene bounds when that helps visualization
- the core may provide renderer-facing polygon footprint data for sketch profiles or profile-driven solids when the viewport needs to render them
- the core may provide reference geometry such as origin planes and axes when those are selectable CAD targets
- the core may provide lightweight solid-face metadata for picking and highlighting when a face is a selectable CAD target
- the core may provide active sketch state, renderable sketch entities, derived sketch dimensions, and renderable sketch constraint markers when sketching is in progress
- the UI may adapt that snapshot for a renderer, but it must not invent CAD state or modeling behavior

Sketch commands follow the same ownership boundary:

- the UI may send selection or sketch intent such as `select_face`, `start_sketch_on_face`, `start_sketch_on_plane`, `set_sketch_tool`, `update_sketch_line`, `update_sketch_point`, `move_sketch_entities`, `set_sketch_line_constraint`, `set_sketch_equal_length_constraint`, `set_sketch_coincident_constraint`, `set_sketch_perpendicular_constraint`, `set_sketch_parallel_constraint`, `set_sketch_point_fixed`, `update_sketch_circle`, `update_sketch_dimension`, `update_sketch_dimension_label_position`, `add_sketch_angle_dimension`, `add_sketch_distance_dimension`, `add_sketch_point_distance_dimension`, `add_sketch_line_length_dimension`, `add_sketch_circle_radius_dimension`, `add_sketch_polygon_radius_dimension`, `add_sketch_line`, `add_sketch_rectangle`, `add_sketch_circle`, `add_sketch_polygon`, `add_sketch_arc`, `select_sketch_point`, `select_sketch_entity`, `select_sketch_dimension`, `select_sketch_profile`, `extrude_profile`, `extrude_face`, `update_extrude_depth`, `loft_profiles`, `update_loft_profiles`, `update_loft_ruled`, `revolve_profile`, `update_revolve_profile`, `update_revolve_axis`, `update_revolve_angle`, `sweep_profile`, `update_sweep_profile`, `update_sweep_path`, `finish_sketch`, or `reenter_sketch`
- `select_sketch_entity { entity_id, additive? }` selects a sketch edge entity (line / circle / arc). Plain selection replaces the selected sketch entity list; additive selection toggles the entity in `selected_sketch_entity_ids[]` and stores the most recent selected entity in `selected_sketch_entity_id` for compatibility.
- `select_sketch_entities { entity_ids[], additive? }` batch selection — replaces (or toggles per-id in additive mode) the sketch entity selection in ONE command. Unknown ids are skipped with a warning instead of aborting the batch.
- `select_sketch_rect { x1, y1, x2, y2, window_mode, additive? }` marquee selection resolved CORE-side against the exact sketch geometry. `x1/y1/x2/y2` are sketch-local coordinates of the two drag corners; `window_mode` is the screen drag direction (left→right, passed by the UI so the semantics don't flip when the view is mirrored). Window mode selects entities fully inside; crossing mode selects entities touching the rectangle. Construction lines/arcs/ellipses are skipped; construction circles are selectable. This replaced the old UI-side screen-space collection — a stale scene mis-collected and the following delete removed the perimeter instead of the marquee'd entities.
- `select_sketch_point { point_id, additive? }` selects a sketch vertex / point. Plain selection replaces the selected sketch point list; additive selection toggles the point in `selected_sketch_point_ids[]` and stores the most recent selected point in `selected_sketch_point_id` for compatibility.
- `add_sketch_line { start_x, start_y, end_x, end_y, is_construction? }` creates a sketch line on the active sketch. Construction lines render dashed, stay available for snapping / constraints, and are excluded from profile loop detection and automatic line dimensions.
- `add_sketch_rectangle { start_x, start_y, end_x, end_y, is_construction? }` creates four sketch lines. When `is_construction` is true, all four sides are construction lines and therefore do not seal selectable profiles or receive automatic side dimensions.
- `add_sketch_circle { center_x, center_y, radius, is_construction? }` creates a sketch circle. Construction circles render dashed, stay selectable / snappable, are excluded from profile and hole detection, and do not receive an automatic diameter dimension.
- `add_sketch_polygon { sides, mode, start_x, start_y, end_x, end_y, is_construction? }` creates a regular N-sided polygon on the active sketch. `mode` is one of `inscribed` (center + vertex), `circumscribed` (center + apothem), or `edge` (two edge endpoints). The core computes vertices in sketch-local space from the supplied parameters and rejects `sides < 3` as a structured error. Non-construction polygons receive an automatic radius dimension.
- `add_sketch_arc { start_x, start_y, end_x, end_y, anchor_x, anchor_y, mode, is_construction? }` creates a sketch arc on the active sketch. `mode` is one of `three_point` (anchor lies on the arc; center = circumcenter of start, anchor, end) or `center_start_end` (anchor is the center; end is snapped onto the resulting circle). Endpoints participate in the shared sketch-point graph and are stored as fixed (v1 freezes arc shape at creation; reshape requires delete + redraw). The core rejects colinear / zero-radius input as a structured error. Non-construction arc edges contribute to closed-profile loop detection alongside lines, with interior points sampled into the profile so OCCT extrudes a clean curved boundary; construction arcs are skipped by profile detection.
- `add_sketch_text { text?, font_path?, height_mm?, angle_deg?, anchor_x, anchor_y, h_align?, v_align?, char_spacing? }` places parametric text on the active sketch at the sketch-local anchor. `text` is UTF-8 and may contain `\n` (multi-line); defaults are `"Text"`, 10 mm, 0°, `h_align "center"` (`left|center|right`), `v_align "middle"` (`top|middle|bottom`), spacing 0. `font_path` empty = the default font (bundled path when present, otherwise a system font via the OCCT font manager); an absolute `.ttf` path loads a user font. Text exceeds 500 characters is rejected as a structured error. The core expands every text into ordinary sketch lines (`generated_by = "text:<id>"`) on every recompute — glyph contours participate in profile detection, extrude, viewport rendering, and STEP/STL export exactly like drawn geometry. The generated lines/vertices are not user-editable: update/trim/move/dimension/constraint commands reject them with a structured error; `delete_sketch_selection` maps a pure-glyph selection to deleting the owning text entity. The record round-trips at `feature_history[].sketch_parameters.texts[]` (`{text_id, text, font_path, height_mm, angle_deg, anchor_x, anchor_y, h_align, v_align, char_spacing, path_entity_id, path_offset}` — the last two reserved for text-on-path).
- `add_sketch_ellipse { center_x, center_y, axis_a_x, axis_a_y, axis_b_x, axis_b_y, is_construction? }` creates a full ellipse (center + major-axis point + minor-axis point). The axis points are fixed at creation (no solver registration); the exact profile engine treats the ellipse as a full closed curve and the wire builder emits an analytic OCCT ellipse edge.
- `add_sketch_slot { center_x, center_y, length, radius, rotation, is_construction? }` / `update_sketch_slot { slot_id, center_x, center_y, length, radius, rotation }` manage straight slots (stadiums) expanded into 2 generated lines + 2 arcs on every recompute.
- `add_sketch_chamfer { corner_vertex_id, line_a_id, line_b_id, distance_a, distance_b }` / `update_sketch_chamfer { chamfer_id, distance_a, distance_b }` / `delete_sketch_chamfer { chamfer_id }` manage parametric corner chamfers (fillet + chamfer on the same corner rejected).
- `add_sketch_spline { points: [{x, y}, ...], is_construction? }` creates a control-point B-spline — the points ARE the poles (movable vertices), degree = min(3, count-1), clamped open-uniform knots. Pole drags re-fit via the vertex sync; a closed control polygon (end pole = start pole) bounds a region by itself. Extrude emits the exact `Geom_BSplineCurve` edge.
- `transform_sketch_entities { entity_ids, dx, dy, center_x, center_y, angle_deg, scale, copy }` translates / rotates / uniformly scales sketch entities (or exploded copies) in one undo step; `move_sketch_entities` is a rigid wrapper.
- `create_linear_array { entity_ids, dx, dy, count }` / `create_circular_array { entity_ids, center_x, center_y, count, total_angle_deg }` create exploded array copies (one undo step).
- `extend_sketch_entity { entity_id, click_x, click_y }` extends a line / arc to the nearest intersection; `offset_sketch_entity { entity_id, distance }` offsets a single line / circle / arc by a signed distance (non-parametric copy).
- `trim_sketch_entity { entity_id, click_x, click_y, segment_index?, expected_revision?, preview_id? }` trims the clicked entity (line / circle / arc / ellipse / spline): the entity is split at every intersection with other non-construction entities and the clicked segment is deleted (circles convert to the complementary arc, full ellipses convert to a partial elliptical arc carrying `has_sweep`/`sweep_start_angle`/`sweep_end_angle`/`ccw` + split endpoint vertex ids; middle-segment trims split arcs, elliptical arcs and splines into two — spline pieces are re-fit exactly via OCCT knot-insertion, so the cut ends land on the intersection). The optional `segment_index` is the hovered segment index from the `trim_preview_result` event — when present the trim deletes EXACTLY that segment instead of re-deriving it from the click point, so the red hover highlight and the trim can never disagree (the 2026-08 "trim floods" regression). `expected_revision` is the document revision the preview was computed against: on mismatch the core ignores the stale index and falls back to the click point. `trim_preview { entity_id, cursor_x, cursor_y }` → `trim_preview_result { entity_id, entity_kind: line|circle|arc|ellipse|spline, hovered_index, revision, full_circle?/full_arc?/full_ellipse?/full_spline?, segments[] }` lets the UI render the authoritative highlight from the core's own split; the UI coalesces previews to one per frame and drops responses that are not the newest request. Constraint policy (D1): surviving pieces inherit H/V badges, whitelisted relations, surviving coincident pairs and point-on-object anchors at line cutters; dimensions re-derive as driven; circle→arc keeps concentric (D4); minted split vertices are frozen before the planegcs pass.
- `trim_sketch_stroke { entries: [{ entity_id, click_x, click_y }] }` is the drag-paint trim (R5): every crossed entity is trimmed in one batch and committed as ONE undo entry; each entry re-derives its segment from its click point against the current geometry; failed entries are logged and skipped.
- `corner_trim_sketch_entities { entity_a_id, entity_b_id, click_x, click_y }` trims/extends two lines/arcs to their virtual corner (nearest curve-curve intersection to the click; parallel curves throw). `corner_trim_preview { entity_a_id, entity_b_id, cursor_x, cursor_y }` → `corner_trim_preview_result { entity_a_id, entity_b_id, valid, revision, corner?, a?, b? }` (per-segment `kind`, `start`, `end`, and arc `center`/`radius`/`ccw`, sketch-local, nothing mutated) drives the second-pick hover ghost.
- `split_sketch_entity { entity_id, click_x, click_y, split2_x, split2_y }` divides the clicked entity at ALL intersections without deleting anything (equal-length relations do not transfer); circles and full ellipses use the two-point rule (`split2_*` ignored for open kinds) and split into two CCW pieces sharing endpoint vertices.
- `add_sketch_arc_radius_dimension { arc_id }`, `add_sketch_arc_angle_dimension { arc_id }` (value = sweep in radians), `add_sketch_arc_length_dimension { arc_id }` create driving arc dimensions; arc length drives the sweep as L / radius.
- `update_sketch_text { text_id, ...patch fields... }` merges the supplied fields over the stored text record and re-expands the glyph geometry (one undo entry per command; the UI debounces typing). Edits that keep the glyph id set unchanged (height / angle / anchor / alignment / spacing) re-snapshot linked extrudes in place; string or font edits change the id set and degrade linked extrudes through the standard `dependency_broken` warning. Text edits require the sketch to be active (re-enter it after extruding, as with any other sketch edit).
- `delete_sketch_text { text_id }` removes the text record; the generated glyph lines and their profiles disappear on the next recompute.
- `add_sketch_angle_dimension { first_line_id, second_line_id }` creates or reselects a line-line angle dimension. The native core validates the two lines share an endpoint and owns the subsequent `update_sketch_dimension` solve.
- `add_sketch_distance_dimension { first_entity_id, second_entity_id }` creates or reselects a distance dimension for parallel line-to-line, circle-center to circle-center, and circle-center to line picks. The native core owns the solve behavior; the UI only sends picked entity ids and edits the returned dimension value.
- `update_sketch_dimension_label_position { dimension_id, label_x, label_y }` stores a sketch-local label placement override for a dimension. The core treats this as presentation metadata only; it does not affect the dimension's solved value. **Every** dimension kind honors the override, but they interpret it differently: linear kinds project it onto the dimension line's offset axis (1 DOF); `circle_radius`, `arc_radius` and `arc_length` place the label freely in 2D and rebuild their leader around it; `angle`, `line_angle` and `arc_angle` reduce it to a radius on the bisector, clamped to [6, 500]. The override is re-glued to its geometry when a dimension *value* edit moves that geometry — around the anchor midpoint for linear kinds, around the circle/arc centre for radial ones.
- `viewport_state.sketch_dimensions[]` carries the emitted leader geometry per kind. For the radial leader kinds (`circle_radius`, `arc_radius`): `anchor_start` is the leader origin on the curve, `anchor_end` is a rim point a quarter turn away that gives the renderer an in-plane direction reference (the centre, contact and label are collinear, so there is no cross product to recover the sketch plane from), `dimension_start`/`dimension_end` are the arrowhead tips — equal when the kind draws a single arrow — and `arc_center`/`arc_radius` carry the centre and measured radius. For the span kinds (`arc_length`, `arc_angle`): `anchor_start`/`anchor_end` are the witness-line feet on the measured arc, `dimension_start`/`dimension_end` are the extension-arc endpoints, and `arc_radius` is the extension radius. The short landing under the value text is derived by the renderer, not emitted.
- `add_sketch_point_distance_dimension { point_a_id, point_b_id }` creates or reselects a straight-line distance dimension between two sketch points. The native core validates both points exist and pushes a `SketchDimension` of kind `point_distance` with the current euclidean distance as its initial value.
- `add_sketch_line_length_dimension { line_id }` creates a length dimension on a single sketch line that doesn't already have one. Validates the entity exists and is not construction, checks for duplicate `dim-line-{id}`, and pushes a `SketchDimension` of kind `line_length` with the current geometric length. Used by the Dimension tool when the user clicks a line whose auto-dimension was deleted.
- `add_sketch_circle_radius_dimension { circle_id, display_as? }` creates a radius dimension on a single sketch circle. Same pattern: validates, deduplicates, pushes a `SketchDimension` of kind `circle_radius`. An optional `display_as` parameter (`"radius"` / `"diameter"`) controls rendering. Used by the Dimension tool when the user clicks a circle.
- `add_sketch_polygon_radius_dimension { polygon_id }` creates a radius dimension on a single sketch polygon. Same pattern: validates, deduplicates, pushes a `SketchDimension` of kind `polygon_radius`. Used by the Dimension tool when the user clicks a polygon.
- `viewport_state.sketch_circles: [{circle_id, plane_id, plane_frame, center, radius, is_selected, is_construction, is_preview}]` carries world-space circle centers plus the sketch-plane radius. `plane_frame` is nullable for legacy origin-plane sketches and present for face/custom-plane sketches so the UI can render inactive/projected circles in their real 3D sketch plane. The corresponding feature-level state lives at `feature_history[].sketch_parameters.circles[]` with local center / radius / `is_construction`.
- `viewport_state.sketch_polygons: [{polygon_id, plane_id, plane_frame, corner_x[], corner_y[], corner_z[], sides, mode, center, radius, is_selected, is_construction, is_preview}]` carries world-space corner arrays and the polygon's center/radius/sides/mode. The UI draws a closed `THREE.Line` loop from the corner arrays. The corresponding feature-level state lives at `feature_history[].sketch_parameters.polygons[]` with local center/radius/sides/mode and the two endpoints that defined the shape.
- `viewport_state.sketch_arcs: [{arc_id, start_point_id, end_point_id, plane_id, plane_frame, center, radius, start, end, ccw, is_selected, is_construction, is_preview}]` carries the world-space endpoint and center coordinates plus the sweep direction (`ccw`); the UI samples between `start` and `end` around `center` using `plane_frame` when available. The corresponding feature-level state lives at `feature_history[].sketch_parameters.arcs[]` with the same shape but in sketch-local 2D coordinates
- `add_sketch_fillet { corner_point_id, line_a_id, line_b_id, arc_a_id?, arc_b_id?, radius }` rounds a corner shared by two sketch entities into a tangent arc. Operands may be two lines, a line and an arc, or two arcs — exactly one of `{line_a_id, arc_a_id}` (and the same for side B) is sent; line-line callers (the v1 shape) omit the arc ids. The corner is identified by the shared sketch point id; the core validates strict eligibility (corner is an endpoint of both entities, the operands are not tangent at the corner, construction arcs are rejected, the radius fits on each operand, no other fillet already at this corner) and rejects with a structured error otherwise. On success the core mutates each operand's filleted endpoint to reference a newly allocated fixed trim point and inserts a generated `SketchArc` between them. The relationship is parametric: a `SketchFillet` record on the sketch carries enough state to keep the geometry tangent under subsequent operand edits (the corner re-derives from the live geometry on every recompute) and to fully restore the original corner on delete
- `update_sketch_fillet_radius { fillet_id, radius }` rewrites the parametric radius and re-runs the sketch recompute pass; the trim distances and arc geometry update in lockstep. If the new radius no longer fits on the current operand geometry the recompute silently skips the update (leaving the previous frame's geometry intact) — the user can drag the operands longer to recover
- `delete_sketch_fillet { fillet_id }` restores each operand's filleted endpoint back to the original corner point and removes the generated arc + the fillet record. The corner point is re-emitted by the next `rebuild_sketch_points` from the fillet's cached `corner_x` / `corner_y` (denormalized onto the fillet record specifically so the points table can survive the case where no other entity references the corner)
- `delete_sketch_selection { entity_ids[], point_ids[], profile_ids[] }` deletes selected sketch geometry from the active sketch. Entity ids may reference sketch lines, circles, or arcs. Point ids resolve to their owned geometry: line / arc endpoints delete connected edges, circle center points delete the circle, and projected standalone points delete that point. Profile ids resolve to the profile's core-owned boundary geometry, so selecting a whole sketch region can remove that shape. The core removes dangling dimensions, relations, anchors, projection links, and generated fillet records as needed, then recomputes sketch points, profiles, and profile-linked extrudes. The UI may warn before sending the command when the active sketch has downstream dependents, but the actual mutation stays core-owned.
- `feature_history[].sketch_parameters.fillets: [{fillet_id, corner_point_id, corner_x, corner_y, line_a_id, line_b_id, arc_a_id?, arc_b_id?, trim_a_point_id, trim_b_point_id, arc_id, radius}]` round-trips through save / load so the parametric model survives across sessions. The optional arc ids carry arc operands (line-arc / arc-arc fillets) and are omitted for line-line records; old builds keep loading line-line files. The generated trim points appear in `points[]` (with `is_fixed=true`) and the generated arc in `arcs[]`, just like any other sketch geometry, but consumers that need to know they're fillet outputs (not user-drawn) can cross-reference by id
- `select_sketch_profile` and `extrude_profile` accept any profile in the document (the owning sketch is resolved by the core); they do not require an active sketch. `select_sketch_profile { profile_id, additive? }` replaces the current sketch-profile selection by default, or toggles the profile when `additive=true` (used by Ctrl/Cmd/Shift-click in the viewport). The document state keeps the legacy `selected_sketch_profile_id` as the most recent selection and also emits `selected_sketch_profile_ids[]` for multi-profile commands. The entity-id variant `{ entity_id, additive?, smallest_only? }` selects every profile whose boundary includes the entity; `smallest_only=true` (the CAM re-pick flow) reduces a shared-boundary click to the smallest owning region, so outline clicks behave like interior clicks.
- `extrude_profile` accepts the legacy single `profile_id`, a `profile_ids[]` array, or `open_entity_ids[]` for thin open-chain extrudes. When multiple closed profiles are provided, the core validates they belong to the same sketch plane. If `mode` is omitted, the core chooses automatically: Join when the extrusion touches an existing body or selected profiles touch each other, Cut when it overlaps an existing body, otherwise New Body. Explicit `new_body` creates one body feature per selected profile. Explicit untargeted `join` groups touching profiles into one body feature while leaving separated profile groups as separate bodies; the stored operation remains `join`, but the grouped body compiles as `new_body` because there is no existing target body. `cut`, `intersect`, and targeted `join` preserve one feature containing all selected regions so the boolean target remains explicit. `extrude_face { face_id, depth, mode?, target_body_id?, parameters? }` creates an extrude from any supported planar body face without requiring a sketch profile. Both creation commands accept `mode` as `new_body | join | cut | intersect` and optional `parameters` with `operation`, `extent_mode`, `side1`, `side2`, `thin`, and `intersect_result`. Existing `depth`, `mode`, and `target_body_id` remain compatibility shorthands. `update_extrude_parameters { feature_id, parameters }` is the full live-preview edit path; `update_extrude_depth`, `update_extrude_mode`, `update_extrude_target_body`, and `update_extrude_profiles` remain compatibility commands.
- `loft_profiles { profile_ids[], ruled? }` creates a new-body loft through two or more sketch profiles in the supplied order. The core resolves each profile to its owning sketch, stores section plane frames and sampled profile loops, and rejects profiles with holes in v1. `update_loft_profiles { feature_id, profile_ids[] }` replaces the section list for an in-progress or edited loft while preserving its ruled/smooth setting. `update_loft_ruled { feature_id, ruled }` toggles smooth versus ruled transitions for live preview. Sketch edits re-resolve loft sections by profile identity and mark the loft `dependency_broken` with a warning if the source region can no longer be matched or rebuilt.
- `revolve_profile { profile_id, axis_entity_id, angle_degrees? }` creates a new body by revolving one sketch profile around a sketch line axis. The profile and axis may come from different sketches. `angle_degrees` defaults to `360`; v1 supports `0 < angle_degrees <= 360`. `update_revolve_profile { feature_id, profile_id }`, `update_revolve_axis { feature_id, axis_entity_id }`, and `update_revolve_angle { feature_id, angle_degrees }` drive live preview and timeline editing. Sketch edits re-resolve the source profile and axis line by id where possible; failures mark the revolve `dependency_broken` with a warning.
- `sweep_profile { profile_id, path_entity_id }` creates a new body by sweeping one closed sketch profile along a sketch path. The profile and path may come from different sketches. The path id may name a line or arc; the core resolves the full connected non-construction line/arc chain containing that entity, rejects branched paths, and stores the ordered world-space path segments for preview/serialization. `update_sweep_profile { feature_id, profile_id }` and `update_sweep_path { feature_id, path_entity_id }` drive live preview and timeline editing. Sketch edits re-resolve the source profile plane and path entity by id where possible; failures mark the sweep `dependency_broken` with a warning.
- sketch profile regions may carry `inner_loops[]` in both `feature_history[].sketch_parameters.profiles[]` and `viewport_state.sketch_profiles[]`. v1 uses this for circles and nested closed polygon profiles inside another polygon profile: the containing region represents the outer area minus the inner loop, while the inner loop remains a separate selectable profile. Selecting both profiles explicitly is therefore the way to extrude the full filled area. Since the exact-arrangement detector, every profile also carries exact `boundary_edges[]` and per-hole `inner_loop_edges[]` (line/arc edges with center/radius/sweep — empty for legacy profiles) in both the document payload and `viewport_state.sketch_profiles[]`; the UI builds the surface fill from these so arc-bounded surfaces render as true arcs instead of the coarse chord samples (`profile_points` remains the legacy fallback).
- the core may emit triangulated body meshes (`viewport_state.meshes` with `primitive_id`, flat `positions`, `normals`, `indices`, `is_selected`, and optional `appearance_color`) so the UI can render boolean'd bodies directly via three.js BufferGeometry instead of reconstructing them from feature primitives; primitives consumed by a Fuse/Cut are suppressed in the legacy `boxes` / `cylinders` / `polygon_extrudes` arrays in the same snapshot. `appearance_color` is also present on body primitives and solid faces when document appearance overrides resolve; face colors take precedence over body colors in the renderer.
- the core also emits `viewport_state.bodies: [{id, label, center, size, local_frame}]` (in document order) so UIs can render stable target pickers and body-local manipulators. The `id` of each body matches the root feature id reported as `target_body_id` on the wire. `center` / `size` come from the current compiled body bounds, and `local_frame` carries the body-local X/Y/Z axes after any replayed Move features.
- the core may emit selectable body edges as `viewport_state.edges: [{id, owner_body_id, kind, points[], is_selected}]` where `id` is `<owner_body_id>:edge:<index>` and `points` is a flat world-space polyline (x0, y0, z0, x1, y1, z1, ...). The UI may raycast against these polylines and dispatch `select_edge` with the picked id; the core then sets `selected_edge_id` on the document state and clears competing selections (face / reference / sketch entities). Edge ids are stable across viewport snapshots when body topology is unchanged, so selection survives mode/depth tweaks
- the core also emits selectable body vertices as `viewport_state.vertices: [{id, owner_body_id, position: {x, y, z}, is_selected}]` where `id` is `<owner_body_id>:vertex:<index>`. The UI raycasts vertex meshes ahead of edges and faces and dispatches `select_vertex` with the picked id; the core then sets `selected_vertex_id` on the document state and clears competing selections. Vertex ids are stable across viewport snapshots under the same conditions as edge ids
- `create_fillet { edge_id, radius }` and `create_chamfer { edge_id, distance }` create body-modifying features owned by the body the edge belongs to. The core resolves the target body from `<owner_body_id>` in the edge id, applies `BRepFilletAPI_MakeFillet` / `BRepFilletAPI_MakeChamfer` during body compilation, and emits the modified body via `viewport_state.meshes`. `update_fillet_radius { feature_id, radius }` and `update_chamfer_distance { feature_id, distance }` drive live preview the same way `update_extrude_depth` does. Fillet and chamfer feature parameters round-trip on `feature_history[].fillet_parameters` / `feature_history[].chamfer_parameters` with `target_body_id`, `edge_ids[]`, and `radius` / `distance`
- `create_shell { face_id, thickness }` creates a body-modifying shell feature owned by the body the selected face belongs to. The selected face is removed as the opening and `BRepOffsetAPI_MakeThickSolid` offsets the remaining solid inward by `thickness`. `update_shell_thickness { feature_id, thickness }` drives live preview, and `confirm_shell { feature_id }` ends the pending pick-stability mode. Shell parameters round-trip on `feature_history[].shell_parameters` with `target_body_id`, `removed_face_ids[]`, `thickness`, and `is_pending`.
- `reenter_sketch` reactivates a finished sketch by feature id without creating a new feature or pushing an undo entry; it only flips the active sketch flags so the UI can resume editing the same plane and entities
- `select_face` is selection only; `start_sketch_on_face` must be driven by a core-provided face id together with the matching core-emitted face plane frame from the viewport snapshot
- the core keeps the sketch plane frame with detected sketch profiles and generated extrusions so face-based loops continue to render and extrude on the selected face rather than being remapped to a perpendicular origin plane
- the core owns the active sketch, active sketch tool including non-drawing selection mode, selected sketch point, selected sketch entity, selected sketch dimension, selected sketch profile, stored sketch entities, stored sketch points including fixed-point state and point-driven edits, stored sketch dimensions, stored sketch line relations, stored sketch profile regions, profile-linked extrude refreshes, and their serialized viewport representation
- the core may emit point-owned constraint markers such as fixed-point badges in the viewport snapshot; the UI may render and clear them through the documented IPC commands, but it must not infer or solve those relations itself
- the core owns selected solid-face ids, the meaning of those ids, and the sketch plane/frame derived from a chosen face

## Versioning

- protocol versioning must be tracked deliberately
- breaking protocol changes require a version bump
- UI and core must agree on protocol version

## Logging and Debugging

- protocol traffic should be easy to inspect during development
- structured messages should remain machine-readable in all environments
- logs should help developers, but they must never become part of the contract

### Parametric Parameters

Document-scoped named numeric parameters that can be referenced by name in
sketch dimension expressions:

- `add_parameter { name, expression, kind? }` — creates a new parameter.
  `name` must be unique (non-empty `[a-zA-Z_][a-zA-Z0-9_]*`). `expression`
  is a simple arithmetic formula (`50`, `width * 2`, `height / 3 + 10`)
  evaluated by a recursive-descent parser in the core. `kind` is
  `"length"` (mm) or `"angle"` (degrees), defaulting to `"length"`. The
  resolved value is stored as `resolved_value` and re-evaluated on every
  parameter change. Rejects duplicate names.

- `update_parameter { name, expression, kind? }` — replaces the
  expression and/or kind of an existing parameter. Re-evaluates all
  parameters (those referencing the changed one cascade) and re-resolves
  dimension expressions across all sketch features.

- `delete_parameter { name }` — removes a parameter. Parameters that
  referenced the deleted one will have `has_error = true` and show an
  error in the UI until their expression is updated.

Parameters are stored in `document_state.parameters[]` and serialized
inside `.polysmith` files. Older files without the `parameters` key load
with an empty array.

### Dimension Expressions

The `update_sketch_dimension` command's `value` field now accepts either
a plain number (backward compatible) or a string expression referencing
parameters by name. When a string is supplied, the core evaluates it
against the current parameter table, resolves the value, and stores the
expression on the dimension. On any parameter change, dimension
expressions are re-evaluated to keep sketch geometry in sync.

Kind checking: the expression resolver validates that angle-type parameters
(`kind = "angle"`) are only used in angle-type dimensions (`"angle"` or
`"line_angle"`). Using an angle parameter in a length dimension produces
a descriptive error.

### Selection Filter

The `update_selection_filter` command allows the UI to control which
geometric element types are visible, selectable, and snappable. The
payload is a flat object of optional boolean fields:

```
{
  type: "update_selection_filter",
  payload: {
    select_curves: true,
    select_points: true,
    select_construction: false,
    select_constraints: true,
    snap_endpoint: true,
    snap_midpoint: true,
    snap_center: true,
    snap_intersection: true,
    snap_nearest: true,
    snap_quadrant: false,
    snap_perpendicular: false,
    snap_parallel: false,
    snap_tangent: true,
    snap_grid: true,
    magnetic_pull: true,
    tolerance_px: 10
  }
}
```

All fields are optional — omitted fields retain their current value. The
filter is stored on `DocumentState.selection_filter` (serialized in
`.polysmith` files, backward-compat absent → defaults) and echoed in
`viewport_state.selection_filter` so the renderer can gate snap /
selection behavior without an extra IPC round-trip.

The UI reads the filter from localStorage for instant snap gating
(synchronous, no IPC latency), but also sends the full payload via IPC
so the core stays consistent.

### Drawing Workspace (ISO drawing)

All drawing state lives in `document_state.drawing` (a
`DrawingDocumentData`: drawings → sheets → views → annotations),
serialized inside `.polysmith` files under the `drawing` key.  The data
model mirrors the ISO 7200 title block, ISO 5456 per-sheet projection
angle, ISO 5455 scales, and ISO 129-1 decimal separator.

- `drawing_create { drawing_id?, name, sheets[], views[], annotations[] }`
  — creates a drawing (always with at least one sheet) and makes it the
  active drawing.  Empty `drawing_id` / `sheet_id` / `view_id` /
  `annotation_id` fields are minted by the core (`drawing-N`,
  `drawing-sheet-N`, `drawing-view-N`, `drawing-annotation-N`).
- `drawing_delete { drawing_id }` — removes the drawing and its
  sheets/views/annotations; active/selection ids pointing inside it are
  cleared.
- `drawing_set_active { drawing_id }` — switches the active drawing.
- `drawing_sheet_create { drawing_id, sheet }` — appends a sheet.
  Referenced `view_ids` must already exist in the drawing (rejected
  before the undo push).
- `drawing_sheet_delete { drawing_id, sheet_id }` — removes the sheet,
  its views, and the annotations attached to those views.
- `drawing_view_create { drawing_id, sheet_id, view }` — creates a view
  on the sheet (mints the view id, appends it to the sheet's ordering).
  `view.kind` is `"projection"` (with `standard_view` — front/right/
  left/top/bottom/back — or a `custom_frame`), `"axonometric"`
  (requires `custom_frame`), `"section"` (requires `view.section` —
  the frame derives from the cutting plane; a standard view or custom
  frame must NOT override it), or `"detail"` (requires `view.detail` —
  see the Detail Views paragraph below).  The bump inside the command
  re-projects the view through the drawing refresh pass.
- `drawing_view_update { drawing_id, view }` — replaces the view's
  definition (same id, same sheet); annotations keep their
  attachments and re-resolve on the next refresh.
- `drawing_view_delete { drawing_id, view_id }` — removes the view,
  its annotations, its detail-view CHILDREN (cascade, Fusion
  behavior), and its id from every sheet's ordering list.
- `drawing_view_move { drawing_id, view_id, sheet_position: [x, y] }`
  — moves the view origin on its sheet (sheet-mm).  Purely cosmetic —
  never re-projects.
- `drawing_view_preview { drawing_id, sheet_id, view }` — NON-mutating
  Insert View ghost: the core projects + flattens an UNCOMMITTED view
  definition (the same engine, coincidence and dash passes as a
  committed view) and replies with `drawing_view_preview_result`
  `{drawing_id, sheet_id, curves[], texts[], view}` where `view` is a
  committed-view-shaped record with an EMPTY `view_id` (the ghost
  marker) plus `label`, `scale`, `origin`, content `min`/`max`, and a
  `warning` (non-empty = degraded projection — no geometry).  Sibling
  sections trace their cutting planes onto the preview; the preview's
  own uncommitted section does not.  Never mutates, never caches.
- `drawing_section_update { drawing_id, view_id, section }` — replaces
  a section view's `SectionDefinition` (cutting plane point/normal,
  `cut_away`, label, hatch angle/spacing).  The view must be kind
  `"section"`; a degenerate normal or non-positive hatch spacing is
  rejected before the undo push.  The bump re-cuts and re-projects.

### Detail views (ISO 128-3 §4.12)

- A `"detail"` view carries `view.detail` — `{parent_view_id, center:
  [x, y], radius, label}` where `center` and `radius` are in the
  PARENT's view-mm (its projection plane, before the parent scale).
  The detail stores its own `scale`, `sheet_position`,
  `source_body_ids`, and `show_hidden` (the UI copies them from the
  parent); it has no standard view or custom frame.
- The refresh derives the detail's projection in a SECOND pass after
  the main view loop: the parent's cached projection (at the target
  revision) is clipped to the circle (`clip_projection_to_circle`)
  and stored under the detail's own view id.  A missing / broken
  parent degrades the detail with `dependency_broken` + `broken_ref =
  <parent_view_id>` + a warning while holding its last-known content
  — the same TNP ladder as every other reference (deleting the
  source body degrades parent AND detail together).  Witnesses ride
  the clip verbatim; ellipses/bsplines crossing the window are
  dropped (documented V1 limit).
- Flatten emits on the DETAIL's sheet: a thin boundary circle at the
  detail's scale+position (purpose `detail_boundary`) and the label
  text `"<label> (<scale>)"` below it (purpose `detail_label`); the
  view's content bounds are the circle's extent.  On the PARENT's
  sheet (cross-sheet parents included): a marker circle over the
  window (purpose `detail_boundary`) + the bare letter (purpose
  `detail_label`).  The viewport view list and its hit tests reuse
  the existing `label` field — no viewport payload change.  DXF puts
  both purposes on the ANNOTATION layer (SVG/PDF are
  purpose-agnostic).
- Validation (before the undo push): the parent must exist and be a
  plain `"projection"` view, and `radius > 0`.  Deleting a parent
  cascade-deletes its detail children.  `drawing_dimension_create`
  and `drawing_annotation_create` refuse `"detail"` views (dimension
  the parent instead) — V1 limits: no section parents, no
  detail-of-detail.
- `drawing_title_block_update { drawing_id, sheet_id, title_block }`
  — replaces the sheet's ISO 7200 title block data: the eight
  mandatory fields (legal owner, identification number, date of
  issue, title, approval person, creator, document type, segment/
  sheet number) plus `revision_rows` (each `[zone, rev, description,
  date, approved]`).  Purely cosmetic — the bump re-flattens only.
  The scale auto-fills from the sheet's FIRST view at flatten time;
  nothing is re-projected.
- `drawing_export { drawing_id, sheet_id, format, file_path,
  dxf_mode? }` — NON-mutating sheet export (see the P8/P9 paragraphs
  below); replies with `document_exported` `{file_path, format,
  exported_feature_count}`.  `dxf_mode` is `"geometry"` (default) or
  `"annotated"`.
- `drawing_sheet_update { drawing_id, sheet_id, paper_size, orientation,
  projection_angle, name }` — edits the sheet: `paper_size` A0–A4,
  `orientation` portrait/landscape, `projection_angle` `"first_angle"`
  (default) or `"third_angle"`, and the display `name`.  Values outside
  the four sizes / two orientations / two angles are rejected before
  the undo push; `paper_size_mm` resolves the trimmed ISO 5457
  dimensions for the combination (portrait = width×height as listed,
  landscape swaps).  The bump re-flattens the sheet.
- `drawing_dimension_create { drawing_id, view_id, dim_type, pick,
  pick_2?, annotation_id? }` — creates (or, with `annotation_id`,
  repairs) a dimension: `dim_type` linear/angular/radius/diameter,
  `pick`/`pick_2` are SHEET-mm points resolved against the view's
  CURRENT projection — the core mints the `SourceEdgeWitness` from the
  nearest record's provenance and the persistent edge reference id
  (`drawing-edge-N`).  A pick is never a stored ordinal (the TNP
  mantra).  Kind-vs-geometry rules (linear needs a straight edge,
  diameter only for arcs > 180°, angular needs two non-parallel
  lines, coincident edges from different sources refuse) throw BEFORE
  the undo push.
- `drawing_dimension_update { drawing_id, annotation_id,
  text_override?, prefix?, arrow_flip?, text_offset? }` — cosmetic
  edits only (never re-projects, never re-resolves); the bump
  re-flattens.  `text_offset` is the dimension's PLACEMENT POINT
  (sheet-mm, from the default text anchor): the text lands at
  default + offset, and the dimension line / extension lines /
  leader / arc derive from that final text position (linear single:
  the perpendicular component moves the dimension line and stretches
  the extension lines, the parallel component slides the text along
  the line; radius/diameter: the leader/dimension line re-aims
  through the dragged text, the center stays fixed; angular: the arc
  radius follows the text distance, clamped to the 12 mm minimum).
  The mouse drag commits one `text_offset` update per drop.
- `drawing_dimension_delete { drawing_id, annotation_id }` — removes
  the annotation and its cached value.
- `drawing_dimension_preview { drawing_id, view_id, dim_type, pick,
  pick_2? }` — NON-mutating: replies with a
  `drawing_dimension_preview` event `{value, text_value, curves[],
  text, error?}` — the core-computed value plus the sheet-mm graphics
  the UI renders until Enter commits the create.
- `drawing_note_create { drawing_id, sheet_id, text, position,
  height_mm?, angle_deg?, h_align? }` — creates a sheet-anchored free
  text note (`SheetNote`, id `drawing-note-N`); `position` is the
  text CENTER in sheet-mm (rounded to 0.01).
- `drawing_note_update { drawing_id, note_id, text?, position?,
  height_mm?, angle_deg?, h_align? }` — edits the note (the mouse
  drag commits one absolute `position` per drop, sheet-index offset
  subtracted UI-side).
- `drawing_note_delete { drawing_id, note_id }` — removes the note.
  `drawing_sheet_delete` cascades to its notes.
- `drawing_annotation_create { drawing_id, view_id, kind, pick,
  pick_2?, text_override?, prefix?, extensions?, placement? }` —
  creates a model-anchored annotation: `kind` one of `leader_text`,
  `center_mark`, `centerline`, `edge_extension`, `surface_finish`,
  `welding`, `tolerance_frame`, `datum`, `balloon`.  `pick`/`pick_2`
  are SHEET-mm points resolved against the view's CURRENT projection
  like dimensions (the TNP mantra) — the core mints the
  `SourceEdgeWitness` plus `attach_param` (0..1 fraction over the
  witness's param range).  Per-kind geometry rules (center_mark needs
  a circular witness, centerline two, edge_extension a straight edge
  with the fraction snapped to the picked end) throw BEFORE the undo
  push.  `placement` (leader_text) is the text's absolute anchor —
  the core computes `text_offset = placement − default anchor`.
- `drawing_annotation_update { drawing_id, annotation_id,
  text_override?, prefix?, arrow_flip?, text_offset?, extensions?,
  attach_param? }` — cosmetic edits only (never re-projects); the
  drag commits one `text_offset` per drop (the leader re-aims through
  the dragged text).
- `drawing_annotation_delete { drawing_id, annotation_id }` — removes
  the annotation and its cached attachment.
- `drawing_annotation_preview { drawing_id, view_id, kind, pick,
  pick_2?, text_override?, prefix? }` — NON-mutating: replies with a
  `drawing_annotation_preview` event `{text_value, curves[], text,
  error?}` — the same live-preview contract as dimensions, keyed by
  `kind` instead of `dim_type`.

Section views (P4): the refresh cuts every source body with a
half-space when `section.cut_away` is true (material on the normal
side is removed) and suppresses hidden edges entirely (ISO 128-3 §7).
The hatch boundary is the cut face's wires at the cutting plane
(`cut_away = false` uses the uncut body's cross-section instead); the
scanline hatching (thin lines, `hatch_angle_deg`/`hatch_spacing_mm`)
is computed by the shared `compute_hatch_segments` engine function.
Every OTHER view of the drawing that sees a sibling section's cutting
plane edge-on carries the cutting-plane trace (a type-H chain line,
`curve_class: "cutting_plane"`) in its projection.

Sheets (P5): `flatten_sheet(document, drawing_id, sheet_id)` produces
the `SheetPrimitiveStream` that the viewport emission (`drawing_sheets`
in the viewport payload) consumes — the sheet furniture (0.7 mm frame
at 20/10 mm margins, centring marks, grid-reference ticks, the
ISO 7200 title block with the ISO 5456-2 projection symbol inside it
honoring the per-sheet angle) plus every view's geometry transformed
into sheet-mm with the ISO 128-2 line styles ALREADY applied (dash
patterns recalculated at corners per Annex A — every dash sequence
starts and ends with a dash; hidden = dashed thin, cutting-plane =
chain thin, hatching = continuous thin emitted last).  Coincident
geometry is de-duplicated by the priority visible > hidden >
cutting_plane > hatch on the undashed records, and cutting-plane
traces lying on a visible/hidden line are dropped (the support-overlap
rule).  The stream is deterministic and memory-only (golden-file
pinned).

Dimensions (P6): the refresh pass resolves each annotation against
its view's fresh projection through the witness ladder — identity
(body + edge index + kind, the topology-stable fast path that follows
parametric edits) → strict body+geometry (0.01 mm) → relaxed geometry
(0.1 mm, re-created features) → ambiguous (multiple distinct sources
match → refuse) → not found (dependency_broken + warning + the
last-known value kept, marked stale — never blank, never silently
substituted).  Measured values and attachment geometry live in the
runtime cache (memory-only, the projection contract).  The flatten
emits the ISO 129-1 graphics: extension lines (8×d gap/overshoot),
closed filled arrowheads (sheet-mm `filled_poly` primitives), the
unbroken dimension line, and a `texts[]` record (3.5 mm lettering,
unidirectional, ⌀/R prefixes, decimal separator — "." default, a
stored "," from before the dot default is migrated on load — ° on
angles).  Dimension `texts[]` records carry the owning
`annotation_id` (title-block and section-label texts do not) so the
UI can hit-test a picked text back to its annotation.  A cosmetic
`drawing_dimension_update` never re-projects — it only re-flattens.

Notes and annotations (ANNOTATE/GEOMETRY/SYMBOLS tabs): sheet notes
flatten as `texts[]` records (purpose `"note"`, `annotation_id` =
note id, absolute position).  Annotations ride the SAME witness ladder
and runtime-cache pattern as dimensions — identity → strict 0.01 mm →
relaxed 0.1 mm → broken (`dependency_broken` + warning + last-known
placement kept, marked stale; a broken annotation's graphics are
suppressed, its text only).  The refresh/flatten pass branches on
`is_annotation_kind()` so the new kinds never enter the dimension
measurer.  Annotation graphics (thin lines, arrows, symbol geometry)
are emitted as primitives with purpose `"annotation"` after the
dimension primitives, and their texts with purpose `"annotation"`;
the DXF exporter layers purpose `"annotation"` on ANNOTATION.

Kind semantics (all sheet-mm offsets, view-scale free): leader_text =
arrow + leader + text (drag re-aims the leader); center_mark = thin
cross ±2.5 mm on a circular witness's center (no text, no
attach_param); centerline = ISO 128-2 chain line through two circles'
centers ±3 mm overshoot (the emitter pre-dashes — annotation
primitives bypass the view-geometry dashing pass; concentric circles
refuse); edge_extension = thin line 5 mm outward from the picked end
(attach_param snaps to 0/1 and the extension roots on the SNAPPED
end); surface_finish = ISO 1302 check mark + value text; welding =
arrow + ISO 2553 reference line + symbol text above its left end;
tolerance_frame = ISO 1101 two-compartment frame (14×7 mm) + centered
text; datum = filled triangle + letter box; balloon = circle r3.5 +
leader + centered number.  Annotation picks prefer the REAL edge when
a silhouette coincides with it (a rim circle in an axis view) — the
ambiguity rule relaxes by source TYPE only, never between two real
edges.

Title block + drawing text (P7): the flatten fills the 180×63 mm
ISO 7200 block bottom-right inside the frame — the eight mandatory
fields as `texts[]` records (purpose `"title_block"`), the scale
auto-filled from the sheet's FIRST view (ISO 5455 formatting with the
drawing's decimal separator), `Sheet x/y` from the sheet index, the
"Dimensions in millimetres" + ISO 8015 notes, the projection symbol
in the top-right cell, and the revision table (zone/rev/description/
date/approved) stacked above the block while rows exist.  Every
surviving cutting-plane trace additionally emits its section label at
both ends (purpose `"section_label"`: the label letter + a filled
arrow pointing along the section's sight direction).  Every `texts[]`
record ALSO emits vector glyph line primitives (purpose
`"text_glyph"`, thin continuous) laid out by the core text engine
with the bundled OSIFONT single-stroke font (LGPL v3 + font-embedding
exception; `POLYSMITH_DRAWING_FONT_PATH` overrides the path) — the
viewport and the PDF/SVG backends draw the glyphs, the DXF backend
emits the DATA record as real text.  Glyph contours are sorted by a
geometric key so the stream stays reproducible across runs (OCCT's
glyph face order is hash-dependent).

Export (P8): `drawing_export { drawing_id, sheet_id, format, file_path }`
is NON-mutating — it flattens the sheet from the current runtime
projections (never re-projects, never pushes undo, never bumps the
revision) and replies with the `document_exported` event.  `format`
is `"svg"` | `"dxf"` | `"pdf"`.  The SVG backend writes an mm
`viewBox` at the sheet size with the y-axis flipped to SVG's screen
convention; circle arcs become `A` path segments (sweep 0 — math-CCW
appears counter-clockwise on screen after the flip), ellipse arcs are
tessellated, filled arrowheads become `<polygon>` elements, and the
vector glyph primitives render the text (the text records stay DATA).
The DXF backend writes ASCII R2013 (AC1027): named layers (VISIBLE
0.5, HIDDEN/CUTTING/HATCH/ANNOTATION/FURNITURE 0.25, FRAME 0.7, TEXT)
with per-entity lineweights, the pre-dashed segments on CONTINUOUS
layers (the ISO patterns are baked in by the core — the HIDDEN/CHAIN
linetypes are still DEFINED for user reuse), text records as real
DRW_Text entities (glyph primitives skipped — the drawing must not
carry its text twice), filled polygons fanned into SOLID quads, and
the title block as the registered `POLYSMITH_TITLE_BLOCK` block +
INSERT (writeBlockRecord precedes writeBlock — the libdxfrw UB trap).
Unknown ids/formats and I/O failures throw structured errors.

Export (P9): the PDF backend (libharu 2.4.4, vendored with zlib
1.3.1 in `third_party/`) writes the sheet at 1:1 sheet size in mm:
lines/circles/arcs as PDF path operators (full circles via
`HPDF_Page_Circle` — `HPDF_Page_Arc` rejects ≥360° sweeps; both only
APPEND the path, so every primitive strokes explicitly), ellipse arcs
tessellated like the SVG backend, filled polygons as filled paths.
Drawing text is REAL selectable text from the bundled OSIFONT font
(subset-embedded, `/FontFile2`, `HPDF_UseUTFEncodings` for UTF-8 —
⌀/±/° round-trip as UTF-16BE in the content stream, compressed with
`HPDF_COMP_ALL`); when the font cannot be loaded the stream's vector
glyph primitives are drawn instead (libharu's raised load error is
consumed by that fallback).  All drawing texts are horizontal today,
so real-text mode requires every record horizontal — otherwise the
glyphs carry the whole sheet.

DXF `dxf_mode: "annotated"` replaces the exploded dimension graphics,
dimension texts and hatch scanlines with real **DIMENSION** entities
(DIMALIGNED/radial/diametric/2-line angular, formatted text, style
reference) under a **DIMSTYLE** named `POLYSMITH_ISO` (closed filled
arrows — empty dimblk + dimtsz 0, text above the line, 3.5 mm text,
8×d extension offsets, dimdsep mirrored from the document's decimal
separator) and **HATCH** entities (ANSI31 predefined pattern at the
section's angle; the spacing becomes the pattern scale; boundary
loops decomposed to LINE edges — libdxfrw's polyline hatch loops are
unimplemented).  The semantic records ride the flattened stream
(`SheetDimension`, `SheetHatchRegion`, populated by the dimension
graphics builder and the section flatten — one computation, one
source of truth); geometry-mode backends ignore them.

Every mutator replies with a `document_state` event; validation errors
reply with an `error` event.  Views are re-projected inside the single
existing refresh pass (`bump_geometry_revision`): the refresh resolves
the view frame (standard view or custom frame), compiles the source
bodies, and stores the HLR projection in the runtime cache stamped
with the upcoming revision.  A missing source body degrades the view
with `broken_ref` + `warning` and holds its last-known projection
marked stale — never a crash, never a silent substitute.  Generated
projections (HLR output) are **memory-only** — they live in the
core's `drawing_runtime` cache, keyed by document + view and
validated against the document revision, and never enter the
serialized document (the CAM toolpath contract).

Templates (CREATE DRAWING dialog): setup-only JSON files —
`{ name, sheets: [{ name, paper_size, orientation, projection_angle,
title_block }] }` — with no ids, view ids, views or annotations (those
are minted at `drawing_create` time).  The core owns the file I/O
(the cam_tool_import_file precedent); the UI picks paths and feeds a
loaded template back through the normal `drawing_create` path.

- `drawing_template_save { file_path, template }` — validates the
  template (non-empty name, ≥1 sheet, the exact A0–A4 / portrait /
  landscape / first_angle / third_angle enums — the drawing_sheet_update
  rules) and writes it as pretty JSON; replies with a
  `drawing_template_save_result` event `{file_path}`.  Validation and
  I/O failures reply with `error` events (`DRAWING_TEMPLATE_SAVE_FAILED`).
- `drawing_template_load { file_path }` — reads + parses the file,
  applies the same lenient defaults as the drawing payload parser
  (missing keys never fail) and the same validation; replies with a
  `drawing_template_load_result` event `{template: {name, sheets[]}}`
  (full sheet payloads including title blocks, ids empty).  Errors
  reply with `error` events (`DRAWING_TEMPLATE_LOAD_FAILED`).
- The Tauri shell exports the user templates directory as
  `POLYSMITH_TEMPLATES_DIR` (`<app_data>/templates`) — the dialog's
  picker default.

## Philosophy

The IPC protocol is the contract of the system.

If the protocol stays clean:

- the architecture stays clean
- the UI stays focused on presentation and user intent
- the core stays responsible for CAD behavior
- the codebase stays understandable and maintainable
