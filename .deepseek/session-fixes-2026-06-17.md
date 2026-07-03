# Session Fixes — 2026-06-17

## Build System

### Eigen3 not found
- **File:** vcpkg
- **Fix:** `vcpkg install eigen3 --triplet x64-windows`
- **Root cause:** Eigen3 was declared as a dependency but never installed in the vcpkg tree.

### Boost find_package broken on CMake 3.30+
- **File:** `native/cad-core/CMakeLists.txt:21`
- **Fix:** Changed `find_package(Boost REQUIRED)` to a fallback pattern — `CONFIG QUIET` first (needed for CMake 3.30+ policy CMP0167 and vcpkg), `MODULE REQUIRED` fallback (old distros without BoostConfig.cmake).
- **Root cause:** CMake 3.30 removed the built-in `FindBoost` module; vcpkg provides `BoostConfig.cmake` but the `find_package` wrapper wasn't routing to it correctly.

### planegcs submodule empty
- **File:** `.gitmodules`
- **Fix:** `git submodule update --init third_party/planegcs`
- **Root cause:** Submodule was registered but never cloned.

### @salusoft89/planegcs npm package missing
- **File:** `apps/desktop-ui/package.json`
- **Fix:** `pnpm install`
- **Root cause:** Package was in lockfile but never materialized in `node_modules`.

---

## Dimension Tool Regression Fixes (from `.deepseek/todos.md`)

All regressions stemmed from the virtual-pivot feature (angle dimensions between non-touching lines) which wrapped existing code in new `if/else` blocks via sed edits, introducing brace nesting errors and scoping bugs.

### HIGH: Angle constraint not enforced
- **File:** `native/cad-core/src/core/sketch/impl/constraint_solver_dimension_constraints.inc`
- **Fix:** Removed `} else {` wrapper around the original shared-endpoint code. Virtual pivot is now an early `continue` path. The original code runs un-nested for normal angle dimensions.
- **Root cause:** The `else` wrapper was fragile — the virtual pivot block called `addConstraintL2LAngle` then had a comment "Fall through to next dimension" but no actual `continue`, relying on structural luck to skip the else block.

### HIGH: Angle update — sed artifacts
- **File:** `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc:168-185`
- **Fix:** Replaced mixed tab/space indentation with consistent spaces in the virtual-pivot rotation block.
- **Root cause:** Sed replacements introduced mixed whitespace, making the code unreadable and prone to future editing errors.

### HIGH: Angle rendering (cosmetic)
- **File:** `native/cad-core/src/core/viewport/impl/sketch_angle_dimension_primitive.inc`
- **Fix:** None needed — indentation of the else body is misleading but C++ ignores indentation. Brace structure verified correct.
- **Root cause:** Wrapping existing code in `} else {` without re-indenting the body.

### MED: Double-click same line no longer clears staged pick
- **File:** `apps/desktop-ui/src/layout/viewport/dimensionToolPicking.ts:392-398`
- **Fix:** Added same-entity guard: if `context.hit.id === stagedFirstId`, clear the pick instead of calling `createAngleOrDistance`.
- **Root cause:** The `stagedFirstId != null` early-return block bypasses `handleDimensionStagedEntity`, which handles the re-click-to-clear case.

### MED: Point-to-point on same line broken
- **File:** `apps/desktop-ui/src/layout/viewport/dimensionToolPicking.ts:404-426`
- **Fix:** When both `stagedFirstId` and `getFirstPoint()` are set (point was staged which also staged its entity), fall through to `handleDimensionPointHit` instead of intercepting with `createPointDistance(entityId, pointId)`.
- **Root cause:** The early-return block was calling `createPointDistance` with an entity ID instead of a point ID for same-line endpoint clicks.

