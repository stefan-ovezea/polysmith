# GCS Implementation Strategy — Freeze State Machine & Advanced Solver Features

**Created:** 2026-06-09
**Status:** Planning
**Source:** User-provided implementation strategy document (`.deepseek/pastes/`)
**Builds on:** [Planegcs Dual-Deployment Solver](Planegcs-Dual-Solver), [Core-UI Design Principles](Core-UI-Design-Principles)

## Overview

This document defines the implementation strategy for the advanced 2D Geometric
Constraint Solver (GCS) features: the **freeze state machine** (solving the
"ghost movement" appending problem), **speculative constraint inferencing**,
**DOF-driven visual feedback**, and **parametric dimension driving**.

The plan builds on the existing dual-deployment architecture: the C++ core runs
planegcs natively (EXACT, 200 iter, DogLeg) as the source of truth; the UI runs
planegcs via WASM (LOOSE, 20 iter, Levenberg-Marquardt) for 60fps interactive
previews.

---

## Part 1: The Appending Problem (Ghost Movements)

### Current Behaviour

When the user draws a new line or adds a constraint to an existing sketch, the
solver distributes displacement across ALL free parameters in the system. This
can shift existing geometry — "ghost movements" — even though only one new
element was added.

### Root Cause

The planegcs solver (both native and WASM) treats every non-`is_fixed` parameter
as an unknown. All unknowns compete for displacement in the optimization. There
is no mechanism to tell the solver "this subset should stay still."

### Solution: Freeze State Machine

The solver must be told which parameters are **frozen** (locked in place) and
which are **active** (free to move). Three distinct modes drive different freeze
configurations:

| Mode | Strategy | is_fixed on existing | is_fixed on new/target |
|---|---|---|---|
| **Append** (new geometry/constraint) | Freeze-All-Then-Thaw | `true` | `false` |
| **Drag** (interactive endpoint move) | Ripple-Freeze | `true` except dragged point + 1-hop dependents | `false` |
| **Recompute** (full solve on commit) | No freeze | as stored | as stored |

### Append Mode — Execution Routine

When the user draws a new entity or applies a constraint to existing geometry:

1. **Snapshot is_fixed state**: Save the current `is_fixed` values for all
   points so they can be restored after the solve.
2. **Freeze history**: Set `is_fixed = true` on ALL existing `SketchPoint`
   entries (but NOT circle center points — those must remain free to satisfy
   radius/concentric constraints declared on circles).
3. **Isolate target**: Clear `is_fixed` on points belonging to the newly
   created entity (or the entity being constrained).
4. **Solve**: Run the solver. Only the new/target entity moves; everything else
   stays put.
5. **Restore**: Reset `is_fixed` to the pre-solve snapshot values.
6. **Early-return on failure**: If the solver diverges or over-constrains,
   restore `is_fixed` and fall back to the ad-hoc `enforce_*` functions (which
   already exist and handle this case).

#### Implementation

**C++ Core** (`constraint_solver.h/.cpp`):
- Add `AppendMode` enum: `{None, Append, Drag}`
- Add `set_mode(AppendMode)` to `ConstraintSolver`
- Modify `build()`: when mode is `Append`, override `is_fixed` on the
  `PointMapping` entries:
  - All existing points → `is_fixed = true` (skip from unknowns)
  - Only points referenced by the new entity → `is_fixed = false`
- Add a `set_focus_ids(vector<string>)` overload that marks which entity IDs
  (lines/circles/points) are the "new" ones — these get unfrozen, everything
  else gets frozen.
- Restore original `is_fixed` after solve (the `PointMapping` is rebuilt each
  `build()`, so no persistent mutation is needed — just override in-system).

**TS WASM** (`planegcsBridge.ts`):
- Add `frozenPointIds?: string[]` to the `solve()` options parameter
- When set, push those points as `fixed: true` regardless of their stored
  `is_fixed` value
- Default behaviour (no frozenPointIds) = current behaviour (use stored
  `is_fixed`)

**Integration** (`sketch_feature.cpp:refresh_sketch_derived_state`):
- The solver is already called before enforce functions
- Determine if this is an "append" scenario by comparing entity counts before
  and after the mutation — if one new line/circle/point was added, run in
  Append mode with that entity as the focus
