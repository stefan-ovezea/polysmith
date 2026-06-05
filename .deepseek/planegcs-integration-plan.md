# Plan: Integrate planegcs Constraint Solver into PolySmith

**Started:** 2026-06-05
**Status:** Phase 0–1 complete ✅ — Phase 2 ready (constraint-by-constraint testing)
**Source:** User request to integrate `third_party/planegcs` as the 2D sketch constraint solver
**Principle:** Core owns document state → solver runs natively in C++ core, NOT WASM in UI

## Current State

- Constraints are stored as data (`SketchConstraint`, `SketchLineRelation` in `feature.h`)
- Individual `enforce_*` functions in `sketch_feature.cpp` handle simple single-constraint cases ad-hoc
- **No unified mathematical constraint solver exists**
- planegcs C++ sources live at `third_party/planegcs/planegcs/` (GCS.cpp, Geo.cpp, Constraints.cpp, SubSystem.cpp, qp_eq.cpp)
- planegcs depends on: Boost (already available via OCCT), Eigen3 (**not installed**)

## Architecture Decision

Run planegcs **natively in the C++ core** (compile planegcs sources as a static library, link into `cad_core`). The WASM bindings (`planegcs_dist/`, embind) are not used. This preserves "Core sends DOCUMENT STATE, not interaction state."

---

## Phase 0 — Prerequisites

### 0.1 Install Eigen3
- Package: `libeigen3-dev` (header-only, needs sudo)
- Verification: `pkg-config --cflags eigen3` or `ls /usr/include/eigen3/Eigen/`
- **Action for user: `sudo apt install libeigen3-dev`**

### 0.2 Create native planegcs static library in CMake
- File: `third_party/planegcs/planegcs/CMakeLists.txt` (modify existing, or create sibling)
- Current CMakeLists.txt targets Emscripten/WASM — need a native variant
- Sources: `GCS.cpp`, `Geo.cpp`, `Constraints.cpp`, `SubSystem.cpp`, `qp_eq.cpp`
- Dependencies: Boost (headers only), Eigen3 (headers only)
- Target: `planegcs` static library

### 0.3 Link planegcs into cad_core
- Edit `native/cad-core/CMakeLists.txt`
- Add `target_link_libraries(cad_core PRIVATE planegcs)`
- Add include path for planegcs headers
- Verification: `cmake --build` compiles and links without errors

---

## Phase 1 — Solver Wrapper (bridging layer)

**New files:** `native/cad-core/src/core/constraint_solver.h`, `constraint_solver.cpp`

### 1.1 Create `constraint_solver.h/.cpp`
- Declare `ConstraintSolver` class (or free functions)
- Key functions:
  - `build_system(const SketchFeatureParameters&) → GCS::System`
  - `solve(GCS::System&) → SolveResult { status, conflicting, redundant }`
  - `apply_solution(GCS::System&, SketchFeatureParameters&) → void`
- Verification: file compiles

### 1.2 Implement `build_system()` — Points + Parameters
- Map each `SketchPoint` → planegcs `GCS::Point` (x, y as `double*` in parameter vector)
- Handle `is_fixed` flag → fixed parameters in planegcs
- Verification: unit test — 2 points + coincident constraint → solver converges

### 1.3 Implement `apply_solution()`
- Read solved parameter values back from planegcs
- Write to `SketchFeatureParameters.points[]` x/y
- Verification: point coordinates change to satisfy constraints after solve