### MED: Angle preview for unrelated lines
- **File:** `apps/desktop-ui/src/layout/viewport/dimensionRelationPreviewGeometry.ts`
- **Fix:** Removed infinite-line intersection computation from `sharedLineEndpoint()` (which affected ALL callers including `sketchLinesShareEndpoint`). Moved virtual-pivot computation into `createLineAnglePreview()` where it's only used for explicitly selected line pairs.
- **Root cause:** `sharedLineEndpoint` was returning virtual pivots for ANY non-parallel lines, flooding the preview system.

---

## Angle Value Sync After Drag

### Auto angle dimensions never refreshed after geometry changes
- **Files:**
  - `native/cad-core/src/core/sketch/impl/private_dimension_sync_helpers.inc`
  - `native/cad-core/src/core/sketch/impl/state_and_create.inc`
- **Fix:** 
  1. Removed `if (dim.is_auto && dim.expression.empty()) continue;` from `sync_angle_dimensions` — auto dims need syncing too
  2. Added virtual-pivot support to `sync_angle_dimensions` using stored `pivot_x`/`pivot_y`
  3. Added `sync_angle_dimensions()` call to `refresh_sketch_derived_state` after `sync_all_line_dimensions`
- **Root cause:** `sync_angle_dimensions` existed but was never called, and even if called, it skipped auto dimensions. Auto angle values were stale after any geometry change.

---

## UI/UX Fixes

### Dimension label click target too small
- **File:** `apps/desktop-ui/src/utils/viewport/sketchObjects.ts:460-461`
- **Fix:** Increased canvas padding from `textWidth + 12` × `38` to `textWidth + 32` × `50`.
- **Root cause:** Only 6px horizontal padding per side; tight click area especially at moderate zoom.

### Label raycasting lost to overlapping arc geometry
- **File:** `apps/desktop-ui/src/layout/viewport/screenSpaceSketchSprites.ts:175-183`
- **Fix:** Offset label sprite 3 screen-pixels toward the camera (`-worldUnitsPerPixel * 3`) so it always wins raycasting against arc geometry at the same world position.
- **Root cause:** Angle dimension labels sit at the bisector, same radius as the arc. `raycaster.intersectObjects` returns whichever is closer to the camera — inconsistent between label and arc.

### Angle arc blocks vertex dragging
- **File:** `apps/desktop-ui/src/layout/viewport/sceneTargetPicking.ts:226-275`
- **Fix:** Restructured `pickActiveSketchTarget` so sphere-based point picking always runs first, regardless of active tool. Previously, the select tool checked dimensions before points, so the angle arc at a shared vertex won every time.
- **Root cause:** Picking order was: dimensions → constraints → points. The angle arc overlaps the shared vertex; clicking it hit the arc instead of the point.

### Escape during angle edit throws "must be greater than zero"
- **File:** `native/cad-core/src/core/sketch/impl/dimension_update.inc:11-20`
- **Fix:** Skip the `value <= 0.001` guard for `angle` and `line_angle` kinds. Moved the dimension lookup before the guard so `dimension.kind` is available.
- **Root cause:** Angle values are signed radians (can be negative or near-zero). The blanket `> 0` check at the top of `update_sketch_dimension` rejected valid angle values when Escape triggered a restore-to-original.

---

## Files Changed

| File | Change |
|---|---|
| `native/cad-core/CMakeLists.txt` | Boost CONFIG/MODULE fallback |
| `native/cad-core/src/core/sketch/impl/constraint_solver_dimension_constraints.inc` | Remove else wrapper, add continue |
| `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc` | Fix sed artifacts (tabs→spaces) |
| `native/cad-core/src/core/sketch/impl/dimension_update.inc` | Skip >0 check for angles |
| `native/cad-core/src/core/sketch/impl/private_dimension_sync_helpers.inc` | Fix sync_angle_dimensions |
| `native/cad-core/src/core/sketch/impl/state_and_create.inc` | Call sync_angle_dimensions |
| `apps/desktop-ui/src/layout/viewport/dimensionToolPicking.ts` | Double-click clear + point-to-point |
| `apps/desktop-ui/src/layout/viewport/dimensionRelationPreviewGeometry.ts` | Move virtual pivot to createLineAnglePreview |
| `apps/desktop-ui/src/layout/viewport/sceneTargetPicking.ts` | Points-first picking order |
| `apps/desktop-ui/src/layout/viewport/screenSpaceSketchSprites.ts` | Camera-facing label offset |
| `apps/desktop-ui/src/utils/viewport/sketchObjects.ts` | Larger label canvas |
| `vcpkg` | Install eigen3 |
| `.gitmodules` (planegcs submodule) | Initialized |
| `node_modules` | pnpm install |

