# Active Task: Dimension System — Consolidation & Bug Fixes

> **Branch:** `dimensions-2`
> **Date:** 2026-06-24

## Status

### ✅ Completed (2026-06-28)

**circle_radius & polygon_radius — idempotent on duplicate:**
- `add_sketch_circle_radius_dimension`: instead of throwing when dimension already exists,
  silently updates the value to match current geometry (mirrors `line_length` behaviour)
- `add_sketch_polygon_radius_dimension`: same idempotent treatment

**Constraint driven detection — wired for H/V constraints:**
- `set_sketch_line_constraint`: after applying an H/V constraint and running the solver,
  checks `solver_conflicting_count`, `solver_redundant_count`, `solver_dofs < 0`.
  If over-constrained, sets `constraint_driven = true` → TS viewport renders `(H)`/`(V)`.
- `clear_sketch_line_constraints`: resets `constraint_driven = false` on clear.
- The `constraint_driven` field already existed on `SketchLine`, was already emitted to TS
  primitives, and was already checked in `count_driving_on_point_pair` — only the assignment
  was missing.

**Point-pair fallback for distance dimensions:**
- `add_sketch_distance_dimension`: now passes point pairs to `finalize_new_dimension`:
  - `line_line_distance` → `start_point_id` from each line
  - `circle_center_distance` → `"point-circle-{id}-center"` for each circle
- This lets `finalize_new_dimension`'s step 2 (>2 constraints on 2 DOF pair) catch
  over-constraint cases the solver might miss.

### ✅ Completed This Session (prior)

**Driven detection — unified architecture:**
- `finalize_new_dimension()` — single shared function called by all 7 creation commands (`dimensions.inc`)
- `count_driving_on_point_pair()` — counts dimensions + H/V constraints on a point pair
- Old heuristic removed from `state_and_create.inc` (was conflicting with per-command checks)

**Dimension rendering — unified primitives:**
- `make_offset_dimension_primitive()` — shared helper for all linear dimension types
  (line_length, point_distance, line_line_distance, circle_center_distance, circle_line_distance)
  Each type now ~15 lines instead of ~70.
- `make_point_distance_dimension_primitive`: Euclidean gets default perpendicular offset
  (fixes missing leaders/arrows on freshly-created point-to-point dimensions)

**Driven display:**
- TS `buildSketchDimensionObject`: strips C++ parens, re-wraps based on TS `driven` flag
- `sync_driven_dimensions` respects `display_as` for point_distance (x/y vs Euclidean)
- Constraint `driven` plumbing in place (C++ struct, IPC, TS types, rendering for `(V)`/`(H)`)

**Picking fixes (`dimensionToolPicking.ts`):**
- `computePointDistanceAxis`: won't pick axis that gives zero distance
- Point-to-point on same line with existing dim → selects existing instead of creating redundant
- Linear mode with existing dim → selects instead of duplicating

### ⚠️ Remaining Gap

| Dimension type | Issue |
|---|---|
| **`angle` (2 lines)** | Creates with `is_auto=true`, never calls `refresh_sketch_derived_state` / `finalize_new_dimension`. Throws on duplicate instead of auto→manual conversion. **PARKED for later.** |

## Structural Weak Points (for future robustness)

To avoid regressions when adding features:

1. **TS picking dispatch** (`dimensionToolPicking.ts`): `dimensionEntityPickAction` and
   `dimensionPointPickAction` have intertwined logic. A clean state machine would help.

2. **Dual placement flows**: linear placement (deferred, with preview) vs immediate
   placement (create-then-drag). Unifying them would reduce duplicated pointer-handling.

3. **Template for new dimension types**: now that `finalize_new_dimension` exists,
   adding a new type requires:
   - C++: validate inputs, create dimension entry, call `finalize_new_dimension(id, [optional pointPair])`
   - TS: picking logic, placement, IPC command
   - If it constrains a point pair, the fallback is automatic via `count_driving_on_point_pair`

## Key Files Changed (2026-06-28)

- `native/cad-core/src/core/sketch/impl/sketch_dimension_create_commands.inc` — circle_radius + polygon_radius idempotent
- `native/cad-core/src/core/sketch/impl/sketch_axis_constraint_commands.inc` — constraint_driven detection
- `native/cad-core/src/core/sketch/impl/sketch_constraint_clear_commands.inc` — reset constraint_driven on clear
- `native/cad-core/src/core/sketch/impl/dimension_distance_commands.inc` — point-pair fallback

## Next Session

1. Fix `angle` (2 lines): add `refresh_sketch_derived_state` call, add solver check + auto→manual conversion
   - File: `native/cad-core/src/core/sketch/impl/dimension_angle_commands.inc`
