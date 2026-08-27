# AI CAD Command Language

This document teaches an AI agent the command language PolySmith exposes over
IPC. It is intentionally agent-oriented: it explains what to remember in
context, what IDs to read from core state, which command types exist, and how to
combine them into CAD modeling workflows.

Runtime source of truth:

- Command union and payload shapes: `apps/desktop-ui/src/types/ipc.ts`
- Command builder helpers: `apps/desktop-ui/src/lib/ipcProtocol.ts`
- Core command dispatch: `native/cad-core/src/app.cpp`
- Core response validation schema: `apps/desktop-ui/src/lib/schemas/ipcSchema.ts`
- Protocol rules: `IPC-Protocol`
- Contextual modeling workflow rule: `Contextual-Modeling-Workflow`

## Mental Model

PolySmith is command-driven CAD. An agent does not mutate CAD state directly.
It sends newline-delimited JSON commands to the native core, then reads
`document_state` and `viewport_state` responses to discover the resulting IDs
and geometry.

The React UI is only a presentation layer. The native core owns:

- document state
- feature history
- sketch state
- geometry solving
- profile detection
- body compilation
- selection state
- face, edge, vertex, profile, sketch entity, and feature IDs
- parameter definitions and formula evaluation

An AI agent should therefore treat PolySmith like a small CAD language:

1. Send one explicit command.
2. Wait for the resulting `document_state`, `viewport_state`, `document_saved`,
   `document_exported`, or `error`.
3. Read IDs and geometry from the returned state.
4. Use those IDs in later commands.
5. Never invent IDs.

## Transport Shape

Commands are JSON objects:

```json
{
  "id": "agent-generated-command-id",
  "type": "command_type",
  "payload": {}
}
```

Rules:

- Every command except `shutdown` requires an `id` string.
- `payload` must be an object. Use `{}` for commands with no parameters.
- Units are millimeters.
- Sketch coordinates are 2D coordinates in the active sketch plane.
- World-space vectors use `{ "x": number, "y": number, "z": number }`.
- A successful mutating command usually returns `document_state`.
- Use `get_viewport_state` after mutations when the next step needs pickable
  faces, edges, vertices, bodies, or sketch profiles.

## AI Assistant Envelope

When PolySmith asks a local model to generate CAD actions, the model must not
return raw protocol messages. It returns a strict JSON envelope without command
IDs:

```json
{
  "message": "short user-facing explanation",
  "commands": [
    {
      "type": "command_type",
      "payload": {}
    }
  ],
  "continue": false
}
```

Rules:

- The model must return JSON only, with no prose before or after the object.
- `message` is user-facing and must not expose internal IDs.
- `commands[]` contains executable PolySmith IPC command types and payloads.
- The app validates every command and rejects malformed batches as a whole.
- The app adds command `id` values immediately before dispatch.
- If a later command needs an ID created by this batch, the model returns only
  the commands that can run now and sets `continue: true`. The app executes the
  batch, refreshes CAD state, and asks the model for the next batch.
- The AI panel keeps a technical working-reference list of current document,
  sketch, profile, body, face, edge, line, and circle IDs. These IDs are for
  command generation and preview/debug use, not normal user-facing prose.
- If the model accidentally includes a later `extrude_profile` with an unknown
  profile ID after creating valid non-construction sketch geometry, the app may
  defer that later command, run only the valid prefix, refresh references, and
  continue the agent loop.
- Profile IDs must come from current `document_state` or `viewport_state`.
  After creating new closed sketch geometry, stop with `continue: true` and
  wait for refreshed state before sending `extrude_profile`.
- Construction sketch geometry is ignored by profile detection. For geometry
  the user wants to extrude, sketch commands must use `is_construction: false`.
- Sketch geometry/edit/projection commands require an active sketch. If no
  sketch is active, the batch must start one with `start_sketch_on_plane`,
  `start_sketch_on_face`, or `reenter_sketch` before issuing those commands.
- If the working references say `Active sketch: none` and the user asks for a
  rectangle, circle, line, arc, 2D profile, or sketch-based extrusion, the first
  modeling command should be `start_sketch_on_plane` with
  `reference_id: "ref-plane-xy"` unless the user specified another plane or
  face. The app may also insert this default XY sketch start when a model omits
  it before new sketch geometry.
- In live app mode, validated commands are sent through the existing Tauri
  bridge, not directly to `cad_core` stdin.

## Core Response Types

### `hello`

Emitted when the core starts.

Payload:

```json
{
  "service": "cad_core",
  "version": "0.1.0"
}
```

### `pong`

Response to `ping`.

Payload:

```json
{
  "version": "0.1.0"
}
```

### `document_created`

Response to `create_document`. Payload is `DocumentState`.

### `document_state`

Response to most document, selection, sketch, and modeling commands. Payload is
`DocumentState`.

Important fields for an agent:

```ts
{
  document_id: string;
  name: string;
  units: string;
  revision: number;
  selected_feature_id: string | null;
  selected_reference_id: string | null;
  selected_face_id: string | null;
  selected_edge_ids: string[];
  selected_vertex_ids: string[];
  active_sketch_plane_id: string | null;
  active_sketch_face_id: string | null;
  active_sketch_feature_id: string | null;
  active_sketch_tool:
    | "select"
    | "line"
    | "rectangle"
    | "circle"
    | "arc"
    | "fillet"
    | "project"
    | "dimension"
    | null;
  selected_sketch_point_id: string | null;
  selected_sketch_entity_id: string | null;
  selected_sketch_point_ids: string[];
  selected_sketch_entity_ids: string[];
  selected_sketch_dimension_id: string | null;
  selected_sketch_profile_id: string | null;
  selected_sketch_profile_ids: string[];
  timeline_cursor: number | null;
  feature_history: FeatureEntry[];
  appearance: {
    body_colors: { body_id: string; color: string }[];
    face_colors: {
      face_id: string;
      owner_body_id: string;
      signature: string;
      color: string;
    }[];
  };
}
```

Use `feature_history` to find stable feature IDs and sketch internals. Feature
kinds currently include:

- `root_part`
- `box`
- `cylinder`
- `sketch`
- `extrude`
- `loft`
- `revolve`
- `sweep`
- `move`
- `fillet`
- `chamfer`
- `construction_plane`

`timeline_cursor` is `null` when the history cursor is at the end. Otherwise it
is the number of non-root timeline actions included in viewport rollback. A
rolled-back cursor does not delete later features; it only affects
`get_viewport_state`.

### `session_state`

Response to `get_session_state`.

```ts
{
  document_count: number;
  has_active_document: boolean;
  active_document_id: string | null;
  can_undo: boolean;
  can_redo: boolean;
}
```

### `viewport_state`

Response to `get_viewport_state`. Use this to discover pickable/renderable
geometry. Important arrays:

- `reference_planes[]`: origin and construction planes
- `reference_axes[]`: origin axes
- `solid_faces[]`: selectable planar/non-planar faces
- `edges[]`: selectable body edges
- `vertices[]`: selectable body vertices
- `sketch_lines[]`, `sketch_circles[]`, `sketch_arcs[]`, `sketch_points[]`
- `sketch_dimensions[]`, `sketch_constraints[]`
- `sketch_profiles[]`: selectable closed regions for extrusion
- `bodies[]`: body IDs plus `center`, `size`, and `local_frame` for boolean target selection and body-local manipulators
- `meshes[]`: triangulated body geometry
- `cut_previews[]`: live cut preview geometry

Body primitives, `solid_faces[]`, and `meshes[]` may include
`appearance_color: "#RRGGBB" | null`. When present, the UI renders that custom
document color instead of the current theme body color. Face colors take
precedence over body colors.

Reference plane IDs:

- `ref-plane-xy`
- `ref-plane-yz`
- `ref-plane-xz`

Body edge IDs have the form `<owner_body_id>:edge:<index>`.
Body vertex IDs have the form `<owner_body_id>:vertex:<index>`.
Face IDs are core-provided strings. Do not construct them unless the core has
already emitted the exact ID in `viewport_state.solid_faces[]`.

### `document_saved`

Response to `save_document`.

```ts
{
  file_path: string;
}
```

### `document_exported`

Response to `export_document`, `export_document_stl`, and `export_body_stl`.

```ts
{
  file_path: string;
  format: "step" | "stl";
  exported_feature_count: number;
}
```

### `log`

Structured diagnostic event. Do not depend on log text for CAD behavior.

```ts
{
  level: "debug" | "info" | "warn" | "error";
  source: string;
  message: string;
  timestamp: string;
}
```

### `error`

Structured command failure.

```ts
{
  code: string;
  message: string;
}
```

On `error`, do not continue as though the command succeeded. Read the message,
refresh state if needed, and choose a valid next command.

## Command Reference

The following commands are implemented by the native core.

### Lifecycle and Inspection

#### `ping`

Checks the core is responsive.

Payload:

```json
{}
```

Returns `pong`.

#### `shutdown`

Requests core shutdown. This command may omit `id`.

Payload:

```json
{}
```

#### `create_document`

Creates a new active document.

Payload:

```json
{}
```

Returns `document_created`.

#### `get_document_state`

Returns the active document state.

Payload:

```json
{}
```

#### `get_session_state`

Returns session metadata and undo/redo availability.

Payload:

```json
{}
```

#### `get_viewport_state`

Returns the core-owned render and pick snapshot.

Payload:

```json
{}
```

### Persistence and Export

#### `save_document`