---

## Virtual-Pivot Angle Dimension (crossing/non-touching lines)

### Stale pivot — reference line drifts after drag
- **Files:**
  - `native/cad-core/src/core/sketch/impl/private_dimension_sync_helpers.inc`
  - `native/cad-core/src/core/sketch/impl/constraint_solver_dimension_constraints.inc`
  - `native/cad-core/src/core/viewport/impl/sketch_angle_dimension_primitive.inc`
  - `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc`
- **Fix:** Recompute virtual pivot from current line positions in all 4 consumers (sync, solver, rendering, update). Stored pivot is a serialization artifact only; live geometry drives computations.
- **Root cause:** Pivot computed once at creation, never updated. Lines moved, pivot stayed stale.

### Crossing point drifts when editing angle value
- **File:** `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc`
- **Fix:** Rotate line B around the virtual pivot (not its near endpoint). Both endpoints translate together, keeping the infinite-line intersection fixed.
- **Root cause:** Rotation center was B's near endpoint; crossing point drifted.

### Lines bound together after 180° round-trip (snap merges point IDs)
- **File:** `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc`
- **Fix:** Removed `snap_line_endpoints_to_coincident_geometry` call from the virtual-pivot rotation block. Independent crossing lines must not have their endpoints merged.
- **Root cause:** At 180° lines become collinear; snap merged B's endpoints with A's, binding them. Subsequent edits rotated both lines together.

### Collinear lines: sync overwrites signed angle to 0 instead of ±π
- **File:** `native/cad-core/src/core/sketch/impl/private_dimension_sync_helpers.inc`
- **Fix:** When `cross ≈ 0` for a virtual-pivot dimension, skip the geometry read-back and preserve the stored signed angle.
- **Root cause:** Both "away-from-pivot" directions collapse to the same unit vector; `atan2` reports 0.

### Collinear lines: update uses wrong reference angle
- **File:** `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc`
- **Fix:** When `cross ≈ 0`, use `dimension.value` as `current_signed_for_value` instead of the geometric read-back.
- **Root cause:** Same as above — geometric measurement is ambiguous for collinear lines.

### Shared-endpoint angle measured from horizontal (choose_host_ray fallback)
- **Files:**
  - `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc`
  - `native/cad-core/src/core/sketch/impl/dimension_angle_commands.inc`
- **Fix:** In `choose_host_ray` / `dir_toward_smaller_angle`, skip any candidate endpoint that IS the pivot itself. The zero-length fallback `(1,0)` can accidentally win scoring.
- **Root cause:** Pre-existing bug exposed by pivot recomputation changes.

### Tolerance alignment (creation vs update vs rendering)
- **Files:**
  - `native/cad-core/src/core/sketch/impl/dimension_angle_update.inc` (0.01→0.10)
  - `native/cad-core/src/core/viewport/impl/sketch_angle_dimension_primitive.inc` (0.05→0.10)
- **Fix:** Aligned endpoint-on-segment tolerance to 0.10 in all three locations.

### Virtual-pivot preview: no quadrant selection
- **File:** `apps/desktop-ui/src/layout/viewport/dimensionRelationPreviewGeometry.ts`
- **Fix:** For the virtual-pivot case (`!firstEndpointAtPivot && !secondEndpointAtPivot`), generate 4 candidates (all pairings of both lines' endpoint directions) so cursor hover can select acute/obtuse/reflex angles.
- **Root cause:** Only 1 candidate was generated; cursor could only flip to reflex.
