# Sketch Endpoint Drag

> **Status as of 2026-05-29:** Basic endpoint drag shipped. Development paused
> in favour of a dedicated constraint-system branch. After constraint fixes
> land, this will expand to **Point Drag** — the ability to drag any sketch
> point, not just line endpoints. That includes intersection points, shared
> coincident points between geometries, circle centers, and construction
> points. Line rubber-bands during
> drag with snap resolution. H/V constraints enforce axis-aligned stretch
> (industry-standard). Constraint relaxation (auto-delete conflicting
> constraints on coincident snap) is planned but not yet implemented. Several
> constraint-interop bugs identified — see Known Gaps below.

## Overview

In **Select mode** (Esc from any tool), the user can grab a sketch line
endpoint and drag it to a new position. The line stretches/rotates in real
time with snap resolution. On release, the new position is committed through
`update_sketch_point` IPC.

## Interaction Flow

1. **Hover** — cursor changes near a non-fixed endpoint
2. **Mousedown** — drag state captured, orbit controls disabled
3. **Drag** — endpoint follows cursor with snap resolution (endpoint,
   midpoint, grid). The line rubber-bands: the dragged endpoint moves, the
   other endpoint stays anchored.
4. **Release** — `update_sketch_point(pointId, x, y)` IPC fires. Core runs
   `propagate_connected_point_move` → `refresh_sketch_derived_state`, which
   re-derives line caches, dimensions, fillets, and constraints.

If the cursor didn't move more than 4 px, the gesture is treated as a click
and falls through to the existing point-selection logic.

## Constraint Behavior During Drag

### Active constraints (horizontal / vertical)

The industry standard (Fusion 360, SolidWorks, Onshape) is that constraints
are **maintained** during drag — never silently broken.

| Constraint | Allowed movement | Blocked |
|---|---|---|
| Horizontal | X axis (stretch / shorten) | Y axis |
| Vertical | Y axis (stretch / shorten) | X axis |

The C++ layer enforces this in `propagate_connected_point_move`
(`sketch_feature.cpp`): after `set_endpoint` updates the dragged endpoint,
the H/V block snaps it back to the constraint axis while keeping the
anchored endpoint fixed. The line **stretches** rather than rigidly
translating (the old behaviour).

### Equal-length relations

Lines with `equal_length` relations propagate length changes through
`enforce_equal_length_relations`. If line A is dragged longer and line B
has an equal-length relation to A, line B is driven to match.

### Driven dimensions

Auto-created line-length dimensions are **driven** (read-only). They update
their displayed value to match the new geometry after the drag commits.
They do not constrain the line.

### Fixed points

Points with `is_fixed = true` (arc endpoints, projected points, quadrant
points) are never draggable. The drag is silently skipped.

## Snap Feedback

During drag, the snap system (`resolveSnappedSketchPoint`) resolves the
nearest snap candidate on every pointer-move. The snap label is displayed
in the viewport status bar.

## Constraint Interoperability

### Perpendicular + H/V coexistence

Applying a perpendicular constraint to a line that already carries an H/V
constraint triggers a partial-failure bug in
`set_sketch_perpendicular_constraint` (`sketch_feature.cpp:1975`):

1. The line's H/V constraint is cleared **and** the perpendicular relation
   is persisted to `line_relations` (lines 1999–2009).
2. `drive_line_perpendicular_to_reference` runs successfully on the first line
   (its H/V was cleared).
3. `enforce_perpendicular_relations` (line 2031) then tries to drive **both**
   lines. The second line still holds its H/V constraint, so
   `drive_line_perpendicular_to_reference` (line 1300) throws:
   *"Cannot drive a perpendicular relation on a line that still has an axis
   constraint."*

**Result:** The error propagates to the UI, but the relation record is
already committed. The viewport swaps the H/V constraint badge for a
perpendicular badge because `relation_constraint_line_ids`
(`viewport.cpp:3399`) suppresses H/V badges for any line that appears in a
relation. The constraint partially applies — persistent across recomputes
but errored during creation.

