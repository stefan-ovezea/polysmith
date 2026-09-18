# Draft Dimension Visualization — Implementation Plan

> **Status as of 2026-05-25:** Fully implemented and verified. See
> [Implementation Log](Implementation-Log) for the session summary.
>
> **Update 2026-09-18 (sketch/dimmensions):** focus/select happens once
> per session instead of per pointer move (no more focus flapping);
> ellipses get radiusX / radiusY preview boxes (UI-only — no core
> ellipse dimension). The rectangle length box anchors at the far edge
> midpoint, never at the cursor. **Arc** has TWO editable HTML badges:
> the length badge (stage 1: aim distance between the ends; stage 2:
> the chord) anchored at the first click, and the **radius badge from
> stage 2 on** (circumradius for three_point, center distance for
> center_start_end) anchored at the arc center. Typed values reshape
> the live draft (typed radius moves the apex onto the typed
> circumcircle, or rescales the center_start_end arc) and commit; the
> first keystroke over the live readout replaces it. Enter at stage 1
> places the second end at the typed distance, at stage 2 commits the
> arc. The old non-editable scene chord dimension (sprite label) was
> removed; a real radial scene dimension for arc stage 2 is deferred.
> Circle/rectangle/polygon previews still use HTML-only positioning
> (future work).

## Problem

When drafting a sketch entity (line, rectangle, circle), two floating HTML
`<input>` boxes show the current angle and length values. These are plain
text boxes positioned at screen coordinates. They overlap when the geometry
is tiny, have no dimension lines/arrows/arcs, and the angle input sits near
the narrow part of the angle where it's unreadable.

## Approach

Replace the HTML-only floating boxes with **scene-rendered dimension
geometry** (Three.js lines + arrows + arcs) — the same visual language as
committed sketch dimensions — with the HTML `<input>` sitting on top of the
dimension label's screen projection.

- **Length**: Linear dimension line parallel to the entity, arrows at both
  ends, extension lines from endpoints, label at midpoint. Offset opposite
  to the angle arc for visual separation.
- **Angle**: Arc centered at the line start point, sweeping from the
  reference angle (horizontal for first line, previous segment direction
  for chained lines) to the line direction. Dotted reference line from
  start along the reference angle. Dotted cursor extension from cursor
  outward.
- **Non-line tools** (rectangle, circle, polygon): fall back to the existing
  HTML-only positioning. **Arc** shows two HTML badges: the length badge
  anchored at the first click (aim distance at stage 1, chord between the
  ends at stage 2) and a radius badge from stage 2 on anchored at the arc
  center (circumradius for three_point, center distance for
  center_start_end); a radial scene dimension (leader from the center, one
  radial arrowhead at the arc — the committed arc radius dimension's visual
  language) is deferred. **Ellipse** shows two HTML boxes at the major/minor
  axis ends whose typed values reshape the live draft and commit.

## Implementation

All in `apps/desktop-ui/src/layout/ViewportPanel.tsx`:

### Architecture

- **`renderDraftDimensions()`** — Called from the animation `render()` loop
  every frame. Builds a disposable `THREE.Group` with all dimension geometry
  (length dimension lines/arrows, dotted extension, dotted reference line,
  angle arc + arrowheads). Screen positions stored in
  `draftDimScreenPositionsRef.current` for HTML input overlay placement.

- **`clearDraftDimGroup()`** — Traverses and disposes all geometry/materials
  from the previous frame's group. Called at the top of every
  `renderDraftDimensions()` invocation.

- **`draftFieldScreenPosition()`** — For the line tool, reads from
  `draftDimScreenPositionsRef` (render-loop-computed 3D positions).
  Falls through to a 2D fallback for first frame / non-line tools.
  Fallback clears stale positions on input change via `delete` to
  prevent one-frame label jumps.

### Single rendering path

The initial implementation had a duplicate rendering path in
`handlePointerMove` (reusable `draftDimSceneObjRef`) that fought with
`renderDraftDimensions()` — `clearDraftDimGroup()` destroyed the
reusable object every frame, `handlePointerMove` recreated it on
mouse move, creating a destroy/recreate cycle at 60 fps. This caused
flickering, GPU churn, and crashes when user input arrived mid-cycle.
The duplicate was removed; only `renderDraftDimensions()` draws
dimension geometry now.

### Formal specification — zoom formulas

All zoom-aware constants are computed once per frame from the
orthographic camera frustum and the viewport pixel height:

```
viewH    = (camera.top - camera.bottom) / camera.zoom
vpH      = renderer.domElement.height (pixels)
zoomDimOffset = max(4, 30 × viewH / vpH)   // ≈30 px on screen
zoomCap       = max(20, 480 × viewH / vpH) // ≈480 px on screen
```

### Length dimension

- **Offset direction**: `-perpDir` (flipped toward camera for 3D
  visibility). This is the opposite side from the angle arc.
- **Dimension line**: endpoints of the rubber band, offset by
  `-2 × zoomDimOffset × perpDir`. The `-2` doubles the offset (~60 px).
- **Extension lines**: from rubber-band start/end to dimension line
  start/end (perpendicular connector).
- **Arrowheads**: at both ends of the dimension line, oriented inward.
- **Label**: midpoint of the dimension line.

### Angle arc

- **Arc center**: the line's start point (pivot).
- **Arc radius**: `max(8, min(lineLen, zoomCap))`. The arc follows the
  rubber band up to the zoom cap; below 8 units the arc has a hard
  minimum so it stays readable for tiny lines.
- **Arc sweep**: `refAngle → angleRad` along the shorter angular path
  (normalised to [-π, π]). Natural CCW direction places the arc in the
  half-plane opposite to the length dimension.
- **Arrowheads**: at both sweep endpoints, orientation tangential to
  the arc.
- **Label**: midpoint of the arc.

### Reference line (angle)

- **Length**: `max(12, lineLen × 0.28)`. 28% of the rubber band,
  minimum 12 world units.
- **Direction**: from the line start along `refAngle`.
- **Style**: dashed, lower opacity than the solid dimension lines.

### Cursor extension

- **Length**: `max(12, lineLen × 0.35)`. 35% of the rubber band,
  minimum 12 world units.
- **Direction**: from cursor outward along the line direction.
- **Style**: dashed, matching the reference line.

### Angle reference (chained lines)

- `previousLineAngleRef` stores the 2D sketch angle of the last
  committed line segment (set on commit, cleared on chain break).
- First / independent line: reference = horizontal (0 rad).
- Chained line: reference = previous segment direction, so the arc
  shows the relative turn angle.

### Per-frame cleanup

Removed two diagnostic magenta lines that created new
`THREE.BufferGeometry` + `THREE.LineBasicMaterial` every frame with
100ms setTimeout cleanup — a memory/GPU leak at 60 fps.

## Remaining

- **Display units** in dimension label (currently hardcoded mm)
- **Perpendicular snap** integration (pre-existing backlog item)

## References

- `buildSketchDimensionObject()` in `viewport.utils.ts` — existing committed
  dimension rendering (arcs, arrows, labels), used as pattern reference
- [Contextual Modeling Workflow](Contextual-Modeling-Workflow) — binding UX
  pattern
- [Implementation Log](Implementation-Log) — 2026-05-25 session summary
