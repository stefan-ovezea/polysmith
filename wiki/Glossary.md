# Glossary

This glossary defines the terms used in the PolySmith project. Use these
definitions in commits, bug reports, spec discussions, and AI agent prompts
to keep communication precise and consistent.

---

## CAD Concepts

### Body
A 3D solid resulting from a feature operation (e.g., an extrude). Bodies are
the physical objects in the model. Each body has a boundary representation
(B-rep) composed of faces, edges, and vertices.

### Feature
The fundamental unit of the parametric model. Every operation that creates
or modifies geometry is a feature (sketch, extrude, fillet, etc.). Features
form an ordered history tree. Internal storage: `Feature` struct with a
unique `feature_id`, a `FeatureKind` enum, and an index into the feature
history.

### Sketch
A 2D drawing on a plane. Sketches contain entities (lines, circles, arcs,
points) with constraints and dimensions. Sketches are the primary input for
3D features like extrude. A sketch is itself a feature kind.

### Extrude
The operation that turns a closed sketch profile into a 3D solid body by
extending it along a direction (usually normal to the sketch plane). An
extrude feature stores a `sketch_feature_id`, `depth`, and an optional
`direction`. Extrudes produce bodies with faces, edges, and vertices.

### Profile
A closed loop of sketch entities that forms a valid region for extrusion.
Profile detection walks the sketch graph to find closed paths composed of
connected lines, circles, and arcs. Nested profiles (loops inside loops)
produce extrudes with holes. The core stores profiles as `SketchProfile`
entries keyed by `profile_id`.

### Face
A bounded surface on a 3D body. Faces are identified by a face index within
the body. Faces are the target of operations like starting a new sketch on a
face, or applying a fillet to a face.

### Edge
A boundary curve of a face. Edges are formed where two faces meet, or where
a face terminates. Edge selection is used for fillet, chamfer, and as
cutting edges in sketch trim operations.

### Vertex
A point where edges meet. Vertices are the 0-dimensional elements of the
B-rep. Internal identification uses OCCT topology indices.

### Plane
A 2D coordinate frame in 3D space. Sketches are drawn on planes. Planes are
defined by an origin point and a normal direction (e.g., "XY plane").

### OCCT / OpenCascade
The OpenCascade Technology geometry kernel. PolySmith links against OCCT for
all B-rep operations: shape creation, Boolean operations, meshing, STL
export, and geometric queries. OCCT is vendored as a submodule at
`third_party/occt/`.

---

## Architecture

### CAD Core
The C++ native process (`native/cad-core/`) that owns all CAD state,
geometry, and modeling logic. The core is the single source of truth. The UI
never owns CAD state. The core communicates with the UI via JSON IPC over
stdin/stdout.

### UI (React)
The React + TypeScript frontend (`apps/desktop-ui/`). Responsibilities:
rendering the UI, capturing user input, sending commands to the core,
receiving and displaying core events. The UI does NOT own CAD state.

### Tauri
The desktop shell that bridges the React UI and the native CAD core. Tauri
manages the window, file dialogs, and menu bar. In PolySmith, Tauri spawns
the CAD core as a child process and pipes stdin/stdout for the JSON IPC
protocol.

### IPC Protocol
The JSON-based communication contract between the UI and the CAD core.
Commands (UI → core) are newline-delimited JSON objects with a `type` field.
Events and state snapshots (core → UI) are also newline-delimited JSON. The
protocol is documented in `IPC-Protocol.md` and validated against
`protocol/schema/commands.schema.json`.

### Command
A JSON message sent from the UI to the CAD core, representing a user intent
(e.g., `add_sketch_line`, `update_extrude_depth`, `delete_feature`). Every
command has a `type`, an `id`, and type-specific payload fields.

### State (document_state / viewport_state)
Two JSON snapshots the core sends back to the UI after every command:

- **document_state**: the full semantic model — features, sketch entities,
  constraints, dimensions, profiles, parameters, selection.
- **viewport_state**: rendering data — vertex buffers, mesh data, sketch
  overlays, and camera posture for the 3D viewport.

### Recompute
The process of rebuilding the CAD model from the feature history after a
change. When a feature parameter changes, the core re-executes the feature
list from that point forward. Recomputes can change face/edge/vertex indices
— see Topological Naming Problem.

