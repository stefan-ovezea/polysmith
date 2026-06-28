# Active Task: Dimension System — Consolidation & Bug Fixes

> **Branch:** `dimensions-2`
> **Date:** 2026-06-24

## Status

### ✅ Completed This Session

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

### ⚠️ Remaining Gaps

| Dimension type | Solver check | Type-specific fallback | Auto→manual conversion |
|---|---|---|---|
| `line_length` | ✅ | ✅ point-pair + H/V | ✅ |
| `point_distance` | ✅ | ✅ point-pair + H/V | N/A |
| `line_angle` | ✅ | ❌ none | ✅ |
| `circle_radius` | ✅ | ❌ none | ❌ (throws if exists) |
| `polygon_radius` | ✅ | ❌ none | ❌ (throws if exists) |
| **`angle` (2 lines)** | **❌ no refresh** | **❌ none** | **❌ always `is_auto=true`** |
| `line_line_distance` | ✅ | ❌ (could use point-pair) | N/A |
| `circle_center_distance` | ✅ | ❌ (could use point-pair) | N/A |
| `circle_line_distance` | ✅ | ❌ none | N/A |

**`angle` (2 lines) is the most broken** — `add_sketch_angle_dimension` creates with
`is_auto=true`, never calls `refresh_sketch_derived_state`, never checks solver.
Created and forgotten.

**Constraint driven marking** — plumbing exists (`constraint_driven` on `SketchLine`,
`driven` on primitive, TS rendering) but nothing sets it. The old heuristic that set
it was removed from `state_and_create.inc`.

**Placement flow** — point_distance creates as Euclidean only; no drag-to-choose axis
like linear placement has for lines.

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

## Key Files

### C++
- `native/cad-core/src/core/sketch/impl/dimensions.inc` — `finalize_new_dimension`, `count_driving_on_point_pair`
- `native/cad-core/src/core/sketch/impl/sketch_dimension_create_commands.inc` — all creation commands
- `native/cad-core/src/core/sketch/impl/state_and_create.inc` — `refresh_sketch_derived_state` (heuristic removed)
- `native/cad-core/src/core/sketch/impl/private_dimension_relation_sync.inc` — `sync_driven_dimensions`
- `native/cad-core/src/core/viewport/impl/sketch_line_dimension_primitives.inc` — `make_offset_dimension_primitive`
- `native/cad-core/src/core/viewport/impl/sketch_line_point_distance_dimension_primitives.inc` — point_distance + line_line_distance
- `native/cad-core/src/core/viewport/impl/sketch_circle_distance_dimension_primitives.inc` — circle distance primitives
- `native/cad-core/src/core/sketch/impl/dimension_angle_commands.inc` — `add_sketch_angle_dimension` (broken)

### TypeScript
- `apps/desktop-ui/src/layout/viewport/dimensionToolPicking.ts` — picking, axis fix, duplicate blocking
- `apps/desktop-ui/src/utils/viewport/sketchObjects.ts` — driven rendering for dims + constraints
- `apps/desktop-ui/src/lib/viewportScene.ts` — `makeSketchConstraint`
- `apps/desktop-ui/src/lib/schemas/ipc/viewportStateSchema.ts` — constraint `driven` schema
- `apps/desktop-ui/src/types/viewport.ts`, `scene.ts` — TS types

## Next Session

1. Fix `angle` (2 lines): add `refresh_sketch_derived_state` call, add solver check + auto→manual conversion
2. Add auto→manual conversion for `circle_radius`
3. Wire constraint driven detection (set `constraint_driven = true` when point-pair is over-constrained)
4. Add point-pair fallback for `line_line_distance` / `circle_center_distance`
