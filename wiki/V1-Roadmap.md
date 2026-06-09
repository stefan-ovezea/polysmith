# PolySmith V1 Roadmap

## Project Focus

PolySmith v1 is intentionally narrow:

- local-first desktop CAD
- hobbyist 3D-printing workflows
- single-part parametric modeling
- a familiar, modern parametric CAD workflow
- a strong architecture boundary between UI and native CAD logic

This roadmap intentionally avoids CAM, cloud collaboration, simulation,
enterprise features, and complex assemblies.

## Current Repo Status

The original v1 milestones 0–3 are complete. The codebase now has:

- a React + Tauri desktop shell with the `Midnight Carbon` design language
- a C++ CAD core built with CMake on top of OpenCascade 7.8
- a JSON IPC bridge with documented commands and a versioned schema
- a real document model with feature history, undo/redo, and core-owned selection state
- 2D sketch system: lines, rectangles, circles, arcs, points, polygons, dimensions,
  geometric constraints (H/V, coincident, parallel, perpendicular, equal-length, tangent,
  concentric, point-on-object), dimensional constraints (length, radius, angle, distance),
  and stored sketch profiles with inner-loop/hole support
- closed-profile detection that survives parametric edits, point merges, and
  mixed line+arc loops
- extrude features: New Body, Join, Cut modes with one-side, symmetric, two-side extent,
  taper, thin, Through All / To Object / To Next, and explicit target-body selection
- Revolve, Sweep, and Loft features with live preview panels
- Box and Cylinder primitives
- Shell body-modifying feature
- Edge & vertex selection, Fillet & Chamfer on edges with live preview panels
- Hole feature: simple, counterbore, countersink, spotface; metric/imperial standard
  presets with clearance/tap-drill/threaded fit selection
- Sketches on origin planes, solid faces, or construction planes; sketch re-entry
- 2D sketch fillets (line-line corners)
- Project tool (face/edge/vertex) with live parametric re-projection links
- Save/load `.polysmith` documents (core-owned JSON format)
- STEP + STL export (honours cut/join booleans)
- OrcaSlicer integration (native window embedding via X11/XWayland)
- Offset construction planes (offset, midplane, tangent, angle-at-plane)
- Construction axes (from sketch lines / straight body edges) and construction points
- Unified selection filter panel: selection, snapping, and constraints controlled by
  one checkbox panel with localStorage persistence
- Comprehensive snap system: endpoint, midpoint, center, quadrant, intersection,
  nearest, tangent, perpendicular, parallel, polar, grid, grid-line
- DOF counting and entity colouring (blue = fully constrained, red = over-constrained,
  yellow = under-constrained)
- Inference engine for auto-creating coincident/concentric constraints at commit time
- Constraint badge click + Delete / right-click → Delete
- Dimension tool: single-entity, two-entity (angle, distance, line-line distance,
  circle-line distance), placement drag, regroup-aware first-click handling
- Draft dimension visualisation (Three.js-rendered dimension geometry during line
  drafting, replacing HTML-only inputs)
- On-demand sketch dimensions: auto-dimensions created only when user types a value;
  drag-only creation produces no dimension
- Per-dimension radius/diameter toggle and driven (reference) dimension support
- Trim tool: line-line, line-circle, line-arc, circle-circle, circle-arc, arc-arc
  intersections; entity splitting; constraint re-evaluation; core-driven hover preview
- Parametric parameters & dimension formulas: document-scoped name → expression →
  resolved value, recursive-descent evaluator with cycle detection
- Endpoint drag with planegcs WASM solver (60 fps local feedback, core commit on
  mouse-up). See [Planegcs-Dual-Solver](Planegcs-Dual-Solver).
- Rectangle drag-selection (left→right = window, right→left = crossing)
- 3D Move tool with local-axis translation/rotation manipulator
- Body copy: linked (re-resolves source) and independent (frozen snapshot) modes
- Materials: body/face colour overrides with HSV picker, saved in `.polysmith`
- Helix, Thread, and Fastener features (threaded holes, cosmetic + modeled threads;
  see Known Modeling Issues below for modeled-thread caveats)
- CAM workspace scaffolding: UI skeleton with Milling/Turning/Printing/Cutting tabs,
  TNP witness resolution for face references, face milling operation
- Natural-language AI command bar
- View cube with cardinal face snaps, sketch-plane rotation arrows, orthographic camera
- Dynamic grids (zoom-aware millimetric spacing, sketch-plane back grid)

## Architectural Invariants (Do Not Break)

These are rules going forward, not goals to chase:

- React UI does **not** own CAD state. The native core is the single
  source of truth for documents, features, geometry, and selection.
- The IPC protocol is the contract. Schema, TypeScript types, C++ command
  dispatch, and `IPC-Protocol` move together.
- All modeling features follow the contextual modeling workflow in
  `Contextual-Modeling-Workflow`.
- Live previews are real geometry recomputed by the core. The UI does not
  invent geometry locally.
- Changes stay minimal, scoped, and reviewable. No vibe-coded rewrites.

## What Remains (V1 Polish & Follow-Up)

### Features still to build

| Feature | Notes |
|---|---|
| **Pattern features** | Linear and circular patterns of features/bodies |
| **Measure tool** | Point-to-point, edge length, face area |
| **Display units toggle** | Metric/inch. Architecture designed in `Display-Units`. Core always mm; UI converts at presentation boundary. |
| **Text tool** | Text as sketch entities via OCCT `StdPrs_BRepFont`. Plan in `Text-Tool-Implementation-Plan`. |
| **Sketch arc constraints & dimension drive** | Arc endpoints are fixed for v1; reshape/dimension-drive follow-up |
| **Line-arc and arc-arc sketch fillets** | Currently line-line only |
| **Perpendicular snap** | General perpendicular-to-line; perpendicular-foot (start-on-host-line) works |
| **Driven dimension proposal** | Auto-offer driven dim when entity already fully constrained |

### Known Modeling Issues

- ⚠️ **Modeled thread geometry is bugged.** The semantic hole/thread/fastener
  feature stack exists and cosmetic thread markers are usable, but modeled
  threads are not reliable yet. Generated fastener threads can lose the helical
  shaft, split into detached pieces, or export as incomplete screw geometry.
  Treat modeled fastener threads, standalone modeled thread cuts, and modeled
  threaded holes as experimental until the native thread construction path is
  reworked and validated against viewport and export.

### CAM

CAM is in early scaffolding. The workspace UI skeleton is wired, TNP witness
resolution for face references is implemented and tested, and a basic face
milling operation exists. The full plan is in [CAM-Development](CAM-Development).

## Key Decisions and Constraints

- The UI does not own CAD state.
- Tauri acts as the bridge between UI and native systems, not as a second
  CAD logic layer.
- The IPC protocol is the contract of the system. The bridge is fire-and-forget;
  flows that depend on post-command state must subscribe to the next
  document/viewport event (see `awaitDocumentChange` in the store) rather than
  reading the store immediately after sending a command.
- V1 stays single-part and local-first.
- Changes should remain minimal, readable, and reviewable.
- Broad rewrites should be avoided unless clearly justified.

## Near-Term Recommended Next Tasks

1. **Pattern features** — linear and circular, highest remaining UX impact
2. **Measure tool** — small, self-contained, high day-to-day value
3. **Display units toggle** — UI-layer only, architecture already designed
4. **Text tool** — blocked by font bundling decision but plan is written