- This may require passing a `focus_ids` hint through the recompute pipeline

### Drag Mode (Ripple-Freeze)

When the user drags an endpoint:

1. **Freeze all except the dragged point** and its **1-hop connected entities**
   (lines that share the dragged point, circles whose center is the dragged
   point).
2. **Solve LOOSE**: WASM solver adjusts only the dragged point + connected
   primitives.
3. **Render preview**: Dashed lines from anchors to solved position.
4. **On pointerup**: Commit the final snapped position. Core runs EXACT solve
   with full constraint system (no freeze).

#### Current State

The WASM solver already runs during drag (`endpointDrag.ts:resolveEndpointDragFrame`)
but does NOT freeze non-dragged entities. The solver sees all constraints and
may distribute displacement to unrelated geometry.

#### Implementation

- Add `activePointIds?: string[]` to `PlanegcsBridge.solve()` options
- When set, mark every point NOT in `activePointIds` as `fixed: true` for the
  WASM solve
- In `resolveEndpointDragFrame`, compute the 1-hop set from the dragged
  `pointId`:
  - Find all lines where `start_point_id` or `end_point_id` equals `pointId`
  - Collect the opposite endpoints of those lines
  - Find all circles where the center point ID matches (from the naming
    convention `point-circle-{id}-center`)
  - Pass this set as `activePointIds`

---

## Part 2: Over-Constrained Conflict Trapping

### Current State

Both solvers return `conflicting` and `redundant` constraint ID lists. The
C++ solver prints them to stderr. The TS bridge captures them in `SolveOutput`.
Neither surfaces them to the user.

### Implementation

1. **Surface solver diagnostics in viewport state**:
   - Add `solver_conflicting: string[]` and `solver_redundant: string[]` to the
     sketch feature parameters or a parallel diagnostics structure
   - Populate from the C++ solver result in `refresh_sketch_derived_state`

2. **UI visual feedback**:
   - **DOF badge**: Add a small badge/indicator in the sketch toolbar showing
     DOF count (e.g., "DOF: 3" in yellow, "DOF: 0" in green)
   - **Constraint badge colour**: Conflicting constraints → red badge,
     redundant → orange badge, normal → current colour
   - **Sketch entity stroke colour**: Over-constrained entity → red stroke
     (use the existing scene object colour pipeline in
     `sketchSceneObjects.ts`)

3. **Tooltip on conflict**: Hovering a red constraint badge shows the list of
   conflicting constraints

---

## Part 3: Speculative Constraint Inferencing — WASM as a General-Purpose Geometric Inference Engine

### Original State

Before P3.2 and P3.4, snap inference during pointer tracking was done with
hand-coded geometric math in TS (`snapResolution.ts`,
`activeSketchPointerMove.ts`). Each snap type
(axis-lock, tangent, perpendicular, parallel, line-body) has 30–60 lines of
dedicated math with its own edge-case handling. This is:

- **Fragile** — each snap type has different edge cases and tolerance handling
- **Unaware of constraints** — snapping doesn't account for existing
  sketch constraints (e.g., a point constrained to a line won't snap correctly
  to a perpendicular orientation off that line)
- **Hard to extend** — adding new snap types requires writing more fragile
  geometry loops
- **Cannot do multi-entity inference** — snapping to the intersection of two
  lines, or the point where a perpendicular from cursor meets a tangent circle,
  requires solving simultaneous constraint equations

### Vision: The Solver as an Inference Engine

Replace the hand-coded geometric snap math with **speculative constraint solves**
against the WASM solver. The core insight: the planegcs solver already knows
about ALL the geometry and ALL the constraints. Instead of computing tangent
points with `asin(r/d)` in TS, push a `temporary: true` tangent constraint
linking a virtual cursor line to the target circle, run a fast solve (5–10
iterations, 1e-3 tolerance), and read the exact tangent point directly from the
solver.

The WASM solver becomes a **general-purpose geometric query engine**:
- _"Given a line from P to cursor C, and a constraint that it's perpendicular to line L, where should C be?"_ → Add `perpendicular_ll(virtualLine, L, temporary:true)`, solve, read C's new position.
- _"Given a cursor point C near two lines A and B, where is their intersection?"_ → Add `point_on_line_pl(C, A)` + `point_on_line_pl(C, B)`, solve, read C.
- _"Given a draft line and a nearby circle, where is the tangent snap point?"_ → Add `tangent_lc(virtualLine, circle, temporary:true)`, solve, read endpoint.

