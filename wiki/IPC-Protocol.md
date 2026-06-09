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
- user-facing mesh export is body-scoped; the File menu exposes STEP only, while body context menus expose mesh export for the selected body
- the STL exporter triangulates the requested body or compound with a fixed linear/angular deflection before writing; the UI does not generate any tessellation itself
- the core replies with `document_exported` when the export succeeds; the payload's `format` field reflects the writer that ran (`step` or `stl`)
- the UI must not reconstruct geometry or write CAD files itself

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

The protocol also covers native document persistence and the Project sketch
tool:

- `create_plugin_feature { plugin_id, feature_type, display_name, parameters_summary, parameters, geometry }`,
  `update_plugin_feature { feature_id, plugin_id, feature_type, display_name, parameters_summary, parameters, geometry }`,
  and `confirm_plugin_feature { feature_id }` add the trusted plugin feature
  boundary. Plugins own UI, defaults, domain validation, and the serialized
  `parameters` JSON. The native core owns the resulting CAD state by storing
  `feature_history[].plugin_feature_parameters` and interpreting the ordered
  generic `geometry` recipe during recompute. Geometry operations are `add` or
  `subtract`; primitives are `box`, `rounded_box`, `tapered_rounded_box`, or
  `cylinder`; coordinates use plugin-local X/Y for the footprint plane and Z
  for height. `tapered_rounded_box` accepts optional `top_width`, `top_depth`,
  and `top_radius` fields for centered lofted profiles. The core must not
  contain plugin-specific modeling code.
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