### B-rep (Boundary Representation)
The standard solid modeling representation used by OCCT. A solid is
represented by its boundaries: faces (surfaces), edges (curves), and
vertices (points), with explicit adjacency relationships.

### Mesh
A triangle mesh approximation of a B-rep body, used for rendering in the
viewport. The core generates meshes from B-rep shapes using
`BRepMesh_IncrementalMesh` and sends them to the UI as `viewport_state`.
Meshes are not the source of truth; B-rep is.

---

## Topological Naming Problem (TNP)

### TNP
The topological naming problem: when an upstream feature is modified, OCCT
may renumber faces, edges, and vertices. A downstream feature that
referenced them by raw index will now point at the wrong geometry. This is
the most notorious bug class in parametric CAD.

### Topology Index
An OCCT-internal integer that identifies a face, edge, or vertex within a
shape's boundary representation. These indices are not stable across
recomputes. **Never store a naked topology index as a persistent reference.**

### dependency_broken
A flag on a feature or projection indicating that its upstream topological
reference could not be re-resolved on recompute. Features with
`dependency_broken` show a warning in the UI but do not crash. The user can
re-select the target to fix the link.

### Re-resolution
The process of re-identifying a referenced geometric element (face, edge,
vertex) on every recompute by matching geometric properties (position,
normal, area, etc.) rather than relying on OCCT topology indices. Every
PolySmith feature that references 3D geometry must re-resolve on recompute.

---

## Sketch System

### Sketch Entity
An individual geometric element in a sketch: line, circle, arc, point, or
polygon. Each entity has a unique `entity_id` and carries its defining
parameters (endpoints for lines, center + radius for circles, etc.).

### Constraint
A geometric rule applied to sketch entities. Two categories:

- **Geometric constraint**: defines relationships between entities
  (horizontal, vertical, parallel, perpendicular, coincident, tangent,
  concentric, equal, symmetric, point-on-object).
- **Dimensional constraint**: specifies a measurement (distance, radius,
  diameter, angle). Stored as `SketchDimension` with a `value` and optional
  formula `expression`.

### Auto-Inference
The system that automatically applies constraints when entities are created.
For example, drawing a near-horizontal line auto-infers a horizontal
constraint. Inference is based on geometric tolerances (angle, distance) at
creation time.

### Selection Filter
A unified controls model that governs three behaviors simultaneously:
selection, snapping, and constraining. The user toggles entity types
(lines, circles, arcs, points, construction lines) through checkboxes. If a
type is not selectable, it cannot be snapped to or constrained. This is
documented in `Sketch-Selection-Controls.md`.

### Snap
A cursor-gravity behavior that pulls the drawing cursor to key geometric
points on existing entities. Snap types include: grid, endpoint, midpoint,
center, quadrant, intersection, nearest, perpendicular, parallel, tangent,
polar. To enable live snaps, the entity type must be enabled in the
selection filter.

### Snap Types

PolySmith supports the following snap behaviors, each toggleable in the
Selection and Snap Filter panel:

**Endpoint Snap** — pulls cursor to the start or end point of a sketch
line or arc.

**Midpoint Snap** — pulls cursor to the exact middle of a sketch line
segment. Sub-segment midpoints (when a line is split by anchored points)
are also supported.

**Center Snap** — pulls cursor to the center point of a circle, arc, or
polygon.

**Grid Snap** — pulls cursor to the nearest grid intersection point.

**Grid Line Snap** — locks cursor to the nearest horizontal or vertical
grid line axis. Unlike Grid Snap which snaps to intersections, this snaps
to the line itself (one coordinate locked, the other free).

**Intersection Snap** — pulls cursor to points where two sketch entities
cross (line-line, line-arc).

**Nearest Snap** — pulls cursor to the closest point on a sketch line
segment (projection clamped to the segment interior).

**Quadrant Snap** — pulls cursor to the 0, 90, 180, and 270 degree
cardinal points on a circle.

**Perpendicular Snap** — projects the cursor onto the perpendicular
foot from an existing line.

**Parallel Snap** — while drawing a line, locks the cursor to the
direction of the nearest existing line.

**Tangent Snap** — while drawing a line from outside a circle, pulls
the cursor to the nearest tangent point on that circle.

**Polar Snap** — locks the cursor to polar angle increments (default 15
degrees) from the draft start point. Angle step is configurable (5 to 90
degrees).

