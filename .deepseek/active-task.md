# Active Task: GCS Freeze State Machine, Ripple-Freeze, DOF Feedback, Speculative Inferencing

**Started:** 2026-06-09
**Status:** P1 + P2 + P3.1 + P3.2 + P3.3 complete ✅ (C++ build clean, TS type-check clean, NOT yet tested in running app)
**Source:** `.deepseek/pastes/` — user-provided GCS architecture document
**Plan:** `wiki/GCS-Implementation-Strategy.md`

## Goal

Replace ad-hoc constraint behavior with a proper freeze state machine, add
solver-based snap inferencing via WASM, and surface DOF/conflict diagnostics
in the UI.

---

## Completed

### P1 — Append Mode Freeze State Machine (C++ Core)

Prevents "ghost movements" when adding new geometry or constraints to an
already-constrained sketch.

**New transient field** on `SketchFeatureParameters`:
- `pending_append_focus_ids: vector<string>` — set by mutators before calling
  refresh, cleared after the solver pass

**Freeze logic** in `refresh_sketch_derived_state` (`state_and_create.inc`):
1. When `pending_append_focus_ids` is non-empty, snapshot all `is_fixed` values
2. Freeze all points (`is_fixed = true`)
3. Unfreeze only points referenced by the focus entity IDs (line endpoints,
   circle centers)
4. Run solver — only focus entities can move
5. Restore original `is_fixed` values

**New helper:** `refresh_sketch_with_append_focus(feature, {ids...})`

**Mutators updated** (8 files) to use append mode:
- `line_entity_commands.inc` — new line creation (passes `{line.id}`)
- `curve_primitives.inc` — circle + arc creation
- `sketch_axis_constraint_commands.inc` — H/V constraint
- `sketch_parallel_constraint_commands.inc` — parallel relation (passes both
  line IDs)
- `sketch_perpendicular_constraint_commands.inc` — perpendicular relation
- `sketch_equal_length_constraint_commands.inc` — equal length
- `sketch_tangent_constraint_commands.inc` — tangent
- `line_anchor_commands.inc` — midpoint + point-line anchors

**Note:** Constraint-clear paths (removing constraints) still use normal
`refresh_sketch_derived_state` — no freeze needed when removing.

### P1 — Drag Ripple-Freeze (TS WASM)

Prevents unrelated geometry from shifting during endpoint drag.

