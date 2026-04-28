# PolySmith V1 Roadmap

## Project Focus

PolySmith v1 is intentionally narrow:

- local-first desktop CAD
- hobbyist 3D-printing workflows
- single-part parametric modeling
- a familiar workflow inspired by Fusion 360
- a strong architecture boundary between UI and native CAD logic

This roadmap intentionally avoids CAM, cloud collaboration, simulation,
enterprise features, and complex assemblies.

## Current Repo Status

The earlier "infrastructure-heavy, feature-light" phase is done. The
codebase now has:

- a React + Tauri desktop shell with the `Midnight Carbon` design language
- a C++ CAD core built with CMake on top of OpenCascade
- a JSON IPC bridge with documented commands and a versioned schema
- a real document model with feature history, undo/redo, and core-owned
  selection state
- 2D sketch entities (lines, rectangles, circles), points, dimensions,
  constraints (horizontal/vertical/perpendicular/parallel/equal-length/
  coincident/fixed), and stored sketch profiles
- closed-profile detection that survives parametric edits and point merges
- extrude features that target sketch profiles, with editable depth via
  `update_extrude_depth`
- selectable solid faces and reference planes; sketches can start on any
  origin plane or any solid face
- finished sketches can be re-entered without rebuilding via `reenter_sketch`
- STEP export of the live document
- a Fusion-style UX pattern documented in
  `docs/architecture/fusion-style-behavior.md`: select inputs → invoke
  action → floating context panel → real geometry preview → confirm/cancel
- an `E` hotkey + floating extrude preview panel that drives live, debounced
  `update_extrude_depth` recomputes
- a Fusion-like document hierarchy with collapsible Origin / Sketches /
  Bodies categories, eye-icon visibility toggles, double-click to re-enter
  sketches, and a right-click context menu for rename / hide / delete

That puts the project past Milestones 0–2 of the original v1 roadmap and
roughly mid-way through Milestone 3.

## Architectural Invariants (Do Not Break)

These are rules going forward, not goals to chase:

- React UI does **not** own CAD state. The native core is the single
  source of truth for documents, features, geometry, and selection.
- The IPC protocol is the contract. Schema, TypeScript types, C++ command
  dispatch, and `docs/architecture/ipc-protocol.md` move together.
- All modeling features follow the Fusion-style UX pattern in
  `docs/architecture/fusion-style-behavior.md`.
- Live previews are real geometry recomputed by the core. The UI does not
  invent geometry locally.
- Changes stay minimal, scoped, and reviewable. No vibe-coded rewrites.

## Where We Are Going

The next phase is the modeling slice that turns PolySmith from "extrude a
single profile" into "model a printable part you'd actually want to print".

### Tier 1 — make modeling actually useful

These three close the gap between fancy demo and real workflow:

- **Cut / subtract extrude.** Add `New Body | Join | Cut` modes to the
  Extrude action. Single largest UX unlock.
- **Save / load `.polysmith` document.** A core-owned JSON document
  format, plus File → Open / Save / Save As. Required for any real
  iteration.
- **STL export.** Sibling of the existing STEP export, targeted at
  3D-printing slicers.

### Tier 2 — the obvious next features

- **Edge & vertex selection.** Currently only faces are selectable.
  Required prerequisite for fillet/chamfer/measure.
- **Fillet & chamfer on edges.** Practically required for printable parts.
- **Hole feature.** Parametric simple/counterbore/countersink on a face,
  built on top of cut extrude.
- **Pattern features.** Linear and circular patterns of features and
  bodies.
- **Mirror.** Body / feature mirror about a plane.

### Tier 3 — modeling primitives & references

- **Offset construction plane.** Sketch on a plane offset from a face or
  reference plane.
- **Sketch arcs, slots, polygons, and offset-curve.**
- **Construction axes** through edges and through two points.

### Cross-cutting polish

Small individually but they shape day-to-day usability:

- **Undo / redo hotkeys** (`⌘Z` / `⌘⇧Z` and Ctrl equivalents).
- **Measure tool** (point-to-point, edge length, face area).
- **Named user parameters** that drive sketch dimensions and feature
  depths.
- **View cube / named views** (Front / Top / Iso).
- **Active sketch panel** showing entities in the same hierarchy treatment.

## Suggested Order

1. Cut extrude → save/load → STL export.
2. Edge & vertex selection plumbing → fillet & chamfer.
3. Pattern + mirror.
4. Hole feature.
5. Polish: undo/redo hotkeys, measure tool, named parameters, view cube.

Each row above maps cleanly onto the existing Fusion-style action pattern
(select inputs → invoke action → floating panel → live preview →
confirm/cancel) and reuses the panel + hotkey machinery already built.

## Key Decisions and Constraints

- The UI does not own CAD state.
- Tauri acts as the bridge between UI and native systems, not as a second
  CAD logic layer.
- The IPC protocol is the contract of the system. The bridge is currently
  fire-and-forget; flows that depend on post-command state must subscribe
  to the next document/viewport event (see `awaitDocumentChange` in the
  store) rather than reading the store immediately after sending a
  command.
- V1 stays single-part and local-first.
- Changes should remain minimal, readable, and reviewable.
- Broad rewrites should be avoided unless clearly justified.

## Near-Term Recommended Next Task

Implement Tier 1 in order: cut extrude, then save/load, then STL export.
Each is a distinct focused turn. Cut extrude is the highest-impact next
feature and the most obvious user-visible gain.