- the UI may send selection or sketch intent such as `select_face`, `start_sketch_on_face`, `start_sketch_on_plane`, `set_sketch_tool`, `update_sketch_line`, `update_sketch_point`, `set_sketch_line_constraint`, `set_sketch_equal_length_constraint`, `set_sketch_coincident_constraint`, `set_sketch_perpendicular_constraint`, `set_sketch_parallel_constraint`, `set_sketch_point_fixed`, `update_sketch_circle`, `update_sketch_dimension`, `update_sketch_dimension_label_position`, `add_sketch_angle_dimension`, `add_sketch_distance_dimension`, `add_sketch_point_distance_dimension`, `add_sketch_line_length_dimension`, `add_sketch_circle_radius_dimension`, `add_sketch_polygon_radius_dimension`, `add_sketch_line`, `add_sketch_rectangle`, `add_sketch_circle`, `add_sketch_polygon`, `add_sketch_arc`, `select_sketch_point`, `select_sketch_entity`, `select_sketch_dimension`, `select_sketch_profile`, `extrude_profile`, `extrude_face`, `update_extrude_depth`, `loft_profiles`, `update_loft_profiles`, `update_loft_ruled`, `revolve_profile`, `update_revolve_profile`, `update_revolve_axis`, `update_revolve_angle`, `sweep_profile`, `update_sweep_profile`, `update_sweep_path`, `finish_sketch`, or `reenter_sketch`
- `select_sketch_entity { entity_id, additive? }` selects a sketch edge entity (line / circle / arc). Plain selection replaces the selected sketch entity list; additive selection toggles the entity in `selected_sketch_entity_ids[]` and stores the most recent selected entity in `selected_sketch_entity_id` for compatibility.
- `select_sketch_point { point_id, additive? }` selects a sketch vertex / point. Plain selection replaces the selected sketch point list; additive selection toggles the point in `selected_sketch_point_ids[]` and stores the most recent selected point in `selected_sketch_point_id` for compatibility.
- `add_sketch_line { start_x, start_y, end_x, end_y, is_construction? }` creates a sketch line on the active sketch. Construction lines render dashed, stay available for snapping / constraints, and are excluded from profile loop detection and automatic line dimensions.
- `add_sketch_rectangle { start_x, start_y, end_x, end_y, is_construction? }` creates four sketch lines. When `is_construction` is true, all four sides are construction lines and therefore do not seal selectable profiles or receive automatic side dimensions.
- `add_sketch_circle { center_x, center_y, radius, is_construction? }` creates a sketch circle. Construction circles render dashed, stay selectable / snappable, are excluded from profile and hole detection, and do not receive an automatic diameter dimension.
- `add_sketch_polygon { sides, mode, start_x, start_y, end_x, end_y, is_construction? }` creates a regular N-sided polygon on the active sketch. `mode` is one of `inscribed` (center + vertex), `circumscribed` (center + apothem), or `edge` (two edge endpoints). The core computes vertices in sketch-local space from the supplied parameters and rejects `sides < 3` as a structured error. Non-construction polygons receive an automatic radius dimension.
- `add_sketch_arc { start_x, start_y, end_x, end_y, anchor_x, anchor_y, mode, is_construction? }` creates a sketch arc on the active sketch. `mode` is one of `three_point` (anchor lies on the arc; center = circumcenter of start, anchor, end) or `center_start_end` (anchor is the center; end is snapped onto the resulting circle). Endpoints participate in the shared sketch-point graph and are stored as fixed (v1 freezes arc shape at creation; reshape requires delete + redraw). The core rejects colinear / zero-radius input as a structured error. Non-construction arc edges contribute to closed-profile loop detection alongside lines, with interior points sampled into the profile so OCCT extrudes a clean curved boundary; construction arcs are skipped by profile detection.
- `add_sketch_angle_dimension { first_line_id, second_line_id }` creates or reselects a line-line angle dimension. The native core validates the two lines share an endpoint and owns the subsequent `update_sketch_dimension` solve.
- `add_sketch_distance_dimension { first_entity_id, second_entity_id }` creates or reselects a distance dimension for parallel line-to-line, circle-center to circle-center, and circle-center to line picks. The native core owns the solve behavior; the UI only sends picked entity ids and edits the returned dimension value.
- `update_sketch_dimension_label_position { dimension_id, label_x, label_y }` stores a sketch-local label placement override for a dimension. The core treats this as presentation metadata only; it does not affect the dimension's solved value.
- `add_sketch_point_distance_dimension { point_a_id, point_b_id }` creates or reselects a straight-line distance dimension between two sketch points. The native core validates both points exist and pushes a `SketchDimension` of kind `point_distance` with the current euclidean distance as its initial value.
- `add_sketch_line_length_dimension { line_id }` creates a length dimension on a single sketch line that doesn't already have one. Validates the entity exists and is not construction, checks for duplicate `dim-line-{id}`, and pushes a `SketchDimension` of kind `line_length` with the current geometric length. Used by the Dimension tool when the user clicks a line whose auto-dimension was deleted.
- `add_sketch_circle_radius_dimension { circle_id, display_as? }` creates a radius dimension on a single sketch circle. Same pattern: validates, deduplicates, pushes a `SketchDimension` of kind `circle_radius`. An optional `display_as` parameter (`"radius"` / `"diameter"`) controls rendering. Used by the Dimension tool when the user clicks a circle.
- `add_sketch_polygon_radius_dimension { polygon_id }` creates a radius dimension on a single sketch polygon. Same pattern: validates, deduplicates, pushes a `SketchDimension` of kind `polygon_radius`. Used by the Dimension tool when the user clicks a polygon.
- `viewport_state.sketch_circles: [{circle_id, plane_id, plane_frame, center, radius, is_selected, is_construction, is_preview}]` carries world-space circle centers plus the sketch-plane radius. `plane_frame` is nullable for legacy origin-plane sketches and present for face/custom-plane sketches so the UI can render inactive/projected circles in their real 3D sketch plane. The corresponding feature-level state lives at `feature_history[].sketch_parameters.circles[]` with local center / radius / `is_construction`.
- `viewport_state.sketch_polygons: [{polygon_id, plane_id, plane_frame, corner_x[], corner_y[], corner_z[], sides, mode, center, radius, is_selected, is_construction, is_preview}]` carries world-space corner arrays and the polygon's center/radius/sides/mode. The UI draws a closed `THREE.Line` loop from the corner arrays. The corresponding feature-level state lives at `feature_history[].sketch_parameters.polygons[]` with local center/radius/sides/mode and the two endpoints that defined the shape.
- `viewport_state.sketch_arcs: [{arc_id, start_point_id, end_point_id, plane_id, plane_frame, center, radius, start, end, ccw, is_selected, is_construction, is_preview}]` carries the world-space endpoint and center coordinates plus the sweep direction (`ccw`); the UI samples between `start` and `end` around `center` using `plane_frame` when available. The corresponding feature-level state lives at `feature_history[].sketch_parameters.arcs[]` with the same shape but in sketch-local 2D coordinates
- `add_sketch_fillet { corner_point_id, line_a_id, line_b_id, radius }` rounds a corner shared by two sketch lines into a tangent arc. The corner is identified by the shared sketch point id; the core validates strict eligibility (corner is an endpoint of both lines, lines are non-parallel, radius fits on each line, no other fillet already at this corner) and rejects with a structured error otherwise. On success the core mutates each line's filleted endpoint to reference a newly allocated fixed trim point and inserts a generated `SketchArc` between them. The relationship is parametric: a `SketchFillet` record on the sketch carries enough state to keep the geometry tangent under subsequent line edits and to fully restore the original corner on delete
- `update_sketch_fillet_radius { fillet_id, radius }` rewrites the parametric radius and re-runs the sketch recompute pass; the trim distances and arc geometry update in lockstep. If the new radius no longer fits on the current line lengths the recompute silently skips the update (leaving the previous frame's geometry intact) — the user can drag the lines longer to recover
- `delete_sketch_fillet { fillet_id }` restores each line's filleted endpoint back to the original corner point and removes the generated arc + the fillet record. The corner point is re-emitted by the next `rebuild_sketch_points` from the fillet's cached `corner_x` / `corner_y` (denormalized onto the fillet record specifically so the points table can survive the case where no other entity references the corner)
- `delete_sketch_selection { entity_ids[], point_ids[], profile_ids[] }` deletes selected sketch geometry from the active sketch. Entity ids may reference sketch lines, circles, or arcs. Point ids resolve to their owned geometry: line / arc endpoints delete connected edges, circle center points delete the circle, and projected standalone points delete that point. Profile ids resolve to the profile's core-owned boundary geometry, so selecting a whole sketch region can remove that shape. The core removes dangling dimensions, relations, anchors, projection links, and generated fillet records as needed, then recomputes sketch points, profiles, and profile-linked extrudes. The UI may warn before sending the command when the active sketch has downstream dependents, but the actual mutation stays core-owned.
- `feature_history[].sketch_parameters.fillets: [{fillet_id, corner_point_id, corner_x, corner_y, line_a_id, line_b_id, trim_a_point_id, trim_b_point_id, arc_id, radius}]` round-trips through save / load so the parametric model survives across sessions. The generated trim points appear in `points[]` (with `is_fixed=true`) and the generated arc in `arcs[]`, just like any other sketch geometry, but consumers that need to know they're fillet outputs (not user-drawn) can cross-reference by id
- `select_sketch_profile` and `extrude_profile` accept any profile in the document (the owning sketch is resolved by the core); they do not require an active sketch. `select_sketch_profile { profile_id, additive? }` replaces the current sketch-profile selection by default, or toggles the profile when `additive=true` (used by Ctrl/Cmd/Shift-click in the viewport). The document state keeps the legacy `selected_sketch_profile_id` as the most recent selection and also emits `selected_sketch_profile_ids[]` for multi-profile commands.
- `extrude_profile` accepts the legacy single `profile_id`, a `profile_ids[]` array, or `open_entity_ids[]` for thin open-chain extrudes. When multiple closed profiles are provided, the core validates they belong to the same sketch plane. If `mode` is omitted, the core chooses automatically: Join when the extrusion touches an existing body or selected profiles touch each other, Cut when it overlaps an existing body, otherwise New Body. Explicit `new_body` creates one body feature per selected profile. Explicit untargeted `join` groups touching profiles into one body feature while leaving separated profile groups as separate bodies; the stored operation remains `join`, but the grouped body compiles as `new_body` because there is no existing target body. `cut`, `intersect`, and targeted `join` preserve one feature containing all selected regions so the boolean target remains explicit. `extrude_face { face_id, depth, mode?, target_body_id?, parameters? }` creates an extrude from any supported planar body face without requiring a sketch profile. Both creation commands accept `mode` as `new_body | join | cut | intersect` and optional `parameters` with `operation`, `extent_mode`, `side1`, `side2`, `thin`, and `intersect_result`. Existing `depth`, `mode`, and `target_body_id` remain compatibility shorthands. `update_extrude_parameters { feature_id, parameters }` is the full live-preview edit path; `update_extrude_depth`, `update_extrude_mode`, `update_extrude_target_body`, and `update_extrude_profiles` remain compatibility commands.
- `loft_profiles { profile_ids[], ruled? }` creates a new-body loft through two or more sketch profiles in the supplied order. The core resolves each profile to its owning sketch, stores section plane frames and sampled profile loops, and rejects profiles with holes in v1. `update_loft_profiles { feature_id, profile_ids[] }` replaces the section list for an in-progress or edited loft while preserving its ruled/smooth setting. `update_loft_ruled { feature_id, ruled }` toggles smooth versus ruled transitions for live preview. Sketch edits re-resolve loft sections by profile identity and mark the loft `dependency_broken` with a warning if the source region can no longer be matched or rebuilt.
- `revolve_profile { profile_id, axis_entity_id, angle_degrees? }` creates a new body by revolving one sketch profile around a sketch line axis. The profile and axis may come from different sketches. `angle_degrees` defaults to `360`; v1 supports `0 < angle_degrees <= 360`. `update_revolve_profile { feature_id, profile_id }`, `update_revolve_axis { feature_id, axis_entity_id }`, and `update_revolve_angle { feature_id, angle_degrees }` drive live preview and timeline editing. Sketch edits re-resolve the source profile and axis line by id where possible; failures mark the revolve `dependency_broken` with a warning.
- `sweep_profile { profile_id, path_entity_id }` creates a new body by sweeping one closed sketch profile along a sketch path. The profile and path may come from different sketches. The path id may name a line or arc; the core resolves the full connected non-construction line/arc chain containing that entity, rejects branched paths, and stores the ordered world-space path segments for preview/serialization. `update_sweep_profile { feature_id, profile_id }` and `update_sweep_path { feature_id, path_entity_id }` drive live preview and timeline editing. Sketch edits re-resolve the source profile plane and path entity by id where possible; failures mark the sweep `dependency_broken` with a warning.
- sketch profile regions may carry `inner_loops[]` in both `feature_history[].sketch_parameters.profiles[]` and `viewport_state.sketch_profiles[]`. v1 uses this for circles and nested closed polygon profiles inside another polygon profile: the containing region represents the outer area minus the inner loop, while the inner loop remains a separate selectable profile. Selecting both profiles explicitly is therefore the way to extrude the full filled area.
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

## Philosophy

The IPC protocol is the contract of the system.

If the protocol stays clean:

- the architecture stays clean
- the UI stays focused on presentation and user intent
- the core stays responsible for CAD behavior
- the codebase stays understandable and maintainable