### 1.4 Handle solver status and diagnostics
- Map `GCS::SolveStatus` → Polysmith status
- Expose conflicting/redundant constraint lists
- Verification: over-constrained sketch → detects conflict (doesn't crash)

---

## Phase 2 — Basic Constraints (one kind at a time)

Each step: map Polysmith constraint → planegcs constraint, test, verify.

### 2.1 Coincident (point-point)
- Polysmith: `SketchConstraint{kind="coincident", target_ids=[p1, p2]}`
- planegcs: `GCS::ConstraintP2PCoincident`
- Test: 2 points + coincident → merge to same position after solve

### 2.2 Horizontal / Vertical on lines
- Polysmith: `SketchLine.constraint = "horizontal" | "vertical"`
- planegcs: `GCS::ConstraintLineHorizontal` / `GCS::ConstraintLineVertical`
- Test: angled line + H constraint → snaps horizontal

### 2.3 Point on object (line, circle)
- Polysmith: stored via `point_line_anchors` / `midpoint_anchors`
- planegcs: `GCS::ConstraintPointOnLine` / `GCS::ConstraintPointOnCircle`
- Test: point on line → stays on line when line moves

### 2.4 Parallel lines
- Polysmith: `SketchLineRelation{kind="parallel"}`
- planegcs: `GCS::ConstraintLinesParallel`
- Test: 2 lines + parallel → stay parallel after solve

### 2.5 Perpendicular lines
- Polysmith: `SketchLineRelation{kind="perpendicular"}`
- planegcs: `GCS::ConstraintLinesPerpendicular`
- Test: 2 lines + perpendicular → maintain 90°

### 2.6 Equal length lines
- Polysmith: `SketchLineRelation{kind="equal_length"}`
- planegcs: `GCS::ConstraintLinesEqualLength`
- Test: 2 lines + equal → same length after solve

### 2.7 Concentric circles
- Polysmith: `SketchConstraint{kind="concentric", target_ids=[c1, c2]}`
- planegcs: `GCS::ConstraintCirclesConcentric`
- Test: 2 circles + concentric → share center

### 2.8 Tangent (line-circle)
- Polysmith: `SketchLineRelation{kind="tangent"}` (line ↔ circle)
- planegcs: `GCS::ConstraintLineTangentToCircle`
- Test: line tangent to circle after solve

---

## Phase 3 — Dimensional Constraints

### 3.1 Distance (point-to-point)
- Polysmith: `SketchConstraint{kind="distance", value=N}`
- planegcs: `GCS::ConstraintP2PDistance`
- Test: 2 points + distance=50 → stay 50mm apart

### 3.2 Radius on circle/arc
- Polysmith: stored on `SketchCircle` or via constraint
- planegcs: `GCS::ConstraintCircleRadius`
- Test: circle + radius=25 → radius stays 25

### 3.3 Angle between lines
- Polysmith: via constraint
- planegcs: `GCS::ConstraintL2LAngle`
- Test: 2 lines + angle=45° → maintain 45°

### 3.4 Horizontal/Vertical distance
- planegcs: `GCS::ConstraintP2PHorizontalDistance` / `GCS::ConstraintP2PVerticalDistance`
- Test: points maintain axis-aligned separation

---

## Phase 4 — Pipeline Integration

### 4.1 Call solver from `refresh_sketch_derived_state()`
- In `sketch_feature.cpp`, after current enforce calls, add solver pass
- Build system → solve → apply solution
- Verification: existing sketch files load and constraints are solver-enforced

### 4.2 Gradual removal of ad-hoc `enforce_*` functions
- As solver handles each constraint kind, remove the corresponding enforce function
- Keep enforce functions only for constraints not yet migrated
- Verification: no regression in existing sketches

### 4.3 DOF counting via solver
- Use `GCS::System::dofs()` instead of manual `dof_counter.cpp`
- Verification: UI DOF display matches solver-reported DOF

### 4.4 Graceful failure handling
- On solver failure: log warning, leave geometry in last-good state
- Over-constrained: report conflicting constraints to UI
- Verification: broken sketch → warning logged, app doesn't crash

---

## Phase 5 — UI & Drag Integration

### 5.1 Inference engine → solver-validated constraints
- `inference_engine.cpp` adds constraints → solver validates them
- Verification: auto-created coincident constraints work correctly

### 5.2 Drag with temporary constraints
- On mouse-down for point drag: add temporary constraint (`SpecialTag::DefaultTemporaryConstraint`)
- Solve → preview → on mouse-up commit or clear temporary
- Verification: dragging a constrained point respects existing constraints

### 5.3 Visual feedback
- Fully constrained → green, under-constrained → yellow, over-constrained → red
- Conflicting constraints highlighted in UI
- Verification: color coding updates after each solve

---

## Key Files (existing, will be modified)

| File | Role |
|------|------|
| `native/cad-core/CMakeLists.txt` | Add planegcs link + Eigen3 include |
| `native/cad-core/src/core/feature.h` | Constraint data structures (possibly extend) |
| `native/cad-core/src/core/sketch_feature.cpp` | Recompute pipeline (call solver) |
| `native/cad-core/src/core/inference_engine.cpp` | Constraint creation at draw time |
| `native/cad-core/src/core/dof_counter.cpp` | May replace with solver-based DOF |
| `third_party/planegcs/planegcs/CMakeLists.txt` | Native build target |

## Key Files (new)

| File | Role |
|------|------|
| `native/cad-core/src/core/constraint_solver.h` | Solver wrapper interface |
| `native/cad-core/src/core/constraint_solver.cpp` | Solver wrapper implementation |
| `native/cad-core/tests/constraint_solver_test.cpp` | Unit tests |

## Blockers

- **Eigen3 not installed** — needs `sudo apt install libeigen3-dev` (root required)
- planegcs CMakeLists.txt targets Emscripten — needs native build variant

## Dimension Auto-Delete Improvements (2026-06-05)

Before the planegcs solver lands, three robustness improvements were made
to the existing auto-dimension deletion mechanism in `ViewportPanel.tsx`:

1. **`touchedFields` tracking** — added alongside `lockedFields` on
   `DraftDimensionSession`. A field is marked touched as soon as the user
   types into it, even if they later clear the value. This handles the
   "typed '10', backspaced to empty" edge case: the dimension is preserved
   because the user demonstrated intent. Previously, only `lockedFields`
   was checked (set on Enter/Tab commit), so clearing a field after typing
   would treat it as never-interacted.

2. **Dimension lookup instead of ID prediction** — the deletion effect now
   searches `sketch.dimensions` by `entity_id` + `kind` instead of
   constructing IDs like `dim-line-${line.line_id}`. This tolerates future
   ID format changes and avoids race conditions where the predicted ID
   doesn't match what the core created.

3. **Safer deletion** — each deletion now guards on `findDimId()` returning
   a value, so a missing dimension (construction line, core bug) silently
   no-ops instead of sending a delete for a non-existent dimension.

## Next Action

User installs Eigen3: `sudo apt install libeigen3-dev`
Then: Phase 0.2 — create native planegcs CMake build