**Fix target:** The relation commit and enforcement should be atomic. If
enforcement fails, the relation must be rolled back. Additionally, when
a vertical and horizontal line share a coincident endpoint (rectangle
corner), the 90° angle is implicit — the perpendicular relation should be
recognized as redundant rather than throwing.

### Badge stacking

When a line carries both an H/V constraint and a relation (perpendicular,
equal-length, parallel, tangent), only the relation badge is rendered.
The H/V badge is suppressed by the `relation_constraint_line_ids` guard
(`viewport.cpp:3461–3463`). This means the user cannot see all active
constraints on a line simultaneously. The constraint-badge layout should
stack multiple badges or show them at distinct offsets so every active
constraint is visible.

## Constraint Relaxation (Future)

When the user drags an endpoint onto another point and a coincident snap
fires, conflicting constraints (e.g. the line's own H/V constraint) could
be auto-deleted and replaced with the coincident constraint. This is how
Fusion 360 handles the "connect this horizontal line to a point at a
different elevation" workflow.

### Proposed rules

1. If the release snap is a **coincident** (endpoint-to-endpoint):
   - Delete the dragged line's H/V constraint if present
   - Delete the dragged line's equal-length relations if present
   - Create a coincident constraint between the two points

2. If the release snap is not coincident:
   - Maintain all existing constraints (current behaviour)

3. The user should see a visual indicator (constraint icon) during drag
   showing what constraint would be applied on release.

## Implementation Files

| Layer | File | What |
|---|---|---|
| UI — drag interaction | `apps/desktop-ui/src/layout/ViewportPanel.tsx` | `endpointDragRef`, detection in `handlePointerDown`, rubber-band in `handlePointerMove`, commit in `handlePointerUp` |
| UI — props | `apps/desktop-ui/src/layout/ViewportPanel.tsx` | `onUpdateSketchPoint` prop |
| UI — wiring | `apps/desktop-ui/src/App.tsx` | `updateSketchPoint` from `useCadCore` |
| C++ — point update | `native/cad-core/src/core/sketch_feature.cpp` | `update_sketch_point` → `propagate_connected_point_move` |
| C++ — H/V stretch | `native/cad-core/src/core/sketch_feature.cpp` | H/V block in `propagate_connected_point_move` — snap to axis, don't rigidly translate |
| C++ — perpendicular constraint | `native/cad-core/src/core/sketch_feature.cpp` | `set_sketch_perpendicular_constraint`, `drive_line_perpendicular_to_reference`, `enforce_perpendicular_relations` |
| C++ — constraint badges | `native/cad-core/src/core/viewport.cpp` | `make_line_constraint_primitive`, `relation_constraint_line_ids` suppression |
| UI — constraint toolbar | `apps/desktop-ui/src/layout/header/SketchToolbar.tsx` | Constraint arm/disarm buttons |
| UI — constraint pick handler | `apps/desktop-ui/src/App.tsx` | `handleSketchConstraintPointPick`, `handleSketchConstraintLinePick` |

## Known Gaps

| Gap | Severity | Notes |
|---|---|---|
| No constraint relaxation on coincident snap | Medium | User must manually delete H/V constraint before dragging to a different elevation |
| No visual constraint preview during drag | Low | Snap label shows, but no constraint badge |
| Circle center points not draggable | Low | `kind === "center"` is blocked at the UI layer; C++ has the path but it's untested |
| Equal-length + H/V interaction not tested | Medium | A line with both an equal-length relation and H/V constraint may behave unexpectedly |
| Multi-select endpoint drag (two lines sharing an endpoint) | Low | Only the first-matched line rubber-bands; the connected line updates only after commit |
| Perpendicular + H/V partial-failure bug | High | Relation persisted before enforcement validates. Error in UI but constraint partially applies. See Constraint Interoperability section. |
| Badge stacking — only one constraint badge renders per line | Medium | `relation_constraint_line_ids` suppresses H/V badge when relations exist. Multiple active constraints on one line are invisible. |