### Architecture: Speculative Solve Pattern

```
pointermove (rAF-batched, 60fps)
  │
  ├─ 1. Determine draft context:
  │     - draftStartLocal (if mid-draft)
  │     - cursorLocal (raw pointer position)
  ├─ 2. Build probe set:
  │     - Gather nearby snap targets (lines within threshold, circles, etc.)
  │     - For each target, construct a speculative constraint pair:
  │       { virtualPrimitive, constraint }
  ├─ 3. Run speculative solves (can batch in one solve call):
  │     - Push all sketch geometry + existing constraints (rebuild, same as drag)
  │     - Push virtual cursor point + virtual draft line
  │     - Push ONE speculative constraint with temporary:true
  │     - Solve with INFERENCE config (5 iter, 1e-3 tolerance)
  │     - Read cursor endpoint position → snap candidate
  │     - Clear, repeat for next snap type
  ├─ 4. Choose best snap:
  │     - For each candidate where solver converged (status 0 or 1):
  │       - Compute distance from raw cursor to solved position
  │       - If distance < snapThresholdPx, accept
  │     - Select the candidate with smallest distance
  └─ 5. Return snap position (overrides raw cursor in resolveSnappedSketchPoint)
```

### Solver Configuration: INFERENCE Mode

A third solver config, optimized for speculative queries:

| Setting | LOOSE (Drag) | **INFERENCE (Snap)** | EXACT (Commit) |
|---|---|---|---|
| Max iterations | 20 | **5** | 200 |
| Tolerance | 1e-4 | **1e-3** | 1e-10 |
| Algorithm | Levenberg-Marquardt | **Levenberg-Marquardt** | DogLeg |
| Purpose | Drag preview | **Snap inference** | Final solve |

The INFERENCE config is intentionally coarser — we only need to know if the
constraint converges at all, and approximately where to. The actual committed
position is recomputed by the core's EXACT solver.

### Snap Type → planegcs Constraint Mapping

Each snap type maps to a specific planegcs constraint on a **virtual draft line**
or **virtual cursor point**:

| Snap Type | Virtual Primitive | planegcs Constraint | `temporary` |
|---|---|---|---|
| **Axis Lock (H/V)** | Virtual line (draft_start → cursor) | `horizontal_l` or `vertical_l` | `true` |
| **Tangent to circle** | Virtual line (draft_start → cursor) | `tangent_lc` (line + circle) | `true` |
| **Perpendicular to line** | Virtual line (draft_start → cursor) | `perpendicular_ll` (virtual line + host line) | `true` |
| **Parallel to line** | Virtual line (draft_start → cursor) | `parallel` (virtual line + host line) | `true` |
| **Line body (on-line)** | Virtual cursor point | `point_on_line_pl` (cursor point + host line) | `true` |
| **Endpoint/coincident** | Already handled by static snap (no solver needed) | `p2p_coincident` | `true` |
| **Intersection (2 lines)** | Virtual cursor point | `point_on_line_pl`(C, L1) + `point_on_line_pl`(C, L2) | `true` |
| **Midpoint on line** | Virtual cursor point | `midpoint_on_line_ll` or `p2p_coincident` with midpoint point | `true` |
| **Concentric (circle)** | Virtual cursor point at circle center | `p2p_coincident` (cursor center + existing center) | `true` |

### Key Design Decision: Batched vs Sequential Solves

**Option A — One solve per snap type (recommended for Phase 1):**

For each candidate snap type relevant to the current pointer context, rebuild
the system, add one speculative constraint, solve, and read the result. This is
simple to implement and debug. A solve with 5 iterations on a typical sketch
(20–50 parameters) completes in ~0.05ms in WASM — well within the 16ms frame
budget even with 6–8 snap types checked per frame.

```
for each snap type enabled by filter:
  w.clear_data()
  pushAllGeometry(params)
  pushAllConstraints(params)
  pushVirtualCursorLine(draftStart, cursor)
  pushSpeculativeConstraint(snapType, target)
  w.set_max_iterations(5)
  w.set_convergence_threshold(1e-3)
  status = w.solve(LevenbergMarquardt)
  if status converged:
    solvedPos = w.get_gcs_params()[cursorPointIndex]
    candidates.push({ kind, position: solvedPos, distance })
```