Saves the active document as a `.polysmith` file.

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_saved`.

#### `load_document`

Loads a `.polysmith` file, replaces the active document, restores ID counters,
and clears undo/redo stacks.

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_state`.

#### `export_document`

Exports solid-producing features as STEP.

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_exported` with `format: "step"`.

#### `export_document_stl`

Exports solid-producing features as binary STL.

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_exported` with `format: "stl"`.

#### `export_body_stl`

Exports one compiled body as binary STL. Use this for user-facing mesh export.

Payload:

```ts
{
  file_path: string;
  body_id: string;
}
```

Returns `document_exported` with `format: "stl"`.

#### `import_stl`

Imports an STL file as a mesh body into the current document. Only the source
path is persisted — the mesh is re-read from disk on every compile; if the file
later disappears the feature degrades with `dependency_broken` + a timeline
warning (no crash).

Payload:

```ts
{
  file_path: string;
  scale?: number; // default 1.0 (STL assumed mm)
}
```

Returns `document_state`.

#### `import_step`

Imports a STEP file (ISO-10303-21) as a non-parametric solid body into the
current document. The file is parsed once at import time (units converted to
mm, the original unit reported in the feature's `parameters_summary`); a B-rep
snapshot is persisted into the saved part file, so the part stays
self-contained — the source .step file is not needed afterwards. Multi-solid
files become ONE body holding a compound. A missing or invalid file throws
before any document mutation (the document stays untouched).

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_state`.

#### `import_iges`

Imports an IGES file as a non-parametric solid body into the current document.
Same semantics as `import_step`: the file is parsed once at import time (units
converted to mm, the original unit from the IGES global section reported in
the feature's `parameters_summary`), a B-rep snapshot is persisted into the
saved part file (self-contained — the source file is not needed afterwards),
and multi-solid files become ONE body holding a compound. A missing or
invalid file throws before any document mutation.

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_state`.

#### `export_document_iges`

Exports all bodies of the current document as an IGES file (BRep mode — every
body becomes an MSBO solid). Units are millimeters.

Payload:

```ts
{
  file_path: string;
}
```

Returns `document_exported` with `format: "iges"`.

#### `convert_mesh_to_body`

Converts a `mesh_import` body into a regular solid body alongside it (sew →
make solid → heal → merge coplanar facets). Requires a watertight mesh;
otherwise a structured error is returned. The converted solid is snapshotted
at creation and is independent afterwards — the source mesh body can be
deleted without losing it. The converted solid supports all regular body
operations.

Payload:

```ts
{
  body_id: string; // the mesh_import feature id
}
```

Returns `document_state`.

#### `detach_body_projections`

Removes every live sketch-projection link sourced from the given body
(face / edge / vertex / body projections). The generated sketch entities
stay in place as fixed lines — only the live link goes away, so the body
can be deleted without breaking the sketches (and extrudes) built on the
projection.

Payload:

```ts
{
  body_id: string;
}
```

Returns `document_state`.

#### `project_body_into_sketch`

Projects a mesh body onto the active sketch plane as fixed-endpoint sketch
lines, recorded as a live `SketchProjection` (`source_kind: "body"`). Works on
origin ref-plane sketches too. Curved STL edges project as polylines — no arc
recovery.

Payload:

```ts
{
  body_id: string; // the mesh_import feature id
  mode: "section" | "silhouette";
  // section = cross-section at the sketch plane; silhouette = outline
  // seen along the sketch plane normal (Fusion "Project" semantics)
}
```

Returns `document_state`.

### Embedded Slicer Handoff

The embedded OrcaSlicer view is not part of the AI CAD command language. When
the UI switches to Slicer view, it only asks Tauri to open/embed the native
OrcaSlicer window. The separate Export to Slicer action is enabled only when the
viewport has at least one body; that action exports through the existing
`export_document_stl` command, waits for `document_exported`, and then hands the
temporary STL path to Tauri-native OrcaSlicer window-management commands. Agents
must not invent separate geometry payloads for the slicer or bypass the native
core export path.

### Primitive Solid Features

Primitive feature commands are direct modeling shortcuts. For richer CAD
objects, prefer sketches plus `extrude_profile`.

#### `add_box_feature`

Creates a box feature.

Payload:

```ts
{
  width: number;
  height: number;
  depth: number;
}
```

Returns `document_state`. Read the new feature ID from
`feature_history[].feature_id` where `kind === "box"`.

#### `update_box_feature`

Updates an existing box.

Payload:

```ts
{
  feature_id: string;
  width: number;
  height: number;
  depth: number;
}
```

#### `add_cylinder_feature`

Creates a cylinder feature.

Payload:

```ts
{
  radius: number;
  height: number;
}
```

#### `update_cylinder_feature`

Updates an existing cylinder.

Payload:

```ts
{
  feature_id: string;
  radius: number;
  height: number;
}
```

### Feature History Commands

#### `rename_feature`

Renames a feature in the timeline.

Payload:

```ts
{
  feature_id: string;
  name: string;
}
```

#### `set_feature_suppressed`

Suppresses or unsuppresses a feature.

Payload:

```ts
{
  feature_id: string;
  suppressed: boolean;
}
```

#### `delete_feature`

Deletes a feature. The core owns dependency handling and warnings.

Payload:

```ts
{
  feature_id: string;
}
```

#### `undo`

Reverts the previous document operation when available.

Payload:

```json
{}
```

#### `redo`

Reapplies an undone operation when available.

Payload:

```json
{}
```

#### `set_timeline_cursor`

Moves the parametric history cursor without mutating feature history. The core
clamps the value into the valid range; setting it to the current action count
places the cursor at the end and serializes back as `timeline_cursor: null`.
Subsequent `get_viewport_state` calls render only the feature-history prefix up
to that cursor.

Payload:

```ts
{
  included_action_count: number;
}
```

### Selection Commands

Selection commands set core-owned selected IDs. They are useful before UI-like
flows, but modeling commands can usually take IDs directly.

#### `clear_selection`

Clears selected feature, reference, face, edge, vertex, sketch entity, sketch
point, sketch dimension, and sketch profile state.

Payload:

```json
{}
```

#### `select_feature`

Payload:

```ts
{
  feature_id: string;
}
```

#### `select_reference`

Selects a reference plane or axis by core-emitted ID.

Payload:

```ts
{
  reference_id: string;
}
```

#### `select_face`

Selects a body face by ID from `viewport_state.solid_faces[]`.

Payload:

```ts
{
  face_id: string;
}
```

#### `select_edge`

Selects or toggles a body edge by ID from `viewport_state.edges[]`.

Payload:

```ts
{
  edge_id: string;
  additive: boolean;
}
```

If `additive` is false, the edge replaces the previous edge selection. If true,
the edge toggles into the multi-edge selection set.

#### `select_vertex`

Selects or toggles a body vertex by ID from `viewport_state.vertices[]`.

Payload:

```ts
{
  vertex_id: string;
  additive: boolean;
}
```

#### `set_body_color`

Applies an opaque document color to a body from `viewport_state.bodies[]`.

Payload:

```ts
{
  body_id: string;
  color: "#RRGGBB";
}
```

#### `set_face_color`

Applies an opaque document color to a face from `viewport_state.solid_faces[]`.
The core stores the face id plus a geometry signature and only reuses the color
while the face still resolves to the same topology.

Payload:

```ts
{
  face_id: string;
  color: "#RRGGBB";
}
```

#### Appearance Clear Commands

- `clear_body_color { body_id }`
- `clear_face_color { face_id }`
- `clear_appearance_overrides {}`

All return `document_state`; send `get_viewport_state` afterwards to render the
updated colors.

### Construction Planes

#### `create_offset_plane`

Creates a parametric construction plane offset from another plane or planar
face.

Payload:

```ts
{
  source_plane_id: string;
  offset: number;
}
```

`source_plane_id` may be:

- `ref-plane-xy`
- `ref-plane-yz`
- `ref-plane-xz`
- an existing construction plane feature ID
- a sketch profile ID
- a planar body face ID from `viewport_state.solid_faces[]`

The offset is signed along the source plane normal. For sketch profiles, the
source frame is the owning sketch plane centered on the profile region.

#### `create_midplane`

Creates a construction plane halfway between two parallel plane-like sources.

Payload:

```ts
{
  source_plane_ids: [string, string];
}
```

Each source id follows the same rules as `create_offset_plane`. The two
resolved frames must be parallel.

#### `create_tangent_plane`

Creates a construction plane tangent to a body face.

Payload:

```ts
{
  source_face_id: string;
}
```

Use a face id from `viewport_state.solid_faces[]`. Curved faces are sampled at
their representative midpoint; planar faces produce a coincident tangent plane.

#### `create_angle_plane`

Creates a construction plane rotated around a linear axis from a plane-like
source.

Payload:

```ts
{
  source_plane_id: string;
  source_axis_id: string;
  angle_degrees: number;
}
```

`source_plane_id` follows the same rules as `create_offset_plane`.
`source_axis_id` may be a sketch line id or a linear body edge id from
`viewport_state.edges[]`. The axis must be parallel to the source plane so the
resulting construction plane can pivot around it.

#### `create_construction_axis`

Creates a construction axis from a linear source.

Payload:

```ts
{
  source_id: string;
}
```

`source_id` may be a sketch line id or a straight body edge id from
`viewport_state.edges[]`. Curved / unsupported edges are rejected by the core.

#### `create_construction_point`

Creates a construction point from a point-like source.

Payload:

```ts
{
  source_id: string;
}
```

`source_id` may be a sketch point id or a body vertex id from
`viewport_state.vertices[]`.

#### `create_hole`

Creates a semantic hole feature on a planar body face.

Payload:

```ts
{
  face_id: string;
  center_x: number;
  center_y: number;
  center_z: number;
  hole_type?: "simple" | "counterbore" | "countersink" | "spotface";
  extent_type?: "blind" | "through_all";
  standard?: "custom" | "metric" | "imperial";
  standard_size?: string;
  hole_fit?: "clearance" | "tap_drill" | "threaded";
  diameter?: number;
  depth?: number;
  counterbore_diameter?: number;
  counterbore_depth?: number;
  countersink_diameter?: number;
  countersink_angle_degrees?: number;
  thread_enabled?: boolean;
  thread_spec?: string;
  thread_pitch?: number;
  major_diameter?: number;
  minor_diameter?: number;
  thread_depth?: number;
  thread_representation?: "cosmetic" | "modeled";
}
```

The core owns the boolean cut and stores the semantic hole parameters in
`feature_history[].hole_parameters`. Cosmetic threaded holes emit a lightweight
thread curve. Modeled threaded holes are experimental/known-buggy and should
not be used when reliable production geometry is required. Face references are
re-resolved during dependency refresh; failed resolution marks the feature
`dependency_broken`.

#### `update_hole_parameters`

Live-edits an existing hole feature.

Payload:

```ts
{
  feature_id: string;
  parameters: Record<string, unknown>;
}
```

Call `confirm_hole { feature_id }` after the contextual panel is accepted.

#### `create_helix`

Creates a construction helix from a linear axis source. The axis source can be a
sketch line id, a construction-axis feature id, or a straight body edge id.

Payload:

```ts
{
  axis_source_id: string;
  radius?: number;
  pitch?: number;
  height?: number;
  handedness?: "left" | "right";
  start_angle_degrees?: number;
}
```

The core samples the helix into `viewport_state.helices[]` and re-resolves the
axis source on recompute. If the source disappears or is no longer linear, the
feature degrades with `dependency_broken` instead of storing a stale topology
index.

#### `create_thread`

Creates a semantic thread feature. Cosmetic representation stores thread
metadata and emits a lightweight cosmetic thread curve. Modeled representation
is experimental/known-buggy and should be treated as a future fix area rather
than reliable production geometry.
The target is a body id and the axis source is a sketch line, construction-axis
feature, or straight body edge.

Payload:

```ts
{
  target_body_id: string;
  axis_source_id: string;
  mode?: "external" | "internal";
  standard?: "custom" | "metric" | "imperial";
  size?: string;
  pitch?: number;
  length?: number;
  representation?: "cosmetic" | "modeled";
}
```

Use `update_thread_parameters { feature_id, parameters }` for live edits and
`confirm_thread { feature_id }` to finish the pending feature. If the target
body or axis source stops resolving during recompute, the feature degrades with
`dependency_broken` instead of trusting stale topology.

#### `create_fastener`

Creates a semantic fastener body. Cosmetic thread representation emits a
lightweight thread curve. Modeled thread representation is experimental/
known-buggy and can currently produce invalid or incomplete threaded shafts in
viewport/export.
Hex-socket and Phillips drive types cut simple recess geometry into the
generated head.

Payload:

```ts
{
  standard?: "metric" | "imperial" | "custom";
  size?: string;
  diameter?: number;
  minor_diameter?: number;
  pitch?: number;
  length?: number;
  thread_length?: number;
  head_type?: "socket_head" | "button_head" | "flat" | "hex_bolt";
  drive_type?: "none" | "hex_socket" | "phillips";
  thread_representation?: "cosmetic" | "modeled";
}
```

#### `create_move`

Creates a semantic 3D Move timeline feature for one body. The target must be a
body id from `viewport_state.bodies[]`. Translation components are millimeters
in the body's local frame. Rotation components are degrees around the current
pre-move body bounding-box center, applied in fixed X -> Y -> Z order.

Payload:

```ts
{
  target_body_id: string;
  translation_x?: number;
  translation_y?: number;
  translation_z?: number;
  rotation_x_degrees?: number;
  rotation_y_degrees?: number;
  rotation_z_degrees?: number;
}
```

Use `update_move_parameters { feature_id, parameters }` for live preview and
`confirm_move { feature_id }` after the contextual panel is accepted. If the
target body disappears during recompute, the Move feature degrades with
`dependency_broken` instead of trusting stale topology.

#### `create_body_copy`

Creates a core-owned `body_copy` timeline feature from an existing body id. In
`linked` mode the copy replays from the source body on recompute. In
`standalone` mode the core stores a frozen shape snapshot so later source edits
do not change the copy. The new body is emitted under the copy feature id,
starts in the same position and local frame as the source, and can be followed
by `create_move` to place it.

Payload:

```ts
{
  source_body_id: string;
  copy_mode?: "linked" | "standalone";
}
```

If the source body disappears during recompute, linked copies degrade with
`dependency_broken` instead of keeping a stale shape reference. Standalone
copies remain valid from their stored snapshot.

#### `unlink_body_copy`

Converts a linked body copy into an independent copy by storing a frozen core
shape snapshot and changing the copy feature's `copy_mode` to `"standalone"`.
Use only on `body_copy` features whose `body_copy_parameters.copy_mode` is
`"linked"`.

Payload:

```ts
{
  feature_id: string;
}
```

#### `update_offset_plane`

Updates a construction plane offset.

Payload:

```ts
{
  feature_id: string;
  offset: number;
}
```

#### `update_angle_plane`

Updates the angle of an existing angle construction plane.

Payload:

```ts
{
  feature_id: string;
  angle_degrees: number;
}
```

### Starting and Reentering Sketches

Sketch commands require an active sketch unless explicitly stated otherwise.
An AI agent must start a sketch on a plane or face before sending sketch
geometry, sketch edit, projection, mirror-preview, or `finish_sketch` commands.
Start a sketch before adding sketch geometry.

#### `start_sketch_on_plane`

Starts a sketch on an origin or construction plane.

Payload:

```ts
{
  reference_id: string;
}
```

Use `ref-plane-xy`, `ref-plane-yz`, `ref-plane-xz`, or a construction plane
feature ID.

#### `start_sketch_on_face`

Starts a sketch on a planar body face. The `plane_frame` must be copied from the
matching face in `viewport_state.solid_faces[]`.

Payload:

```ts
{
  face_id: string;
  plane_frame: {
    origin: { x: number; y: number; z: number };
    x_axis: { x: number; y: number; z: number };
    y_axis: { x: number; y: number; z: number };
    normal: { x: number; y: number; z: number };
  };
}
```

#### `finish_sketch`

Finishes the active sketch.

Payload:

```json
{}
```

#### `reenter_sketch`

Reactivates an existing sketch feature by feature ID without creating a new
sketch.

Payload:

```ts
{
  feature_id: string;
}
```

### Sketch Tools

#### `set_sketch_tool`

Sets the core-owned active sketch tool.

Payload:

```ts
{
  tool:
    | "select"
    | "line"
    | "rectangle"
    | "circle"
    | "polygon"
    | "arc"
    | "fillet"
    | "chamfer"
    | "trim"
    | "extend"
    | "offset"
    | "ellipse"
    | "slot"
    | "spline"
    | "text"
    | "move"
    | "project"
    | "dimension";
}
```

An AI agent that sends direct add/update commands does not always need to set
the tool, but setting it keeps UI state consistent for interactive workflows.

### Sketch Geometry Creation

All sketch geometry is created in the active sketch plane using local 2D
coordinates.

#### `add_sketch_line`

Adds a line segment.

Payload:

```ts
{
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  is_construction: boolean;
}
```

Construction lines render dashed, can be referenced by constraints/snaps, and
do not form profiles.

#### `add_sketch_rectangle`

Adds four sketch lines from diagonal corners.

Payload:

```ts
{
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  is_construction: boolean;
}
```

Non-construction rectangles normally produce a closed `sketch_profile`.
Construction rectangles are reference geometry only and do not produce
extrudable profiles.

#### `add_sketch_circle`

Adds a circle. `mode` selects the creation interpretation:

- `center_radius` (default / empty): the given center + radius.
- `two_point`: `p1` / `p2` are diameter endpoints.
- `three_point`: `p1` / `p2` / `p3` lie on the circle (circumcircle).
- `tangent_two_lines`: `line_a_id` / `line_b_id` + the hint point `hint_x` /
  `hint_y` — the circle lands in the wedge containing the hint, tangent to
  both lines; a `tangent_circle_line` relation keeps it tangent under
  solver runs (radius re-derives from the fixed center).
- `tangent_three_lines`: the incircle of the three lines.

Payload:

```ts
{
  center_x: number;
  center_y: number;
  radius: number;
  is_construction: boolean;
  mode?: string;          // see above
  p1_x?: number; p1_y?: number;
  p2_x?: number; p2_y?: number;
  p3_x?: number; p3_y?: number;
  line_a_id?: string; line_b_id?: string; line_c_id?: string;
  hint_x?: number; hint_y?: number;
}
```

Non-construction circles produce circular profiles.

#### `add_sketch_polygon`

Adds a regular N-sided polygon.

Payload:

```ts
{
  sides: number;        // >= 3
  mode: string;         // "inscribed" | "circumscribed" | "edge"
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  is_construction: boolean;
}
```

Modes:

- `inscribed`: center + vertex. `(start_x, start_y)` is the center; `(end_x, end_y)`
  is a vertex on the polygon's circumcircle. The core computes vertices from the
  center and radius (= distance to vertex).
- `circumscribed`: center + apothem. `(start_x, start_y)` is the center;
  `(end_x, end_y)` is the midpoint of one edge. The core pushes vertices
  outward by `radius / cos(π/N)` so the polygon circumscribes the reference
  circle.
- `edge`: two endpoints of one edge. The core derives center and radius from
  the edge length and side count. The polygon is oriented clockwise from
  `(start_x, start_y)`.

The core rejects `sides < 3` as a structured error. Non-construction polygons
receive an automatic radius dimension.

#### `add_sketch_arc`

Adds an arc.

Payload:

```ts
{
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  anchor_x: number;
  anchor_y: number;
  mode: "three_point" | "center_start_end";
  is_construction: boolean;
}
```

Modes:

- `three_point`: start, end, and anchor all lie on the arc. The core computes
  the circumcenter.
- `center_start_end`: anchor is the center. Radius comes from center to start,
  and the end point is snapped onto that circle.

The core rejects colinear and zero-radius arcs.

#### `add_sketch_text`

Places parametric text on the active sketch.

Payload:

```ts
{
  text: string;        // UTF-8; '\n' starts a new line. Default "Text".
  font_path: string;   // "" = default font; absolute .ttf path = user font.
  height_mm: number;   // Default 10.
  angle_deg: number;   // Rotation around the anchor. Default 0.
  anchor_x: number;    // Sketch-local anchor point.
  anchor_y: number;
  h_align: "left" | "center" | "right";       // Default "center".
  v_align: "top" | "middle" | "bottom";       // Default "middle".
  char_spacing: number; // Fraction added to each advance. Default 0.
}
```

Text over 500 characters is rejected. The glyph geometry expands into
ordinary sketch lines on every recompute, so text profiles extrude,
render, and export exactly like drawn geometry. Glyph lines are not
individually editable — edit the text entity instead.

#### `update_sketch_text`

Merges a partial patch over the stored record and re-expands the glyphs.

```ts
{ text_id: string; text?: string; font_path?: string; height_mm?: number;
  angle_deg?: number; anchor_x?: number; anchor_y?: number;
  h_align?: string; v_align?: string; char_spacing?: number }
```

Height/angle/anchor/alignment/spacing edits keep the generated id set
and re-snapshot linked extrudes; string/font edits degrade linked
extrudes to `dependency_broken` with a warning. Requires the sketch to
be active (re-enter it after extruding).

#### `add_sketch_ellipse`

Adds a full ellipse from three clicks: center, a major-axis point, and a
minor-axis point (the minor radius is the perpendicular distance of the
third click from the major axis). The axis points are FIXED at creation —
no solver registration yet, so the cached shape cannot drift. The ellipse
participates in the exact profile engine as a full closed curve
(`entity_kind "ellipse"`); the wire builder emits an analytic OCCT ellipse
edge. Construction ellipses are excluded from profiles. Trim on ellipses
is rejected (v1).

Payload:

```ts
{
  center_x: number; center_y: number;
  axis_a_x: number; axis_a_y: number;
  axis_b_x: number; axis_b_y: number;
  is_construction?: boolean;
}
```

#### `add_sketch_slot` / `update_sketch_slot`

Adds / updates a straight slot (stadium). The core expands every slot into
2 lines + 2 arcs (tangent by construction, `generated_by = "slot:<id>"`)
at the top of every recompute — profiles / extrude / viewport consume the
plain entities with zero downstream changes. `length` is the distance
between the two arc centers and must stay >= 2 * radius. The slot center
is a regular movable vertex (distance dimensions work).

Payload (both):

```ts
{
  center_x: number; center_y: number;
  length: number;
  radius: number;
  rotation: number;   // radians
  // update only:
  slot_id: string;
}
```

#### `add_sketch_chamfer` / `update_sketch_chamfer` / `delete_sketch_chamfer`

Adds / edits / removes a parametric corner chamfer (line-line), the bevel
analogue of the fillet. The chamfer line and trim points are generated
geometry re-derived from the corner + the two distances on every recompute.
A corner already holding a fillet is rejected (both ways).

Payloads:

```ts
// add
{ corner_vertex_id: string; line_a_id: string; line_b_id: string;
  distance_a: number; distance_b: number; }
// update
{ chamfer_id: string; distance_a: number; distance_b: number; }
// delete
{ chamfer_id: string; }
```

#### `add_sketch_spline`

Adds a control-point B-spline: `points` ARE the poles — regular movable
vertices. The drawn curve is the clamped open-uniform B-spline they define
(degree = min(3, count - 1); `spline_math.h` is the single evaluation
source for the profile walk, the viewport, the draft preview and the wire
builder). No solver registration — pole drags re-fit the curve through the
ordinary vertex sync. At least 2 points are required; 3+ give curvature.
The profile engine treats the spline as an open exact curve
(`entity_kind "spline"`); intersections delegate to OCCT 2D algorithms.
A control polygon whose end pole coincides with its start pole CLOSES the
loop and bounds a region by itself (like a full circle). The extrude wire
builder emits the exact `Geom_BSplineCurve` edge trimmed to the walked
sub-span. Trim / extend / offset on splines are rejected (v1).

Payload:

```ts
{
  points: Array<{ x: number; y: number }>;
  is_construction?: boolean;
}
```

#### `delete_sketch_text`

```ts
{ text_id: string }
```

Removes the text record and its generated glyph geometry. Deleting
selected glyph lines via `delete_sketch_selection` also deletes their
owning text.

#### `add_sketch_fillet`

Rounds a sketch corner shared by two sketch lines into a tangent arc.

Payload:

```ts
{
  corner_point_id: string;
  line_a_id: string;
  line_b_id: string;
  radius: number;
}
```

The corner point must be an endpoint of both lines. Read `corner_point_id`,
`line_a_id`, and `line_b_id` from the active sketch's `points[]` and `lines[]`.

### Sketch Geometry Updates

#### `update_sketch_line`

Replaces a line's endpoints.

Payload:

```ts
{
  line_id: string;
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
}
```

#### `update_sketch_point`

Moves a sketch point. The core updates owned geometry and constraints.

Payload:

```ts
{
  point_id: string;
  x: number;
  y: number;
}
```

#### `move_sketch_entities`

Rigidly moves sketch entities — a translation plus an optional rotation
around a sketch-local center — in a single undo step. Used by the sketch
Move tool commit.

Payload:

```ts
{
  entity_ids: string[];
  dx: number;
  dy: number;
  center_x: number;
  center_y: number;
  angle_deg: number;
}
```

Semantics:

- `entity_ids` are line / circle / arc ids, or vertex ids for standalone
  points. Every vertex owned by those entities is transformed by
  `R(angle)·(p − center) + center + (dx, dy)`; circle/arc radii are
  preserved (pure rigid motion, no scaling).
- Fixed vertices never move; connected geometry ripples exactly like an
  endpoint drag (`propagate_connected_point_move`), then the native
  planegcs solver re-solves constraints.
- When `|angle_deg| > 1e-6` the core frees H/V constraints on the moved
  lines (a rotated line is genuinely no longer axis-aligned; keeping the
  constraint would snap it back). Pure translations keep H/V constraints.
- Exactly ONE undo state is pushed for the whole move; the selection is
  preserved so the move can be repeated on the same entities.
- Projected entities are fixed (derived from 3D body geometry) and are
  therefore skipped.

#### `transform_sketch_entities`

Translate / rotate / uniform-scale sketch entities (or exploded copies of
them) in a single undo step. `move_sketch_entities` is a rigid wrapper
over this command. `copy: true` leaves the originals and pushes fresh
copies (new ids, copies share vertices with each other but never with the
originals; H/V constraints inferred only when not rotating).

Payload:

```ts
{
  entity_ids: string[];
  dx: number; dy: number;
  center_x: number; center_y: number;
  angle_deg: number;
  scale: number;
  copy: boolean;
}
```

#### `create_linear_array` / `create_circular_array`

Exploded array copies (direct-commit, one undo step per array — undo is
the adjust path for v1). Linear: `count` copies at `(dx, dy)` spacing;
circular: `count` copies around `(center_x, center_y)` across
`total_angle_deg`.

Payloads:

```ts
// linear
{ entity_ids: string[]; dx: number; dy: number; count: number; }
// circular
{ entity_ids: string[]; center_x: number; center_y: number;
  count: number; total_angle_deg: number; }
```

#### `extend_sketch_entity`

Extends a line (infinite support) or arc (full circle) from the end nearest
the click to the nearest intersection with another non-construction entity.
H/V constraints survive; arc angle dimensions flip to driven.

Payload:

```ts
{ entity_id: string; click_x: number; click_y: number; }
```

#### `trim_sketch_entity`

Splits the clicked line / circle / arc / ellipse / spline at every
intersection with other non-construction entities and deletes the
segment under the click (circles become the complementary arc; a full
ellipse becomes a partial elliptical arc with a stored sweep;
middle-segment trims split arcs, elliptical arcs and splines into two —
spline pieces are re-fit exactly via OCCT knot-insertion, so the cut
ends land on the intersection). The optional `segment_index` — the
hovered index from the matching `trim_preview_result` — overrides
click-based selection so the trim deletes exactly the highlighted
segment. `expected_revision` is the document revision the preview was
computed against: when it does not match the current document, the core
IGNORES `segment_index` and re-derives the segment from the click point
(a stale preview must never cut the wrong piece).

Payload:

```ts
{
  entity_id: string;
  click_x: number;
  click_y: number;
  segment_index?: number;     // from trim_preview_result.hovered_index
  expected_revision?: number; // document.revision of that preview
  preview_id?: string;        // command id of that preview (diagnostics)
}
```

#### `offset_sketch_entity`

Offsets a single line / circle / arc by a signed distance (left-normal
convention for lines; radius + d for circles and arcs, sweep preserved).
Non-parametric: the offset is a fresh entity (no auto dimensions).
Construction / generated / ellipse / spline entities are rejected.

Payload:

```ts
{ entity_id: string; distance: number; }
```

#### `update_sketch_circle`

Updates a circle.

Payload:

```ts
{
  circle_id: string;
  center_x: number;
  center_y: number;
  radius: number;
}
```

#### `set_sketch_line_construction`

Toggles whether a sketch line is construction geometry.

Payload:

```ts
{
  line_id: string;
  is_construction: boolean;
}
```

#### `update_sketch_fillet_radius`

Updates a parametric sketch fillet.

Payload:

```ts
{
  fillet_id: string;
  radius: number;
}
```

#### `delete_sketch_fillet`

Removes a sketch fillet and restores the original corner.

Payload:

```ts
{
  fillet_id: string;
}
```

#### `delete_sketch_selection`

Deletes selected sketch geometry by explicit IDs.

Payload:

```ts
{
  entity_ids: string[];
  point_ids: string[];
  profile_ids: string[];
}
```

`entity_ids` may reference sketch lines, circles, or arcs. `point_ids` resolve
to owned geometry. `profile_ids` resolve to profile boundary geometry.

### Sketch Selection Commands

#### `select_sketch_entity`

Selects or toggles a sketch edge entity.

Payload:

```ts
{
  entity_id: string;
  additive: boolean;
}
```

Entity IDs may be line IDs, circle IDs, or arc IDs.

#### `select_sketch_point`

Selects or toggles a sketch point.

Payload:

```ts
{
  point_id: string;
  additive: boolean;
}
```

#### `select_sketch_dimension`

Selects a sketch dimension.

Payload:

```ts
{
  dimension_id: string;
}
```

#### `select_sketch_profile`

Selects or toggles a closed sketch profile.

Payload:

```ts
{
  profile_id: string;
  additive?: boolean;
}
```

Profiles can be selected from any sketch in the document. The core resolves the
owning sketch.

### Sketch Constraints and Anchors

#### `set_sketch_line_constraint`

Sets or clears a horizontal/vertical relation.

Payload:

```ts
{
  line_id: string;
  constraint: "none" | "horizontal" | "vertical";
}
```

#### `set_sketch_equal_length_constraint`

Sets or clears an equal-length relation between two lines.

Payload:

```ts
{
  line_id: string;
  other_line_id: string;
}
```

Use `"none"` for `other_line_id` to clear the relation.

#### `set_sketch_perpendicular_constraint`

Sets or clears a perpendicular relation between two lines.

Payload:

```ts
{
  line_id: string;
  other_line_id: string;
}
```

Use `"none"` for `other_line_id` to clear the relation.

#### `set_sketch_parallel_constraint`

Sets or clears a parallel relation between two lines.

Payload:

```ts
{
  line_id: string;
  other_line_id: string;
}
```

Use `"none"` for `other_line_id` to clear the relation.

#### `set_sketch_tangent_constraint`

Sets or clears a line-circle tangent relation.

Payload:

```ts
{
  line_id: string;
  circle_id: string;
}
```

Use an empty string for `circle_id` to clear.

#### `set_sketch_coincident_constraint`

Makes two sketch points coincident.

Payload:

```ts
{
  point_id: string;
  other_point_id: string;
}
```

#### `set_sketch_point_fixed`

Fixes or unfixes a sketch point.

Payload:

```ts
{
  point_id: string;
  is_fixed: boolean;
}
```

#### `set_sketch_midpoint_anchor`

Constrains a point to the midpoint of a host line.

Payload:

```ts
{
  point_id: string;
  host_line_id: string;
}
```

Use an empty string for `host_line_id` to clear.

#### `set_sketch_point_line_anchor`

Constrains a point to a parametric position along a host line.

Payload:

```ts
{
  point_id: string;
  host_line_id: string;
  t: number;
}
```

`t` is clamped by the core. `0` is the host start, `1` is the host end, and
`0.5` is the midpoint. Use an empty string for `host_line_id` to clear.

### Sketch Dimensions

#### `add_sketch_angle_dimension`

Adds or reselects an angle dimension between two lines sharing an endpoint.

Payload:

```ts
{
  first_line_id: string;
  second_line_id: string;
}
```

#### `add_sketch_distance_dimension`

Adds or reselects a distance dimension.

Payload:

```ts
{
  first_entity_id: string;
  second_entity_id: string;
}
```

Supported combinations:

- parallel line to parallel line
- circle center to circle center
- circle center to line

#### `add_sketch_point_distance_dimension`

Adds or reselects a straight-line distance dimension between two sketch points.

Payload:

```ts
{
  point_a_id: string;
  point_b_id: string;
}
```

#### `add_sketch_line_length_dimension`

Creates a length dimension on a single sketch line. Used when the Dimension tool
clicks a line whose auto-dimension was deleted. The core validates the entity
exists, checks for a duplicate `dim-line-{id}`, then creates a `SketchDimension`
of kind `line_length` with the current geometric length.

Payload:

```ts
{
  line_id: string;
}
```

#### `add_sketch_circle_radius_dimension`

Creates a radius (or diameter) dimension on a single sketch circle. Same
pattern as line length: validates, deduplicates, creates a `SketchDimension`
of kind `circle_radius`. An optional `display_as` field controls rendering.

Diameter convention: the STORED value is always the radius (the solver
contract); the payload's `value` carries the DISPLAYED diameter (x2) when
`display_as` is absent or `"diameter"`, and dimension edits divide the
entered diameter by two at the IPC boundary — both the expression path
and the numeric path follow this.

Payload:

```ts
{
  circle_id: string;
  display_as?: "radius" | "diameter";
}
```

#### `add_sketch_polygon_radius_dimension`

Creates a radius dimension on a single sketch polygon. Same pattern as the
others: validates, deduplicates, creates a `SketchDimension` of kind
`polygon_radius`.

Payload:

```ts
{
  polygon_id: string;
}
```

#### `add_sketch_arc_radius_dimension`

Creates a radius dimension on a sketch arc (kind `arc_radius`). Driving
for user arcs via the deterministic `enforce_arc_dimensions` pass.

Payload:

```ts
{ arc_id: string; }
```

#### `add_sketch_arc_angle_dimension`

Creates an angle dimension on a sketch arc (kind `arc_angle`). The value
is the sweep in radians; edits re-derive the end point along the circle
with the stored ccw sense.

Payload:

```ts
{ arc_id: string; }
```

#### `add_sketch_arc_length_dimension`

Creates a length dimension on a sketch arc (kind `arc_length`). The sweep
re-derives from `length / radius` on every drive; driven dimensions
re-measure from the geometry. The viewport label reads `L <value> mm`.

Payload:

```ts
{ arc_id: string; }
```

#### `update_sketch_dimension`

Solves a dimension to a new value. `value` can be a plain `number` (backward
compatible) or a `string` expression referencing named parameters (e.g.
`"width * 2"`, `"thickness / 3"`). When a string is supplied, the core
evaluates it against the current parameter table and stores the expression
so it re-evaluates on parameter changes.

Payload:

```ts
{
  dimension_id: string;
  value: number | string;
}
```

Dimension kinds emitted by state:

- `line_length`
- `circle_radius` (stores the radius; `display_as` selects R or ⌀ display)
- `arc_radius`
- `arc_length` (drives the sweep as L / radius)
- `arc_angle` (arc sweep, radians internally)
- `polygon_radius`
- `angle` (between two lines, radians internally)
- `line_angle` (from positive X axis, radians internally)
- `line_line_distance`
- `circle_center_distance`
- `circle_line_distance`
- `point_distance` (distance between two sketch points)

Angle dimensions (`angle`, `line_angle`) store radians in `value` but
expressions are authored in degrees. The core converts degrees→radians
during expression evaluation. `line_angle` preserves the sign quadrant
from the current geometry when re-evaluated.

#### `delete_sketch_dimension`

Deletes a sketch dimension by ID.

Payload:

```ts
{
  dimension_id: string;
}
```

#### `update_sketch_dimension_label_position`

Stores presentation-only label placement for a sketch dimension in sketch-local
coordinates. This does not change the solved dimension value.

Payload:

```ts
{
  dimension_id: string;
  label_x: number;
  label_y: number;
}
```

#### `update_sketch_dimension_display`

Toggles a circle dimension between radius and diameter display.

Payload:

```ts
{
  dimension_id: string;
  display_as: "radius" | "diameter";
}
```

### Parametric Parameters

Document-scoped named parameters that can be referenced in dimension expressions.
Parameters support simple arithmetic (`+`, `-`, `*`, `/`, parens) and can
reference other parameters by name (e.g. `height = width * 2`). Cycle detection
is built-in.

#### `add_parameter`

Adds a new named parameter.

Payload:

```ts
{
  name: string;       // e.g. "width" — must be unique, non-empty
  expression: string; // e.g. "50", "width * 2", "(a + b) / 3"
  kind: "length" | "angle"; // default "length". "angle" stores degrees.
}
```

#### `update_parameter`

Replaces the expression of an existing parameter. Re-evaluates all parameters
and re-resolves dimension expressions across all sketch features.

Payload:

```ts
{
  name: string;
  expression: string;
  kind: "length" | "angle"; // default "length"
}
```

#### `delete_parameter`

Removes a parameter. Other parameters that reference it will have
`has_error = true` until their expressions are updated. Dimensions that
reference it keep their last resolved value silently.

Payload:

```ts
{
  name: string;
}
```

Parameters appear in `document_state.parameters[]`:

```ts
{
  name: string;
  expression: string;
  resolved_value: number;    // mm for length, degrees for angle
  kind: "length" | "angle"; // "length" = mm, "angle" = degrees
  has_error: boolean;
  error_message: string;
}
```

**Kind checking:** Angle-type parameters cannot be used in length
dimensions — the core resolver checks `p.kind` against the target dimension
kind and throws a descriptive error when mismatched.

### Mirror Preview Lifecycle

Mirror follows the contextual modeling lifecycle: start preview, update inputs, commit
or cancel.

#### `start_mirror_preview`

Starts an empty pending mirror preview in the active sketch.

Payload:

```json
{}
```

#### `update_mirror_preview_axis`

Sets the mirror axis.

Payload:

```ts
{
  axis_line_id: string;
}
```

Use an empty string to clear the axis.

#### `update_mirror_preview_objects`

Sets the sketch objects to mirror.

Payload:

```ts
{
  object_ids: string[];
}
```

Object IDs are sketch line IDs and circle IDs supported by the current mirror
tool.

#### `commit_mirror_preview`

Commits preview geometry into real sketch geometry.

Payload:

```json
{}
```

#### `cancel_mirror_preview`

Cancels the pending mirror preview.

Payload:

```json
{}
```

### Profile Extrusion

#### `extrude_profile`

Creates an extrude feature from one or more closed sketch profiles. This command
does not require an active sketch; the core resolves each profile's owning
sketch.

Payload:

```ts
type ExtrudeSideParameters = {
  extent_type: "distance" | "through_all" | "to_object" | "to_next";
  distance: number;
  start_offset: number;
  taper_angle_degrees: number;
  target_reference_id?: string;
};

{
  profile_id?: string;
  profile_ids?: string[];
  open_entity_ids?: string[];
  depth: number;
  mode?: "new_body" | "join" | "cut" | "intersect";
  target_body_id?: string;
  parameters?: {
    operation?: "new_body" | "join" | "cut" | "intersect";
    extent_mode?: "one_side" | "symmetric" | "two_sides";
    side1?: ExtrudeSideParameters;
    side2?: ExtrudeSideParameters;
    thin?: { enabled: boolean; thickness: number; placement: "center" | "inside" | "outside" };
    intersect_result?: "replace_target" | "new_body";
  };
}
```

Rules:

- Prefer `profile_ids` even for one profile.
- `profile_id` is kept for legacy single-profile callers.
- Multiple profiles must belong to the same sketch plane.
- With `mode: "new_body"`, each selected profile creates its own body.
- With untargeted `mode: "join"`, touching profiles are grouped into one body
  while separated profile groups become separate bodies. The feature keeps
  `operation: "join"` for editing, but compiles as `mode: "new_body"` because
  there is no existing target body.
- With `mode: "cut"`, `mode: "intersect"`, or a targeted `join`, the selected
  profiles stay in one feature so the boolean target remains explicit.
- If `mode` is omitted, the core chooses automatically: Join when the
  extrusion touches an existing body or selected profiles touch each other,
  Cut when it overlaps an existing body, otherwise New Body.
- `open_entity_ids` requires `parameters.thin.enabled = true` and currently
  accepts connected sketch line / arc chains.
- For `join` and `cut`, `target_body_id` is optional. If omitted, the core
  falls back to the most recent body when possible.
- Use `viewport_state.bodies[]` to discover explicit target body IDs.

#### `extrude_face`

Creates an extrude feature directly from a planar solid face. This command does
not require an active sketch.

Payload:

```ts
{
  face_id: string;
  depth: number;
  mode?: "new_body" | "join" | "cut" | "intersect";
  target_body_id?: string;
  parameters?: { ...same extrude parameter fields as `extrude_profile` };
}
```

Rules:

- Use a planar face ID from `viewport_state.solid_faces[]`.
- Annular faces carry their inner loop into the extrude profile.
- `mode` and `target_body_id` follow the same rules as `extrude_profile`.

#### `update_extrude_depth`

Live-edits an extrude depth.

Payload:

```ts
{
  feature_id: string;
  depth: number;
}
```

#### `update_extrude_mode`

Changes an existing extrude's boolean composition mode.

Payload:

```ts
{
  feature_id: string;
  mode: "new_body" | "join" | "cut";
}
```

#### `update_extrude_target_body`

Changes or clears an extrude's explicit boolean target.

Payload:

```ts
{
  feature_id: string;
  target_body_id?: string;
}
```

Omit `target_body_id` to clear the explicit target.

#### `update_extrude_parameters`

Live-edits the complete extrude parameter object. Use this for feature-complete
extrude editing instead of sending several shorthand commands.

Payload:

```ts
{
  feature_id: string;
  parameters: ExtrudeFeatureParameters;
}
```

#### `update_extrude_profiles`

Replaces the source profile set for an extrude.

Payload:

```ts
{
  feature_id: string;
  profile_ids: string[];
}
```

### Profile Loft

#### `loft_profiles`

Creates a new body by lofting through two or more sketch profiles in the
provided order. This command does not require an active sketch.

Payload:

```ts
{
  profile_ids: string[];
  ruled?: boolean;
}
```

Rules:

- Use two or more IDs from `feature_history[].sketch_parameters.profiles[]` or
  `viewport_state.sketch_profiles[]`.
- Profile order controls section order.
- `ruled` defaults to `false` for a smooth transition; `true` creates straight
  section-to-section transitions.
- v1 supports closed profiles without holes. Profiles with `inner_loops[]` are
  rejected so the core does not create ambiguous solids.
- Loft always creates a new body; boolean join/cut is not supported yet.

#### `update_loft_profiles`

Replaces the source profile list for an existing loft while preserving its
smooth/ruled setting.

Payload:

```ts
{
  feature_id: string;
  profile_ids: string[];
}
```

#### `update_loft_ruled`

Changes an existing loft between smooth and ruled transitions.

Payload:

```ts
{
  feature_id: string;
  ruled: boolean;
}
```

### Profile Revolve

#### `revolve_profile`

Creates a new body by revolving one sketch profile around a sketch line axis.
This command does not require an active sketch.

Payload:

```ts
{
  profile_id: string;
  axis_entity_id: string;
  angle_degrees?: number;
}
```

Rules:

- Use a profile ID from `feature_history[].sketch_parameters.profiles[]` or
  `viewport_state.sketch_profiles[]`.
- Use a sketch line ID for `axis_entity_id`; the axis may come from a different
  sketch than the profile.
- `angle_degrees` defaults to `360` and must be greater than `0` and no more
  than `360`.
- Revolve always creates a new body; boolean join/cut is not supported yet.

#### `update_revolve_profile`

Replaces the source profile for an existing revolve while preserving its axis
and angle.

Payload:

```ts
{
  feature_id: string;
  profile_id: string;
}
```

#### `update_revolve_axis`

Replaces the axis line for an existing revolve while preserving its profile and
angle.

Payload:

```ts
{
  feature_id: string;
  axis_entity_id: string;
}
```

#### `update_revolve_angle`

Changes an existing revolve angle in degrees for live preview or timeline edit.

Payload:

```ts
{
  feature_id: string;
  angle_degrees: number;
}
```

### Profile Sweep

#### `sweep_profile`

Creates a new body by sweeping one closed sketch profile along a sketch path.
This command does not require an active sketch.

Payload:

```ts
{
  profile_id: string;
  path_entity_id: string;
}
```

Rules:

- Use a profile ID from `feature_history[].sketch_parameters.profiles[]` or
  `viewport_state.sketch_profiles[]`.
- Use a sketch line or arc ID for `path_entity_id`; the path may come from a
  different sketch than the profile.
- The core resolves the connected non-construction line/arc chain containing
  `path_entity_id`. Branched or disconnected paths are rejected.
- Sweep always creates a new body.

#### `update_sweep_profile`

Replaces the source profile for an existing sweep while preserving its path.

Payload:

```ts
{
  feature_id: string;
  profile_id: string;
}
```

#### `update_sweep_path`

Replaces the path seed entity for an existing sweep while preserving its
profile. The same connected-chain resolution rules apply.

Payload:

```ts
{
  feature_id: string;
  path_entity_id: string;
}
```

### Body Fillets and Chamfers

These commands operate on body edges from `viewport_state.edges[]`. Edge IDs
must belong to the same owner body for a multi-edge operation.

#### `create_fillet`

Creates a body edge fillet preview/feature.

Payload:

```ts
{
  edge_ids: string[];
  radius: number;
}
```

The core also accepts legacy `{ edge_id: string, radius: number }`, but agents
should use `edge_ids`.

#### `update_fillet_edges`

Changes the selected edges for a fillet.

Payload:

```ts
{
  feature_id: string;
  edge_ids: string[];
}
```

#### `update_fillet_radius`

Changes a fillet radius.

Payload:

```ts
{
  feature_id: string;
  radius: number;
}
```

#### `confirm_fillet`

Confirms a fillet feature.

Payload:

```ts
{
  feature_id: string;
}
```

#### `create_chamfer`

Creates a body edge chamfer preview/feature.

Payload:

```ts
{
  edge_ids: string[];
  distance: number;
}
```

The core also accepts legacy `{ edge_id: string, distance: number }`, but agents
should use `edge_ids`.

#### `update_chamfer_edges`

Changes the selected edges for a chamfer.

Payload:

```ts
{
  feature_id: string;
  edge_ids: string[];
}
```

#### `update_chamfer_distance`

Changes a chamfer distance.

Payload:

```ts
{
  feature_id: string;
  distance: number;
}
```

#### `confirm_chamfer`

Confirms a chamfer feature.

Payload:

```ts
{
  feature_id: string;
}
```

#### `create_shell`

Creates a shell preview/feature from a selected body face. The selected face is
removed as the shell opening.

Payload:

```ts
{
  face_id: string;
  thickness: number;
}
```

`face_id` must come from `viewport_state.solid_faces[]`. `thickness` must be
positive and is applied inward.

#### `update_shell_thickness`

Changes a shell thickness.

Payload:

```ts
{
  feature_id: string;
  thickness: number;
}
```

#### `confirm_shell`

Confirms a shell feature.

Payload:

```ts
{
  feature_id: string;
}
```

### Project Into Sketch

Projection commands require an active sketch. They copy or live-link body
geometry into that sketch.

#### `project_face_into_sketch`

Projects a solid face outline into the active sketch.

Payload:

```ts
{
  face_id: string;
}
```

Use a face ID from `viewport_state.solid_faces[]`. Repeated projection of the
same source is idempotent. Annular circular planar faces project as concentric
sketch circles; non-circular holed planar faces preserve inner loops as
projected sketch lines.

#### `project_profile_into_sketch`

Projects a sketch profile boundary into the active sketch.

Payload:

```ts
{
  profile_id: string;
}
```

Use a profile ID from `document_state.feature_history[].sketch_parameters.profiles[]`
or `viewport_state.sketch_profiles[]`. Polygon profiles produce projected lines
with fixed endpoints; circular profiles produce a projected sketch circle. Inner
loops are included.

#### `project_edge_into_sketch`

Projects a body edge into the active sketch.

Payload:

```ts
{
  edge_id: string;
}
```

Linear edges become sketch lines. Circular edges become sketch circles or arcs
when valid for the current sketch plane.

#### `project_vertex_into_sketch`

Projects a body vertex into the active sketch as a fixed standalone point.

Payload:

```ts
{
  vertex_id: string;
}
```

## State Shapes an Agent Should Remember

### Feature Entries

Every feature in `document_state.feature_history[]` has a stable ID and
feature-specific parameter objects.

Common fields:

```ts
{
  feature_id: string;
  kind: string;
  name: string;
  status: string;
  suppressed: boolean;
  dependency_broken: boolean;
  dependency_warning: string;
  parameters_summary: string;
}
```

Parameter fields:

- `box_parameters: { width, height, depth } | null`
- `cylinder_parameters: { radius, height } | null`
- `extrude_parameters: ExtrudeFeatureParameters | null`
- `loft_parameters: { ruled, sections[] } | null`
- `revolve_parameters: { sketch_feature_id, profile_id, axis_sketch_feature_id, axis_entity_id, axis_start_x, axis_start_y, axis_start_z, axis_end_x, axis_end_y, axis_end_z, angle_degrees } | null`
- `sweep_parameters: { sketch_feature_id, profile_id, path_sketch_feature_id, path_entity_id, path_start_x, path_start_y, path_start_z, path_end_x, path_end_y, path_end_z, path_segments[] } | null`
- `fillet_parameters: { target_body_id, edge_ids, radius, is_pending } | null`
- `chamfer_parameters: { target_body_id, edge_ids, distance, is_pending } | null`
- `shell_parameters: { target_body_id, removed_face_ids, thickness, is_pending } | null`
- `move_parameters: { target_body_id, translation_x, translation_y, translation_z, rotation_x_degrees, rotation_y_degrees, rotation_z_degrees, is_pending } | null`
- `construction_plane_parameters: { plane_type, source_plane_id, source_plane_ids, source_axis_id, offset, angle_degrees, plane_frame } | null`
- `sketch_parameters: SketchFeatureParameters | null`

### Sketch Parameters

When a feature has `kind === "sketch"`, its `sketch_parameters` are the local
2D source of truth:

```ts
{
  plane_id: string;
  plane_frame: PlaneFrame | null;
  active_tool: SketchTool;
  lines: SketchLineEntry[];
  circles: SketchCircleEntry[];
  arcs: SketchArcEntry[];
  fillets: SketchFilletEntry[];
  points: SketchPointEntry[];
  dimensions: SketchDimensionEntry[];
  line_relations: SketchLineRelationEntry[];
  midpoint_anchors: SketchMidpointAnchorEntry[];
  point_line_anchors: SketchPointLineAnchorEntry[];
  projected_points: SketchProjectedPointEntry[];
  projected_sources: string[];
  projections: SketchProjectionEntry[];
  profiles: SketchProfileRegionEntry[];
  pending_mirror: PendingMirrorEntry | null;
}
```

The most common agent reads:

- `lines[].line_id`, `start_point_id`, `end_point_id`
- `circles[].circle_id`
- `arcs[].arc_id`, `start_point_id`, `end_point_id`
- `points[].point_id`, `kind`, `x`, `y`
- `profiles[].profile_id`, `kind`, `points`, `source_circle_id`
- `dimensions[].dimension_id`, `kind`, `value`
- `fillets[].fillet_id`

### Viewport Picks

Use `viewport_state` when selecting existing body topology:

```ts
solid_faces[]: {
  face_id: string;
  owner_id: string;
  owner_kind: string;
  label: string;
  sketchability: string;
  center: Vector3;
  normal: Vector3;
  plane_frame: PlaneFrame;
  is_selected: boolean;
}
```

```ts
edges[]: {
  id: string;
  owner_body_id: string;
  kind: string;
  points: number[];
  length: number;
  is_selected: boolean;
}
```

```ts
vertices[]: {
  id: string;
  owner_body_id: string;
  position: Vector3;
  is_selected: boolean;
}
```

```ts
bodies[]: {
  id: string;
  label: string;
}
```

```ts
sketch_profiles[]: {
  profile_id: string;
  plane_id: string;
  plane_frame: PlaneFrame | null;
  profile_kind: "polygon" | "circle";
  profile_points: { x: number; y: number }[];
  start_x: number;
  start_y: number;
  width: number;
  height: number;
  radius: number;
  is_selected: boolean;
}
```

## Agent Workflow Recipes

### Create a Fresh Document

1. Send `create_document`.
2. Read `document_created.payload.document_id`.
3. Send `get_viewport_state` if you need planes or axes.

Example:

```json
{"id":"cmd-001","type":"create_document","payload":{}}
```

### Create a Box Primitive

```json
{
  "id": "cmd-010",
  "type": "add_box_feature",
  "payload": {
    "width": 80,
    "height": 40,
    "depth": 20
  }
}
```

Then read the new `box` feature from `feature_history`.

### Sketch a Rectangle and Extrude It

1. Send `start_sketch_on_plane` using `ref-plane-xy`.
2. Send `add_sketch_rectangle`.
3. Read `document_state.feature_history[]` and find the active sketch feature.
4. Read `sketch_parameters.profiles[]` from that sketch, or request
   `get_viewport_state` and read `sketch_profiles[]`.
5. Send `extrude_profile` with the profile ID.
6. Send `finish_sketch` if the sketch is still active and the desired UX is a
   completed sketch.

Commands:

```json
{"id":"cmd-020","type":"start_sketch_on_plane","payload":{"reference_id":"ref-plane-xy"}}
```

```json
{
  "id": "cmd-021",
  "type": "add_sketch_rectangle",
  "payload": {
    "start_x": 0,
    "start_y": 0,
    "end_x": 80,
    "end_y": 40,
    "is_construction": false
  }
}
```

After reading `profile_id` from state:

```json
{
  "id": "cmd-022",
  "type": "extrude_profile",
  "payload": {
    "profile_ids": ["profile-id-from-state"],
    "depth": 25,
    "mode": "new_body"
  }
}
```

### Sketch a Circle and Cut a Hole

1. Create or identify an existing target body.
2. Send `get_viewport_state` and choose a planar face from `solid_faces[]`.
3. Start a face sketch with `start_sketch_on_face`, copying the exact
   `plane_frame` from that face.
4. Add a circle in local sketch coordinates.
5. Read the circular profile ID.
6. Send `extrude_profile` with `mode: "cut"` and `target_body_id`.

Important: the AI must not compute its own face frame. Use the core-emitted
`plane_frame`.

### Join Multiple Profiles

1. Draw multiple closed, non-construction profiles in the same sketch.
2. Read all desired `profile_id` values.
3. Send `extrude_profile` with `profile_ids`.
4. Use `mode: "join"` and an explicit `target_body_id` from
   `viewport_state.bodies[]` if joining to an existing body.

### Add a Body Fillet

1. Send `get_viewport_state`.
2. Pick one or more edges from `edges[]` with the same `owner_body_id`.
3. Send `create_fillet`.
4. Read the new feature ID from `feature_history[]` where `kind === "fillet"`.
5. Optionally send `update_fillet_radius` or `update_fillet_edges`.
6. Send `confirm_fillet`.

### Add a Body Chamfer

Same flow as body fillet, but use `create_chamfer`,
`update_chamfer_distance`, `update_chamfer_edges`, and `confirm_chamfer`.

### Project Existing Geometry Into a Sketch

1. Start or reenter a sketch.
2. Send `get_viewport_state`.
3. Choose a body face, edge, or vertex.
4. Send the matching `project_*_into_sketch` command.
5. Read generated lines, circles, arcs, or points from the active sketch's
   `sketch_parameters`.

Projection creates live-link records in `sketch_parameters.projections[]`.
If upstream geometry changes, the core refreshes projected entities during
recompute.

### Use Dimensions to Drive Geometry

1. Draw the geometry.
2. Read the relevant sketch entity IDs.
3. Send an `add_sketch_*_dimension` command.
4. Read the resulting `dimension_id`.
5. Send `update_sketch_dimension` with the desired value (number or expression string).

For simple line length and circle radius dimensions, the core may auto-create
dimensions. Always read state to confirm the dimension exists before updating.

### Use Parameters to Drive Multiple Dimensions

1. Define parameters with `add_parameter` (e.g. `width = 80`, `spacing = width / 4`).
2. Read `document_state.parameters[]` to confirm resolved values.
3. In dimension updates, use expression strings referencing parameter names
   (e.g. `update_sketch_dimension` with `value: "width"` or `value: "spacing * 2"`).
4. When a parameter changes via `update_parameter`, all dimensions referencing it
   re-evaluate automatically.

Parameters are evaluated with a fixpoint loop (max 50 passes). Expressions can
reference other parameters by name. Cycle detection rejects circular references.
Plain numbers and arithmetic can mix freely with parameter names.

## Command Planning Rules for Agents

Use these rules when translating a user request like "draw a bracket with two
holes" into PolySmith commands.

1. Prefer sketch plus extrude for meaningful CAD parts.
2. Use primitive `add_box_feature` / `add_cylinder_feature` only for simple
   standalone solids or quick tests.
3. Always create or load a document before modeling.
4. Start a sketch before adding sketch geometry.
5. Use non-construction geometry for profiles.
6. Use construction geometry only as references, axes, and snap helpers.
7. After creating sketch geometry, read state before using profile, line,
   circle, arc, point, or dimension IDs.
8. After creating solid geometry, read viewport state before using face, edge,
   vertex, or body IDs.
9. For face sketches, copy `plane_frame` from `viewport_state.solid_faces[]`.
10. For booleans, read `viewport_state.bodies[]` and use explicit
    `target_body_id` when possible.
11. For fillet/chamfer, use edge IDs from `viewport_state.edges[]`, not sketch
    line IDs.
12. For sketch fillet, use sketch line IDs and a shared sketch point ID, not
    body edge IDs.
13. Treat `error` as a failed command. Do not assume partial success.
14. Never expose internal IDs to end users. IDs are for agent context and IPC
    only.
15. Define named parameters with `add_parameter` before using them in dimension
    expressions.
16. After changing a parameter, dimension expressions that reference it
    re-evaluate automatically — no need to manually update dimensions.
17. Set `kind` on parameters: `"length"` for mm values, `"angle"` for
    degrees. Angle parameters can only be used in angle-type dimensions
    (`"angle"`, `"line_angle"`).

## Common ID Lookup Patterns

### Find the Active Sketch

From `document_state`:

1. Read `active_sketch_feature_id`.
2. Find `feature_history[]` entry with matching `feature_id`.
3. Use its `sketch_parameters`.

### Find a Newly Created Feature

After a mutating command:

1. Compare current `feature_history[]` with the previous state if available.
2. Otherwise use the last entry of the expected `kind`.
3. Confirm its parameter object is non-null.

### Find a Parameter

From `document_state`:

1. Read `parameters[]`.
2. Search by `name`.
3. Read `resolved_value` for the current evaluated value.
4. Read `kind` (`"length"` for mm, `"angle"` for degrees) to ensure correct
   usage context (angle params only in angle dimensions).
5. Check `has_error` — if true, `error_message` explains why evaluation failed.
6. Parameters reference each other by `name` in their `expression` strings.

### Find Profiles After Drawing

After non-construction closed geometry:

1. Read `active_sketch_feature_id`.
2. Inspect that sketch's `sketch_parameters.profiles[]`.
3. Or send `get_viewport_state` and inspect `sketch_profiles[]`.

### Find a Face to Sketch On

1. Send `get_viewport_state`.
2. Inspect `solid_faces[]`.
3. Prefer faces where `sketchability === "planar"`.
4. Use that entry's `face_id` and `plane_frame` in `start_sketch_on_face`.

### Find Edges for Fillet/Chamfer

1. Send `get_viewport_state`.
2. Inspect `edges[]`.
3. Group candidate edges by `owner_body_id`.
4. Use only edges from one body in a single operation.

## Gotchas

- Gridfinity is available through the bundled plugin, not as native-core
  Gridfinity commands. Use the plugin SDK/command helpers so the command
  includes both the plugin-owned `parameters` JSON and the generic `geometry`
  recipe required by `create_plugin_feature` / `update_plugin_feature`.
  `tapered_rounded_box` plugin geometry may include `top_width`, `top_depth`,
  `top_radius`, `top_offset_x`, and `top_offset_y` for lofted profiles.
  `profile_extrude` plugin geometry may include a `profile_plane` (`xy`, `xz`,
  or `yz`), `profile_points: [{u, v}]`, and `extrude_x` / `extrude_y` /
  `extrude_z` for generic profile solids.
  `rounded_rect_profile_sweep` plugin geometry may include a `yz` profile and
  `path_width`, `path_depth`, and `path_radius` for generic rounded-path
  profile sweeps.
- `select_*` commands are not required before modeling commands that accept
  explicit IDs.
- `extrude_profile` accepts profiles from finished sketches.
- `extrude_face` accepts planar body faces from `viewport_state.solid_faces[]`.
- `finish_sketch` is separate from `extrude_profile`; extrusion can happen
  while a sketch is active.
- `is_construction: true` geometry does not create closed profiles.
- `start_sketch_on_face` requires the exact face `plane_frame` from viewport
  state.
- Body edge IDs and sketch line IDs are different namespaces.
- Body fillet/chamfer commands use body edge IDs.
- Sketch fillet commands use sketch line IDs and sketch point IDs.
- `update_extrude_target_body` clears its explicit target when
  `target_body_id` is omitted.
- The core supports legacy single `edge_id` and `profile_id` payloads in a few
  places, but agents should prefer arrays where available.
- Use `get_viewport_state` after body-changing commands because face and edge
  topology may change.

## Minimal Agent Context Summary

If an AI agent can only keep a compact version of this document in context, keep
this:

- All commands are JSON `{ id, type, payload }`; only `shutdown` may omit `id`.
- CAD state lives in the core. Send commands, then read `document_state` and
  `viewport_state`.
- Units are millimeters. Sketch geometry uses 2D local plane coordinates.
- Origin plane IDs: `ref-plane-xy`, `ref-plane-yz`, `ref-plane-xz`.
- Start sketches with `start_sketch_on_plane` or `start_sketch_on_face`.
- Draw with `add_sketch_line`, `add_sketch_rectangle`, `add_sketch_circle`,
  `add_sketch_arc`, `add_sketch_polygon`, `add_sketch_fillet`,
  `add_sketch_chamfer`, `add_sketch_ellipse`, `add_sketch_slot`,
  `add_sketch_spline`, and `add_sketch_text`; edit with
  `extend_sketch_entity`, `offset_sketch_entity`,
  `transform_sketch_entities`, `create_linear_array`, and
  `create_circular_array`.
- Closed non-construction geometry creates `sketch_profiles`.
- Extrude profiles with `extrude_profile { profile_ids, depth, mode,
  target_body_id? }`, or planar body faces with
  `extrude_face { face_id, depth, mode, target_body_id? }`, where mode is
  `new_body`, `join`, or `cut`.
- Update extrudes with `update_extrude_depth`, `update_extrude_mode`,
  `update_extrude_target_body`, and `update_extrude_profiles`.
- Loft sketch profiles with `loft_profiles { profile_ids, ruled? }`; update
  lofts with `update_loft_profiles` and `update_loft_ruled`.
- Revolve one sketch profile around one sketch line with
  `revolve_profile { profile_id, axis_entity_id, angle_degrees? }`; update
  revolves with `update_revolve_profile`, `update_revolve_axis`, and
  `update_revolve_angle`.
- Sweep one sketch profile along one sketch line with
  `sweep_profile { profile_id, path_entity_id }`; update sweeps with
  `update_sweep_profile` and `update_sweep_path`.
- Read `viewport_state.bodies[]` for boolean targets.
- Read `viewport_state.solid_faces[]` for face sketches and copy `plane_frame`.
- Read `viewport_state.edges[]` for body fillet/chamfer.
- Read sketch `lines[]`, `circles[]`, `arcs[]`, `points[]`, `profiles[]`, and
  `dimensions[]` from `feature_history[].sketch_parameters`.
- Use `create_fillet` / `create_chamfer` for body edges; use
  `add_sketch_fillet` for sketch corners.
- Projection commands are `project_face_into_sketch`,
  `project_profile_into_sketch`, `project_edge_into_sketch`, and
  `project_vertex_into_sketch`.
- Never invent IDs. Never expose IDs in user-facing UI copy.