**`planegcsBridge.ts:**
- `solve()` now accepts optional `opts?: { activePointIds?: string[] }`
- When `activePointIds` is set, all points NOT in the set are forced
  `fixed: true` regardless of stored `is_fixed`

**`endpointDrag.ts`:**
- New `computeRippleActivePoints(sketch, pointId)` — computes 1-hop connected
  set (dragged point + opposite endpoints of connected lines + circle centers)
- `resolveEndpointDragFrame()` now passes `activePointIds` to WASM solve

### P2 — Over-Constrained UI Feedback + DOF Visual Feedback

**C++ diagnostics fields** (on `SketchFeatureParameters` + `ViewportState`):
- `solver_conflicting_count: int` (-1 = no data)
- `solver_redundant_count: int` (-1 = no data)
- Populated in `state_and_create.inc` after each solver pass
- Serialized in `viewport_to_payload_state_scene_interaction_entries.inc`

**TS types/schema** updated to include the three new fields
(`solver_dofs`, `solver_conflicting_count`, `solver_redundant_count`)

**New component:** `SketchDofBadge.tsx`
- Reads viewport solver data from `useCadCoreStore`
- Renders next to SketchToolbar in `AppHeader.tsx`
- Amber `DOF: N` (under-constrained), Green `✓` (fully constrained),
  Red `⚠ N` (conflicting), Orange `~N` (redundant)
- Hidden when no solver data

### P3.1 — Speculative Inferencing Foundation

**New `INFERENCE` solver config** in `planegcsBridge.ts`:
- 5 iterations, 1e-3 tolerance, Levenberg-Marquardt

**New file:** `apps/desktop-ui/src/lib/speculativeSolve.ts`

Core function `speculativeSolve()`:
- Builds full sketch system in WASM (geometry + constraints + dimensions)
- Adds virtual draft line (draft_start → cursor)
- Adds speculative `temporary: true` constraint of requested type
- Runs INFERENCE solve
- Returns `{ position, converged, distance, solverStatus }`

Supports all 6 snap types: `horizontal_l`, `vertical_l`, `tangent_lc`,
`perpendicular_ll`, `parallel`, `point_on_line_pl`

### P3.2 — Dynamic Snaps Replaced with Speculative WASM

All 5 dynamic snap types now try the speculative WASM solver first,
falling back to hand-coded TS math when the bridge is unavailable or
the solve fails.

Strategy: fast TS proximity gating to find best candidate entity →
single speculative solve to refine position (only **1 WASM solve per
snap type per frame**)

New helper functions in `snapResolution.ts`:
- `speculativeAxisLockSnap` — H/V constraint on virtual line
- `speculativeTangentSnap` — tangent_lc constraint on best circle
- `speculativePerpendicularSnap` — perpendicular_ll on best line
- `speculativeParallelSnap` — parallel on best line
- `speculativeLineBodySnap` — point_on_line_pl on best line

`dynamicSnapCandidate()` signature extended with:
- `sketchParameters?: SketchFeatureParameters | null`
- `constraints?: SketchConstraintData[]`

`resolveDynamicSnap()` now passes `sketchParameters` through to
`dynamicSnapCandidate()`.

---

### P3.3 Fix — Angle Dimension Stealing + Geometry Deformation (2026-06-10)

**Two bugs, one commit (4405e23):**

1. **TS "stealing":** `dimensionToolPicking.ts:dimensionEntityPickAction()` always
   computed a `deleteDimensionId` for the first entity and `applyEntityPickAction()`
   unconditionally deleted it before creating the angle dimension. So clicking L1
   then L2 with the dimension tool deleted L1's permanent auto line-length dimension
   and replaced it with an angle dimension — "stealing" it.

2. **C++ over-constraining:** The same commit added code that re-created both lines'
   dimensions as DRIVING (`is_auto=false`) + created the angle dimension as DRIVING
   (default `is_auto=false`). The solver then had 3 driving constraints on 2 lines
   (angle + 2 lengths), over-constraining the system and deforming geometry.

**Fix (2 files):**

- `dimensionToolPicking.ts`: Changed `deleteDimensionId` to `null` for both
  `entity_distance` and `point_distance_to_entity_reference` actions. The first
  entity's dimension is no longer deleted when creating a relationship dimension.
  The `if (action.deleteDimensionId)` guard in `applyEntityPickAction()` already
  handles null safely.

- `dimension_angle_commands.inc`: Removed the 32-line block that auto-created
  driving line-length dimensions. Set `is_auto = true` on the angle dimension
  so it's display-only at creation (matching auto line-length convention).

- ✅ C++ build clean
- ✅ TS type-check clean
- ✅ C++ tests pass

---

## Not Yet Done

| Phase | Feature | Notes |
|---|---|---|
| **Testing** | Run the app, verify everything works | Nothing tested in running Tauri app yet |
| **P3.4** | Delete legacy dynamic snap math (~400 lines) | Only after P3.2 verified working in app |
| **P3** | Dimension Drive Mode | New IPC + Append Mode with dimension entity as focus |
| **P5** | Kinematic Animation | Far future |

### P3.3 — Multi-Constraint Solves (completed 2026-06-10)

- **`speculativeMultiSolve()`** in `speculativeSolve.ts` — accepts array of speculative constraints, pushes all simultaneously, single INFERENCE solve
- **Shared geometry builder** extracted: `pushSketchGeometryAndConstraints()` — used by both single and multi-constraint paths
- **`speculativeIntersectionSnap()`** — line-line intersection via `point_on_line_pl` × 2; TS proximity gating finds best line pair, single WASM solve refines
- **`speculativeTangentThroughLineSnap()`** — tangent to circle + on a line via `tangent_lc` + `point_on_line_pl`; TS proximity gating finds best circle + line pair
- **`snapIntersectionLineIds: [string, string]`** — new field on `DynamicSnapResult` + `SketchPreviewPoint`
- **`intersection`** i18n snap label added to `en.json` + `ViewportPanel.tsx` labels
- Both helpers wired into `dynamicSnapCandidate()` — gated on `filter.snap_intersection` and `filter.snap_tangent` respectively
- TypeScript `tsc --noEmit`: zero errors

---

## Key Files Changed (this implementation session)

### C++ Core
- `native/cad-core/src/core/sketch/sketch_feature_parameters.h` — +3 fields
- `native/cad-core/src/core/sketch/impl/state_and_create.inc` — freeze logic + helper
- `native/cad-core/src/core/sketch/impl/sketch_feature_lifecycle_declarations.inc` — helper decl
- `native/cad-core/src/core/sketch/impl/line_entity_commands.inc` — append mode
- `native/cad-core/src/core/sketch/impl/curve_primitives.inc` — append mode
- `native/cad-core/src/core/sketch/impl/sketch_axis_constraint_commands.inc` — append mode
- `native/cad-core/src/core/sketch/impl/sketch_parallel_constraint_commands.inc` — append mode
- `native/cad-core/src/core/sketch/impl/sketch_perpendicular_constraint_commands.inc` — append mode
- `native/cad-core/src/core/sketch/impl/sketch_equal_length_constraint_commands.inc` — append mode
- `native/cad-core/src/core/sketch/impl/sketch_tangent_constraint_commands.inc` — append mode
- `native/cad-core/src/core/sketch/impl/line_anchor_commands.inc` — append mode
- `native/cad-core/src/core/viewport/viewport_state.h` — +diagnostics fields
- `native/cad-core/src/core/viewport/impl/dof_status_emit.inc` — populate diagnostics
- `native/cad-core/src/core/viewport/impl/empty_viewport_state.inc` — init defaults
- `native/cad-core/src/core/viewport/impl/viewport_state_return.inc` — return values
- `native/cad-core/src/protocol/impl/viewport_to_payload_state_scene_interaction_entries.inc` — serialization

### TypeScript UI
- `apps/desktop-ui/src/lib/planegcsBridge.ts` — INFERENCE config, SpeculativeSnapType, activePointIds
- `apps/desktop-ui/src/lib/speculativeSolve.ts` — **NEW** — core speculative solve module
- `apps/desktop-ui/src/lib/planegcsSolver.ts` — (no changes)
- `apps/desktop-ui/src/layout/viewport/endpointDrag.ts` — ripple-freeze
- `apps/desktop-ui/src/layout/viewport/snapResolution.ts` — 5 speculative helpers + wiring
- `apps/desktop-ui/src/layout/header/SketchDofBadge.tsx` — **NEW** — toolbar DOF badge
- `apps/desktop-ui/src/layout/header/AppHeader.tsx` — wire SketchDofBadge
- `apps/desktop-ui/src/lib/schemas/ipc/viewportStateSchema.ts` — +diagnostics
- `apps/desktop-ui/src/types/ipc.ts` — +diagnostics
- `apps/desktop-ui/src/types/viewport.ts` — +snapIntersectionLineIds field (P3.3)
- `apps/desktop-ui/src/i18n/en.json` — +intersection snap label (P3.3)
- `apps/desktop-ui/src/layout/ViewportPanel.tsx` — pass intersection label (P3.3)
- `.deepseek/instructions.md` — updated active priority (P3.3)
- `.deepseek/planegcs-integration-plan.md` — marked SUPERSEDED

### Documentation
- `wiki/GCS-Implementation-Strategy.md` — **NEW** — full implementation plan
- `wiki/Home.md` — added link

---

## Build Status

- ✅ C++ `make -j$(nproc)`: all 3 targets built clean
- ✅ C++ tests: `cad_core_multi_profile_extrude_test` + `cad_core_cam_face_reference_test` both pass
- ✅ TypeScript `tsc --noEmit`: zero errors
- ❌ App NOT run / tested yet

## Next Session

1. Run the app: `pnpm dev` from project root, test drawing + constraints + drag
2. Verify DOF badge appears in toolbar when constraints exist
3. Verify append mode doesn't break existing sketches
4. Test intersection snap: draw two crossing lines, try to snap to their intersection
5. Test tangent-through-line snap: draw circle + line, try tangent snap near the line
6. If stable, proceed to P3.4 (delete legacy dynamic snap math ~400 lines)