**Option B — Single solve with all speculative constraints (future optimization):**

Push all speculative constraints simultaneously, solve once, read all results.
This is faster but harder to disambiguate — constraints may conflict with each
other, and the solver distributes displacement across all of them. Requires
careful weight tuning. Deferred to Phase 4.

### Proximity Gating (Performance)

Running the WASM solver on every pointer move for every snap type would be
wasteful when the cursor is far from any snap target. Apply proximity
pre-filtering:

1. **Grid snap** and **axis lock** are geometric (no entities involved) — keep
   current TS math, it's trivial.
2. **Static snaps** (endpoint, midpoint, center) are point-distance checks —
   keep current TS math, it's already correct and fast.
3. **Dynamic snaps** (tangent, perpendicular, parallel, line-body) require the
   solver **only when**:
   - A compatible target entity exists within the snap threshold distance
   - The relevant selection filter toggle is ON
   - The draft has moved enough (> 1px) to establish a direction vector

This means the solver-based path runs only when there's actually a candidate
to evaluate — typically < 10% of pointer move frames.

### Candidate Ranking

After collecting solver-validated snap candidates:

1. **Priority snaps** (endpoint, midpoint) always win regardless of distance —
   keep existing static-snap-first priority.
2. **Solver-validated dynamic snaps** outrank static nearest/body snaps when
   closer than the static candidate.
3. **Distance ordering** within each tier: closest snap wins.

### Implementation Plan — Phased Rollout

#### Phase 3.1: SpeculativeSolve Helper (foundation)

**New file:** `apps/desktop-ui/src/lib/speculativeSolve.ts`

```typescript
// Core function — reusable across all snap types
function speculativeSnapSolve({
  bridge, params, constraints,
  draftStart, cursor,
  snapType, targetEntityId,
}: SpeculativeSolveParams): SpeculativeResult | null
```

- Creates a virtual cursor line (draft_start → cursor) in the solver
- Pushes the appropriate speculative constraint based on `snapType`
- Runs INFERENCE-mode solve
- Returns `{ position: [number, number], converged: boolean, residual: number }` or null

#### Phase 3.2: Replace Dynamic Snaps (one type at a time)

| Step | Snap Type | Replaces TS Function | planegcs Constraint |
|---|---|---|---|
| 3.2a | **Tangent** | `tangentSnapCandidate()` (lines 682–738 in snapResolution.ts) | `tangent_lc` |
| 3.2b | **Perpendicular** | `perpendicularSnapCandidate()` (lines 740–808) | `perpendicular_ll` |
| 3.2c | **Parallel** | `parallelSnapCandidate()` (lines 810–878) | `parallel` |
| 3.2d | **Line Body** | `lineBodySnapCandidate()` (lines 635–680) | `point_on_line_pl` |
| 3.2e | **Axis Lock** | `axisLockSnapCandidate()` (lines 575–633) | `horizontal_l` / `vertical_l` |

Each step:
1. Add the speculative solve variant to `speculativeSolve.ts`
2. Wire into `dynamicSnapCandidate()` (in `snapResolution.ts`)
3. Run side-by-side with existing TS math for comparison during testing
4. Remove TS math variant once solver-based version is verified
5. The solver-based result carries a `solverValidated: true` flag that makes
   it outrank static nearest/body snaps when closer

#### Phase 3.3: Multi-Constraint Solves (intersection, compound snaps)

Snaps that require TWO simultaneous constraints:
- **Intersection of two lines**: `point_on_line_pl` on L1 + `point_on_line_pl` on L2
- **Tangent from point to circle through a line**: `tangent_lc` + `point_on_line_pl`
- **Perpendicular through midpoint**: `perpendicular_ll` + `point_on_line_pl` at t=0.5

These are inherently solver-only — the current TS code can't express them at
all. They become possible only because the speculative solve pattern can
handle multi-constraint systems.

#### Phase 3.4: Delete Legacy Dynamic Snap Math (completed 2026-06-13)