### Object Snap Override
A modifier-key mechanism that temporarily inverts all snap toggles. Hold
Alt while sketching to reverse every snap checkbox: disabled types become
active, enabled types become inactive. Release Alt to restore the panel
settings.

### Object Snap Tracking (Planned)
A drawing assistant that projects guide lines from existing geometry
endpoints. The user briefly hovers over an existing point to "acquire"
it, then moves the cursor away. A temporary dotted guide line extends
horizontally and/or vertically from the acquired point. When the active
draft (rubber-band line) crosses this guide, the cursor snaps to the
intersection of the guide line and the rubber-band direction.

Example: an existing vertical line A has its top endpoint at Y=5. The
user starts a new vertical line B at X=3, hovers near A's top endpoint
(acquiring it), then drags upward. A horizontal dotted guide extends
from Y=5. Line B snaps to end at exactly Y=5, matching line A's height
without an explicit dimension.

Implementation:
- Acquire: detect hover near existing endpoints/centers while drafting.
  Store the point in a transient tracked-point list.
- Render: draw dashed guide lines from tracked points along cardinal
  axes using a dedicated Three.js line material.
- Snap: compute the intersection of the active rubber-band direction
  with each guide line. Closest intersection within tolerance wins.
- Cleanup: clear tracked points when the draft commits or cancels.

### Dimension
A displayed measurement on a sketch entity. Dimensions can be driving
(control the geometry) or driven (report the current value). Dimensions
support formula expressions referencing the document parameter table (e.g.,
`width / 2`).

### DOF (Degrees of Freedom)
The number of independent ways a sketch entity or the entire sketch can move
while still satisfying all current constraints. A fully-constrained sketch
has 0 DOF. The UI displays DOF count and colors under-constrained entities
in blue (as opposed to black for fully-constrained). Internal calculation:
`DOF = 2 * vertices − constraint_equations`.

### Construction Entity
A sketch entity marked as construction-only. Construction lines, circles,
and points are visible in the sketch but do not participate in profile
detection and are not extruded. Used for reference geometry.

### Profile Detection
The algorithm that walks the sketch entity graph to find closed loops
suitable for extrusion. Handles mixed line-arc profiles and nested loops
(for holes). Runs on every sketch change (entities added, modified, or
deleted).

---

## Sketch Tools

### Tool (SketchTool)
A modal state in the sketch environment representing the active drawing
operation. Examples: Line, Rectangle, Circle, Arc, Polygon, Fillet, Project,
Mirror, Trim, Select. The toolbar shows which tool is active; clicking a
tool deselects the previous one and enters that tool's draw mode.

### Drafting
The process of drawing a new sketch entity with ongoing cursor feedback. The
UI shows a draft preview (live geometry) as the user moves the cursor. On
confirm (click or Enter), the entity is committed to the core.

### Fillet
A tool that rounds the corner between two sketch entities (lines or arcs)
with a specified radius. The fillet replaces the corner with an arc segment.

### Trim
A tool that removes portions of a sketch entity by cutting it at
intersection points with other entities. The user clicks the portion to
delete — the highlighted segment under the cursor dies and the rest stays.
One trim operation affects exactly one entity (or, with drag-paint, one
stroke of entities committed as one undo step). Algorithm: intersection
detection → entity splitting → segment selection → commit. Surviving
pieces inherit applicable constraints (see
[Trim-Tool-Redesign-Requirements.md](Trim-Tool-Redesign-Requirements.md),
decision D1). Companion tools: **Corner Trim** (two entities → virtual
corner), **Extend** (stretch to the first intersection), **Split** (divide
at all intersections without deleting). Documented in
`Trim-Tool-Implementation-Plan.md` and `Trim-Tool-Redesign-Requirements.md`.

### Project (Sketch Projection)
A tool that copies edges, faces, or vertices from existing 3D geometry into
the active sketch as fixed sketch entities. Projected entities maintain a
live link to their source geometry; if the source changes shape, the
projection updates or flags `dependency_broken`.

### Mirror
A sketch tool that creates a mirrored copy of selected entities across a
mirror line. Follows the contextual modeling pattern: start preview →
select objects → select axis line → confirm.

---

## State & Document

### Document
The top-level container for all CAD state. A `DocumentState` holds the
feature history, parameter table, and active selections. Documents can be
saved to and loaded from `.polysmith` JSON files.

### Feature History
The ordered list of all features in the document. Adding a feature appends
to the end; deleting a feature cascades to its dependents. The history is
the basis for recompute: to rebuild, the core replays features in order.

