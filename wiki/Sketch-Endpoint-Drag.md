# Sketch Endpoint Drag

> **Status as of 2026-05-29:** Basic endpoint drag shipped. Line rubber-bands
> during drag with snap resolution. H/V constraints enforce axis-aligned
> stretch (industry-standard). Constraint relaxation (auto-delete conflicting
> constraints on coincident snap) is planned but not yet implemented.

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

## Known Gaps

| Gap | Severity | Notes |
|---|---|---|
| No constraint relaxation on coincident snap | Medium | User must manually delete H/V constraint before dragging to a different elevation |
| No visual constraint preview during drag | Low | Snap label shows, but no constraint badge |
| Circle center points not draggable | Low | `kind === "center"` is blocked at the UI layer; C++ has the path but it's untested |
| Equal-length + H/V interaction not tested | Medium | A line with both an equal-length relation and H/V constraint may behave unexpectedly |
| Multi-select endpoint drag (two lines sharing an endpoint) | Low | Only the first-matched line rubber-bands; the connected line updates only after commit |