After all snap types were solver-based and P3.2 was verified in the app, the
hand-coded geometric fallback functions were removed from `snapResolution.ts`:
- `axisLockSnapCandidate` (~60 lines)
- `tangentSnapCandidate` (~56 lines)
- `perpendicularSnapCandidate` (~70 lines)
- `parallelSnapCandidate` (~80 lines)
- `lineBodySnapCandidate` (~45 lines)
- Various helper functions (`closestPointOnSegment2d`, `angleDiffBetween2d`)

`dynamicSnapCandidate` now only coordinates speculative solver snaps plus the
P3.3 multi-constraint solves. Fast TS proximity gating remains inside the
speculative helpers so the UI still runs one targeted solve per snap type
instead of broad-solving every entity.

**Net deletion: ~300 lines of fragile fallback geometry math.**

### planegcsBridge API Changes

```typescript
// New config for inference
export const INFERENCE: SolverConfig = {
  maxIterations: 5,
  convergenceThreshold: 1e-3,
  algorithm: Algorithm.LevenbergMarquardt as Algorithm,
};

// New method on PlanegcsBridge
speculativeSolve({
  params: SketchFeatureParameters,
  constraints: SketchConstraintData[],
  draftStart: [number, number],
  cursor: [number, number],
  snapType: SpeculativeSnapType,
  targetEntityId: string,
}): SpeculativeResult | null

type SpeculativeSnapType =
  | "tangent_lc"
  | "perpendicular_ll"
  | "parallel"
  | "point_on_line_pl"
  | "horizontal_l"
  | "vertical_l";

interface SpeculativeResult {
  position: [number, number];  // solved snap point
  converged: boolean;
  residualDistance: number;    // screen pixels from raw cursor
}
```

### Interaction with Existing Constraints

A critical advantage of solver-based snapping: the speculative solve respects
ALL existing constraints on the sketch. If the cursor point is coincident with
a point that is already constrained to a line, snapping perpendicular from that
point will correctly account for the chain of constraints — something the
current TS math cannot do.

### Edge Cases & Safety

1. **Solver failure**: If the speculative solve returns `Failed` (status 2),
   that dynamic snap candidate is rejected for the frame. Speculative
   constraints are always `temporary: true` so they never affect the DOF count
   or conflict tracking.

2. **Over-constrained during inference**: If adding a speculative constraint
   over-constrains the system (e.g., horizontal + vertical on same line),
   the solver reports `conflicting` — the snap is rejected for the frame.

3. **Performance budget**: With max 5 iterations per solve and typical sketch
   sizes (10–50 parameters), a single speculative solve takes ~0.02–0.05ms.
   Checking 6 snap types per frame = ~0.3ms, leaving 15.7ms for rendering.

4. **WASM memory**: Each `clear_data()` + rebuild cycle allocates WASM objects.
   The `GcsWrapper` already handles cleanup via `clear_data()`. No memory leak
   concerns — the WASM heap is reused across solves.

---

## Part 4: DOF-Driven Visual Feedback

### Current State

DOF counting exists in `dof_counter.cpp` using a manual per-entity
`total - consumed` model. The solver also reports system-level DOF via
`solver_dofs`. Neither is surfaced as a visual colour change.

### Implementation