### Undo / Redo
Stacks maintained by `DocumentManager`. Every mutating operation pushes a
`DocumentState` snapshot onto the undo stack and clears the redo stack.
Undo restores the previous snapshot; redo re-applies it. Hotkeys:
Ctrl/Cmd+Z (undo), Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y (redo).

### Geometry Revision
A counter that increments on every geometry-affecting operation. The UI uses
this to detect when to re-render meshes. Denoted `bump_geometry_revision()`
in the core.

### Parameter Table
A document-scoped table of named parameters (e.g., `width`, `height`,
`thickness`). Each parameter has a `name`, an `expression` (formula), and a
`resolved_value`. Sketch dimensions can reference parameters in their
expressions.

### Formula Expression
An arithmetic expression referencing parameter names, e.g., `width / 2`,
`sqrt(width^2 + height^2)`. Evaluated by the recursive-descent expression
evaluator in `formula_eval.cpp`. Supports `+`, `-`, `*`, `/`, parentheses,
and unary minus. Cycle detection via a `resolving` set.

---

## Viewport & Rendering

### Viewport
The 3D rendering surface in the UI where the model is displayed. The
viewport receives `viewport_state` JSON from the core containing mesh data
and sketch overlays. The viewport also captures mouse events (click, drag,
hover) and sends them to the core as selection/input commands.

### Viewport Overlay
2D rendering drawn on top of the 3D viewport (e.g., sketch dimensions,
constraint badges, axis lines, grid). Overlays are defined in `viewport.cpp`
and rendered on the UI side from the viewport state.

### Camera
The viewpoint from which the viewport renders the scene. The core sends
camera posture (position, target, up vector) in viewport state. The orbit
cube provides interactive rotation; double-clicking a face frames the camera
for a new sketch on that face.

### STL Export
Converts a B-rep body to an STL triangle mesh file for 3D printing. The core
uses `BRepMesh_IncrementalMesh` + `StlAPI_Writer` to generate the STL.

---

## Serialization & Storage

### .polysmith file
The native PolySmith document format — a JSON file containing the full
feature history, sketch entities, constraints, dimensions, parameters, and
projection records. Read and written by `serialization.cpp`.

### Payload
A JSON object representing a single feature or sketch entity's serialized
data. `to_payload()` serializes; `from_payload()` or
`sketch_parameters_from_payload()` deserializes. Payloads carry a `type`
field and type-specific fields.

### Serialization
The process of converting in-memory C++ state to JSON (for `.polysmith`
files and IPC). `serialization.cpp` handles both feature parameters and
viewport primitives.

---

## Development

### Contextual Modeling Workflow
The binding UX pattern for all modeling features:
1. Select input(s)
2. Invoke action (hotkey or toolbar)
3. Floating context panel with real geometry preview
4. Confirm or cancel

Every new feature must follow this pattern. Documented in
`Contextual-Modeling-Workflow.md`.

### Live Preview
Real geometry computed by the core and rendered in the viewport during a
pending action. The UI polls viewport snapshots but does not invent geometry
locally. Extrude preview `update_extrude_depth` is the canonical example.

### CAD Agent
An AI assistant that interacts with PolySmith through the IPC protocol. CAD
agents send commands like any other protocol client but generate them from
natural-language descriptions. The agent does not bypass the IPC protocol or
write directly to the core process.

### Bootstrap
The one-time setup command (`pnpm bootstrap`) that syncs submodules,
installs dependencies, and builds OpenCascade and the native CAD core.
Required before the first `pnpm dev`.

---

## Conventions

- **id suffix**: internal identifiers use `_id` suffix (e.g., `feature_id`,
  `sketch_id`, `entity_id`, `profile_id`). These are opaque strings never
  exposed in user-facing UI.
- **snake_case**: C++ identifiers follow `snake_case` (e.g.,
  `document_state`, `selected_feature_id`).
- **camelCase**: TypeScript identifiers follow `camelCase` (e.g.,
  `documentState`, `selectedFeatureId`).
- **kebab-case**: wiki page filenames use kebab-case with hyphens for spaces
  (e.g., `Architecture-Overview.md`).
- **entity vs. feature**: an *entity* is an element inside a sketch
  (line, circle); a *feature* is a top-level operation in the history tree
  (sketch, extrude, fillet).