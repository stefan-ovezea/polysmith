# Plan: Missing Dimension Types — LibreCAD Gap Analysis

## Context

Compared Polysmith's dimension capabilities against LibreCAD (a mature 2D
CAD). Three dimension types are missing that matter for a parametric
sketcher. Drawing-sheet types (ordinate, leader, baseline, chain) are
deferred until a Drawing workspace exists.

## Gaps & Priority

### 1. Arc Length Dimension (highest value, moderate effort)

**What:** Dimension showing the length along an arc (`radius × sweep_angle`).
Allows constraining an arc's sweep while keeping its radius or vice versa.

**Current state:** `SketchArcEntry` exists with full geometry (center, radius,
start/end points, ccw). The dropdown lists `arc_length` mode. Picking code
explicitly skips arcs ("Arcs don't support dimensions yet").

**What needs to happen:**
- New dimension kind: `"arc_length"` (C++ core + TS types)
- New IPC command: `add_sketch_arc_length_dimension` (C++ handler + TS factory)
- Solver constraint: use `p2p_angle` on the arc's center-to-start and
  center-to-end vectors, or a new `arc_length` constraint
- Viewport primitive: render an arc-length dimension (arc with arrows at
  endpoints, label along the arc)
- UI picking: un-ignore arcs in `handleDimensionEntityHit`
- Preview: show arc length preview when hovering an arc in the dimension tool

### 2. Horizontal / Vertical Point Distance (high value, moderate effort)

**What:** Dimension between two points projected onto the X or Y axis.
Essential for locating features ("this hole is 50mm to the right of this
edge", not "50mm away in any direction").

**Current state:** `point_distance` exists but gives Euclidean distance only.
No horizontal or vertical projection.

**What needs to happen:**
- New dimension kinds: `"point_distance_horizontal"` and
  `"point_distance_vertical"` (or a single kind with an axis field)
- New IPC commands: `add_sketch_point_distance_dimension` extended with an
  optional `axis` field (`"x"`, `"y"`, or absent for Euclidean)
- Solver constraint: `p2p_distance_x` / `p2p_distance_y` (planegcs supports
  these — FreeCAD's solver has horizontal/vertical distance constraints)
- Rendering: dimension line remains horizontal or vertical regardless of
  point positions (like a linear dimension with angle 0° or 90°)
- UI: auto-detect horizontal vs vertical based on cursor position when
  placing the dimension (if cursor is closer to horizontal from the points,
  create horizontal; if closer to vertical, create vertical)

### 3. Aligned Dimension (already covered)

**What:** Dimension between two points, parallel to the line connecting them.

**Current state:** `point_distance` already measures the Euclidean distance
between two points. The dimension line is drawn parallel to the point-to-point
direction. This IS an aligned dimension. No work needed — just document it
as such.

## Non-Goals (Drawing-Sheet Types)

These are documentation/annotation features for a future Drawing workspace.
They don't drive geometry and aren't needed in the sketcher:

- **Ordinate** — reference dimension from a common baseline
- **Leader** — annotation leader line with text
- **Baseline / Chain** — dimensioning systems for groups of features
- **Tidy up / Arrange** — automatic dimension layout
- **Flip arrows** — toggle arrow direction
- **Match** — copy dimension style
- **Dimension break** — break dimension lines where they cross

## Order of Implementation

### Phase A: Arc Length Dimension

1. **C++ dimension kind + solver** — Add `"arc_length"` to `SketchDimension`,
   implement `add_sketch_arc_length_dimension()` following the
   `add_sketch_circle_radius_dimension` pattern. Arc length =
   `radius × abs(sweep_angle)`. Use `p2p_angle` constraint on the center→start
   and center→end vectors.

2. **C++ IPC** — Wire command through document manager and IPC handler.

3. **TS types + factory** — Add command type, factory function, wire through
   ref chain following `addSketchCircleRadiusDimension` pattern.

4. **UI picking** — Remove the arc skip in `handleDimensionEntityHit`. Add
   `createDimensionArc` to `dimensionToolActions`.

5. **Viewport rendering** — Add arc-length dimension primitive
   (likely in `sketch_line_dimension_primitives.inc` or a new file).

6. **Preview** — Show arc length preview when hovering an arc.

### Phase B: Horizontal / Vertical Point Distance

1. **C++ dimension kinds + solver** — Add `"point_distance_h"` and
   `"point_distance_v"` kinds. Extend `add_sketch_point_distance_dimension`
   with an optional axis parameter. Use planegcs `p2p_distance_x` /
   `p2p_distance_y` constraints.

2. **C++ IPC** — Extend existing command with optional `axis` field.

3. **TS types + factory** — Extend `AddSketchPointDistanceDimensionCommand`
   with optional `axis`. Update factory.

4. **UI** — When creating a point-to-point dimension, detect whether the
   cursor is closer to horizontal or vertical from the two points.
   Auto-select the appropriate constraint type.

5. **Rendering** — Horizontal/vertical dimension lines are always axis-aligned
   (not connecting the two points directly). Similar to how `line_angle` has
   a reference line — the dimension line runs along the axis.

6. **Preview** — Show the projected distance based on cursor position.

## Key Risks

- **Planegcs solver support** — Need to verify that planegcs (the 2D
  constraint solver) supports horizontal/vertical distance constraints
  (`p2p_distance_x`, `p2p_distance_y`). If not, they'd need to be added to
  the solver first (higher effort).

- **Arc freeze** — The v1 arc implementation freezes shape at creation
  (points stored with `is_fixed=true`). An arc-length dimension would need
  to unfreeze and drive the arc geometry through the solver.

- **Angular / linear mode filter** — The new dimension types should be
  filterable by mode. Arc length → `arc_length` mode. Horizontal/vertical
  distance → `linear` mode or a new sub-mode.