1. **Use solver DOF as the authoritative count**:
   - The solver's `dofsNumber()` after `diagnose()` is the ground truth
   - Keep `dof_counter.cpp` as a fallback for sketches without constraints
     (where the solver doesn't run)
   - Add `solver_dofs` to the viewport state IPC so the UI can read it

2. **Entity colour transitions**:
   - **Under-constrained** (>0 DOF): Blue stroke (default, current)
   - **Fully constrained** (0 DOF): Black stroke
   - **Over-constrained** (<0 DOF or conflicting): Red stroke
   - Implement in `sketchSceneObjects.ts` — after scene sync, walk entities
     and override stroke colour based on `get_entity_dof_status()`

3. **Sketch-level DOF badge** in the toolbar:
   - "DOF: N" with colour coding
   - Click to open a panel listing per-entity DOF status

---

## Part 5: Parametric Dimension Driving

### Current State

Dimensions are displayed (auto and manual). Manual dimensions with expressions
are enforced by the solver. But the interaction flow is mostly one-way: draw
entity → dimension auto-created → user edits expression → solver enforces.

### Vision

When a user edits a dimension value, the change should flow directly:
1. Update the dimension's `value` in sketch parameters
2. Trigger a localized solver pass (not a full recompute)
3. UI transforms smoothly to the new geometry

### Implementation

1. **Add `update_dimension_value` IPC command**:
   - Updates a single dimension's expression and value
   - Triggers a solver-only recompute (not a full `refresh_sketch_derived_state`)
   - Returns the new geometry state with solved positions

2. **Add "drive" mode to the solver**:
   - When re-solving after a dimension edit, freeze all geometry EXCEPT the
     entity the dimension belongs to and its immediate dependents
   - This is effectively Append mode with the dimension's entity as focus

---

## Part 6: Future — Kinematic Mechanism Animation

Deferred. Requires:
- A driving angle/parameter concept in the data model
- A render-loop-integrated solver that can step through a parameter range
- UI controls for angle range, speed, and playback

Not in scope for the current planning cycle.

---

## Implementation Order (Priority)

| Phase | Feature | Dependencies | Complexity |
|---|---|---|---|
| **P1** | Append Mode (Freeze-All) | None | Medium — solver + TS bridge |
| **P1** | Drag Ripple-Freeze | Append Mode helpers | Low — mostly wiring in endpointDrag.ts |
| **P2** | Over-Constrained UI Feedback | None | Low — existing data, surface to UI |
| **P2** | DOF Visual Feedback | None | Medium — colour pipeline + IPC |
| **P3.1** | `speculativeSolve.ts` helper + INFERENCE config | planegcsBridge `temporary` support | Medium — new TS module |
| **P3.2a** | Speculative Tangent Snap | P3.1 | Low — replace `tangentSnapCandidate` |
| **P3.2b** | Speculative Perpendicular Snap | P3.1 | Low — replace `perpendicularSnapCandidate` |
| **P3.2c** | Speculative Parallel Snap | P3.1 | Low — replace `parallelSnapCandidate` |
| **P3.2d** | Speculative Line-Body Snap | P3.1 | Low — replace `lineBodySnapCandidate` |
| **P3.2e** | Speculative Axis Lock Snap | P3.1 | Low — replace `axisLockSnapCandidate` |
| **P3.3** | Multi-Constraint Solves (intersection, compound) | P3.2 | High — new solver capabilities |
| **P3.4** | Delete legacy dynamic snap math | P3.2 complete | Done — pure deletion |
| **P3** | Dimension Drive Mode | Append Mode helpers | Medium — new IPC + solver mode |
| **P5** | Kinematic Animation | Driving angle concept | Very high |

---

## Key Files

| File | Changes Required |
|---|---|
| `native/cad-core/src/core/sketch/constraint_solver.h` | Add Append/Drag mode, focus_ids |
| `native/cad-core/src/core/sketch/constraint_solver.cpp` | Freeze logic in build(), mode dispatch |
| `native/cad-core/src/core/sketch/impl/constraint_solver_system_setup.inc` | Mode-aware unknown declaration |
| `native/cad-core/src/core/sketch/sketch_feature.cpp` | Pass focus hint to solver, surface diagnostics |
| `apps/desktop-ui/src/lib/planegcsBridge.ts` | frozenPointIds/activePointIds options, transient constraints |
| `apps/desktop-ui/src/layout/viewport/endpointDrag.ts` | Ripple-freeze 1-hop set computation |
| `apps/desktop-ui/src/layout/viewport/sketchSceneObjects.ts` | DOF-based colour pipeline |
| `apps/desktop-ui/src/layout/header/SketchToolbar.tsx` | DOF badge |
| `apps/desktop-ui/src/types/ipc.ts` | Solver diagnostics in viewport state |
| `apps/desktop-ui/src/lib/planegcsBridge.ts` | INFERENCE config, `speculativeSolve()` method, `temporary` constraint flag handling |
| `apps/desktop-ui/src/layout/viewport/snapResolution.ts` | Speculative solver dynamic snaps; legacy fallback math deletion complete |

## Key Files (New)

| File | Role |
|---|---|
| `wiki/GCS-Implementation-Strategy.md` | This document |
| `apps/desktop-ui/src/lib/speculativeSolve.ts` | Speculative solve helper — builds virtual primitives, runs INFERENCE solves, returns snap candidates |
