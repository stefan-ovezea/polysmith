# Snap Engine Fix Plan

Status: **Not started** — plan only. All session changes reverted.

## Root Cause

The `resolve_snap()` function in `snap_engine.cpp` collects candidates from
fundamentally incompatible categories into one list and ranks them by a single
numeric distance:

| Category | Snap kinds | Distance meaning |
|---|---|---|
| Discrete points | endpoint, midpoint, center, quadrant, intersection | Euclidean distance from cursor to the fixed point |
| Direction-constrained | axis_lock, parallel, perpendicular_direction, polar | Perpendicular offset from a constraint line (can be near-zero regardless of cursor position) |
| Continuous | nearest, perpendicular foot, tangent, grid | Projection distance from cursor to geometry |

A direction snap can have `distance = 0.01` while an endpoint has `distance = 2.0`.
Ranking them together means direction snaps always win — every snap says "Parallel"
or "Horizontal" regardless of where the cursor is.

## The Fix: Separate Ranking Per Category

Each category must be ranked independently. Categories have absolute priority
over each other:

```
Priority: Discrete > Direction > Continuous
```

Within each category, the closest candidate wins.

## Step-by-Step Implementation

### Step 1: Revert everything, keep one fix

Revert to clean state, but keep the perpendicular-foot clamping fix
(`snap_engine.cpp:420` — remove `t = clamp(t, 0, 1)`). This is a genuine bug:
when the cursor projection falls outside the target line segment, clamping
produces a non-perpendicular point.

Files to keep:
- `snap_engine.cpp` — only the perpendicular clamping removal

### Step 2: Refactor `resolve_snap` to support category-based ranking

Add an enum or a mode parameter that controls which categories are active:

```cpp
enum class SnapCategory {
    Discrete,     // endpoint, midpoint, center, quadrant, intersection
    Direction,    // axis_lock, perpendicular_direction, parallel, polar
    Continuous    // nearest, perpendicular_foot, tangent, grid, grid_line
};
```

Or simpler: split `resolve_snap` into three separate collection functions that
can be called independently:

```cpp
std::optional<SnapCandidate> resolve_discrete_snaps(cursor, sketch, filter, tolerance);
std::optional<SnapCandidate> resolve_direction_snaps(cursor, sketch, filter, tolerance, start_x, start_y);
std::optional<SnapCandidate> resolve_continuous_snaps(cursor, sketch, filter, tolerance, start_x, start_y);
```

Each returns the single best candidate within its category by distance only
(no cross-category comparison).

### Step 3: Wire the three-pass system in `drag_sketch_point`

Replace `resolve_snap()` call with sequential calls to the three category
functions:

```cpp
auto snap = resolve_discrete_snaps(cursor_x, cursor_y, params, filter, 2.0);
if (!snap) {
    snap = resolve_direction_snaps(cursor_x, cursor_y, params, filter, 2.0, start_x, start_y);
}
if (!snap) {
    snap = resolve_continuous_snaps(cursor_x, cursor_y, params, filter, 4.0, start_x, start_y);
}
```

### Step 4: Test discrete snaps only

Before adding anything else, verify that endpoint, midpoint, and center snaps
work correctly during endpoint drag:

- [ ] Endpoint snap works on line ends
- [ ] Midpoint snap works at line midpoint (not stolen by endpoint)
- [ ] Center snap works on circle/arc centers
- [ ] Info panel shows correct snap label
- [ ] Badge appears near cursor (from Step 5 below)

### Step 5: Add floating badge (UI only, no snap logic changes)

Wire `constraintPreview` during endpoint drag. This was already done in the
session — keep the ViewportPanel.tsx changes:

- `dragCursorRef` — tracks cursor canvas position during drag
- `snapKindToBadgeKind()` — maps snap kind to badge glyph
- `onDragSnap` handler sets `constraintPreview`
- Badge render supports: ● (endpoint), M (midpoint), ⊥ (perpendicular), H/V, T, ∥, /

### Step 6: Add direction snaps

Once discrete snaps are solid, add direction snaps as pass 2:

- [ ] axis_lock (horizontal/vertical) works when dragging H/V from start
- [ ] perpendicular_direction works when dragging ⊥ to another line
- [ ] parallel works when dragging ∥ to another line
- [ ] polar works at configured angle increments
- [ ] Direction snaps don't fire when discrete snap is closer
- [ ] Direction snaps don't fire at every cursor position

### Step 7: Add continuous snaps (pass 3)

Last priority — only fire when nothing else matches:

- [ ] nearest (line body) — project cursor onto nearest line segment
- [ ] tangent — from cursor to circle edge
- [ ] grid — round to grid intersection
- [ ] grid_line — lock to grid axis

### Step 8: Wire `resolve_draft_snap` (line drafting)

Apply the same category-based ranking to the line drafting path (`app.cpp`).
This path already has correct behavior for axis_lock with tight tolerance (0.5),
but should use the same category functions for consistency.

### Step 9: Add `snap_kind` to `DragPointResult`

Add `snap_kind` and `host_param_t` to the drag response so the UI can distinguish
snap types. This was already done in the session — keep those changes:

- `document.h`: add `snap_kind`, `host_param_t` to `DragPointResult`
- `document.cpp`: populate them
- `app.cpp`: serialize them in `drag_snap_result`
- `ipc.ts`, `ipcSchema.ts`: add to types/schema

### Step 10: Enable all snaps by default

Flip `snap_quadrant`, `snap_perpendicular`, `snap_parallel`, `snap_grid_line`,
`snap_polar` from `false` to `true` in both:

- `feature.h` — C++ struct defaults
- `SelectionFilterPanel.tsx` — UI panel defaults

## Key Principles

1. **Never compare distances across categories.** A direction snap's angular
   deviation is not comparable to a discrete snap's Euclidean distance.

2. **Discrete always beats direction.** If the cursor is near a line endpoint,
   snap to the endpoint — not to "parallel" just because the drag happens to
   be roughly parallel.

3. **Direction always beats continuous.** If the user is dragging horizontally,
   lock to horizontal — don't snap to the nearest point on some line body.

4. **Test one category at a time.** Don't add direction snaps until discrete
   snaps are verified. Don't add continuous until direction snaps are verified.

5. **The perpendicular foot clamping bug is real and separate.** Removing the
   `t = clamp(t, 0, 1)` is correct regardless of the ranking system — a
   perpendicular snap should always produce a 90° angle.
