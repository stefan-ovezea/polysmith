# Active Task: Dimension System — Consolidation & Bug Fixes

> **Branch:** `dimensions-2`
> **Date:** 2026-06-24

## Status

### ✅ Completed (2026-07-01) — from other machine

**H/V point ordering fix — prevents geometry flips:**
- `constraint_solver_dimension_constraints.inc`: planegcs `ConstraintDifference::error()` is
  `param2 - param1 - difference`, so `addConstraintDifference(p1, p2, val)` enforces
  `p2 - p1 = val`.  Before this fix, H/V point_distance dims always set `val = dim.value`
  (absolute), but if the current geometry had the opposite sign (e.g. endPoint left of
  startPoint for a horizontal dim), the solver would flip the point ordering → cascading
  corruption through shared endpoints.  Now computes the signed difference from current
  solver-point positions and preserves its sign on the target value.

**Toggle Driving — right-click context menu on any dimension:**
- New IPC command `toggle_sketch_dimension_driven` toggles the `driven` flag and
  refreshes derived state.  Driven dimensions display in parentheses `(value)` and
  do not constrain the solver.  Works for all dimension kinds.
- TS: context menu gains "Toggle Driving" button; full IPC wiring through command
  factory, hook, props, and context menu actions.
- C++: `toggle_sketch_dimension_driven()` in `sketch_dimension_toggle_driven_command.inc`.

**Toggle Construction — right-click context menu on sketch lines:**
- Right-click a sketch line → "Toggle Construction" flips `is_construction`.
  Reads current state from the document (not stale `sketchLinesRef`).
- Construction is now purely visual / profile-exclusion (Fusion 360 behaviour):
  `set_sketch_line_construction` just flips the flag — no dimension auto-creation
  or auto-deletion.  Dimensions keep their driving/driven status regardless of
  the construction flag.
- Removed restrictions that blocked creating driving dimensions on construction
  lines, circles, and polygons.

### ✅ Completed (2026-06-28)

**circle_radius & polygon_radius — idempotent on duplicate:**
- `add_sketch_circle_radius_dimension`: instead of throwing when dimension already exists,
  silently updates the value to match current geometry (mirrors `line_length` behaviour)
- `add_sketch_polygon_radius_dimension`: same idempotent treatment

**Circle radius/diameter viewport toggle:**
- `make_circle_dimension_primitive`: checks `display_as` to build label with "⌀" (diameter)
  or "R" (radius) prefix, with correct value scaling.

**Constraint driven detection — wired for H/V constraints:**
- `set_sketch_line_constraint`: after applying an H/V constraint and running the solver,
  checks `solver_conflicting_count`, `solver_redundant_count`, `solver_dofs < 0`.
  If over-constrained, sets `constraint_driven = true` → TS viewport renders `(H)`/`(V)`.
- `clear_sketch_line_constraints`: resets `constraint_driven = false` on clear.

**Point-pair fallback for distance dimensions:**
- `add_sketch_distance_dimension`: now passes point pairs to `finalize_new_dimension`:
  - `line_line_distance` → `start_point_id` from each line
  - `circle_center_distance` → `"point-circle-{id}-center"` for each circle

**Angle ghost regression fix:**
- Auto-mode linear placement now checks for relation preview candidates (angle between
  two lines) during pointerMove; pointerUp commits relation if active instead of
  always committing linear placement.

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
| **Solver alignment** | WASM frontend uses configurable iter/tol (LOOSE: 20/1e-4, EXACT: 200/1e-10). C++ backend uses planegcs hardcoded defaults (100 iter × sketchSize, 1e-10 tol, DogLeg). Need to align or at minimum expose config on C++ side. **TODO.** |

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

## Key Files Changed (2026-07-01)

- `native/cad-core/src/core/sketch/impl/constraint_solver_dimension_constraints.inc` — H/V point ordering fix
- `native/cad-core/src/core/sketch/impl/sketch_dimension_toggle_driven_command.inc` — toggle driven (new file)
- `native/cad-core/src/core/sketch/impl/sketch_entity_dimension_commands.inc` — toggle driven document command
- `native/cad-core/src/app/impl/sketch_create_dimension_delete_display_command_handlers.inc` — toggle driven IPC dispatch
- `native/cad-core/src/core/sketch/impl/line_entity_commands.inc` — construction simplified (purely visual)
- `native/cad-core/src/core/sketch/impl/sketch_dimension_create_commands.inc` — removed construction restrictions
- `apps/desktop-ui/src/layout/viewport/ViewportContextMenu.tsx` — Toggle Driving + Toggle Construction buttons
- `apps/desktop-ui/src/layout/viewport/viewportContextMenuActions.ts` — toggleDriven + toggleConstruction actions
- `apps/desktop-ui/src/layout/viewport/ViewportPanelShell.tsx` — prop plumbing
- `apps/desktop-ui/src/layout/viewport/viewportPanelTypes.ts` — type additions
- `apps/desktop-ui/src/layout/viewport/contextMenuState.ts` — lineId in context menu state
- `apps/desktop-ui/src/types/viewport.ts` — lineId in ViewportContextMenuState
- `apps/desktop-ui/src/lib/ipc/sketchCommands.ts` — toggle_driven command factory
- `apps/desktop-ui/src/types/ipc/sketchCommands.ts` — toggle_driven command type
- `apps/desktop-ui/src/types/ipc.ts` — IPC command union
- `apps/desktop-ui/src/hooks/useCadCore.ts` — toggleSketchDimensionDriven hook
- `apps/desktop-ui/src/App.tsx` — toggleDriven + toggleConstruction wiring

## Key Files Changed (2026-06-28)

- `native/cad-core/src/core/sketch/impl/sketch_dimension_create_commands.inc` — circle_radius + polygon_radius idempotent
- `native/cad-core/src/core/sketch/impl/sketch_axis_constraint_commands.inc` — constraint_driven detection
- `native/cad-core/src/core/sketch/impl/sketch_constraint_clear_commands.inc` — reset constraint_driven on clear
- `native/cad-core/src/core/sketch/impl/dimension_distance_commands.inc` — point-pair fallback
- `native/cad-core/src/core/viewport/impl/sketch_circle_radius_dimension_primitives.inc` — radius/diameter label toggle
- `apps/desktop-ui/src/layout/ViewportPanel.tsx` — angle ghost regression + relation preview during linear placement

## Next Session

1. Fix `angle` (2 lines): add `refresh_sketch_derived_state` call, add solver check + auto→manual conversion
   - File: `native/cad-core/src/core/sketch/impl/dimension_angle_commands.inc`
