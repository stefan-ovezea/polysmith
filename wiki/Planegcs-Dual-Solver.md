# Planegcs Dual-Deployment Solver

PolySmith uses FreeCAD's [PlaneGCS](https://github.com/Salusoft89/planegcs) 2D
geometric constraint solver in **two deployments** — native C++ in the CAD core
and WASM in the TypeScript UI — sharing the same C++ source, constraint model,
and algorithm family.

## Why Dual Deployment

The [Core-UI Design Principles](Core-UI-Design-Principles) mandate that the
core owns document state and the UI owns interaction state. Constraint solving
straddles this boundary:

- **Final positions** after a drag commit are document state — the core's
  native planegcs (200 iterations, 1e-10 tolerance, DogLeg algorithm) is the
  single source of truth.
- **Preview positions** during a drag are interaction state — the UI's WASM
  planegcs (20 iterations, 1e-4 tolerance, Levenberg-Marquardt algorithm) gives
  60fps local feedback with no IPC round-trip.

Both run the **same C++ sources** (`third_party/planegcs/planegcs/`). The only
difference is configuration — iteration count, convergence tolerance, and
algorithm choice.

Before dual deployment, the UI used hand-coded ad-hoc constraint logic that
produced different results from the core solver. Entity positions would "jump"
between drag preview and commit. Dual deployment eliminates this mismatch.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ BUILD TIME                                                      │
│   third_party/planegcs/planegcs/ (C++ sources)                 │
│         │                                                       │
│         ├── Emscripten (Docker) ──► planegcs.wasm              │
│         │                              │                        │
│         │                              ▼                        │
│         │                    @salusoft89/planegcs (npm)         │
│         │                         TypeScript wrapper            │
│         │                              │                        │
│         │                              ▼                        │
│         │                    apps/desktop-ui                    │
│         │                                                       │
│         └── CMake (native) ──► libplanegcs.a                   │
│                                    │                            │
│                                    ▼                            │
│                              native/cad-core                    │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ RUNTIME                                                         │
│                                                                  │
│  ┌──────────────────────┐   IPC (commit only)   ┌────────────┐ │
│  │ UI (WASM planegcs)   │◄─────────────────────►│ Core (C++) │ │
│  │                      │                       │            │ │
│  │ LOOSE                │                       │ EXACT      │ │
│  │ 20 iterations        │                       │ 200 iter   │ │
│  │ 1e-4 tolerance       │                       │ 1e-10 tol  │ │
│  │ Levenberg-Marquardt  │                       │ DogLeg     │ │
│  │                      │                       │            │ │
│  │ INTERACTION STATE    │                       │ DOCUMENT   │ │
│  └──────────────────────┘                       └────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Solver Configurations

| Setting | UI (WASM) | Core (C++) | Reason |
|---------|-----------|------------|--------|
| Max iterations | 20 | 200 | UI needs speed, Core needs accuracy |
| Tolerance | 1e-4 | 1e-10 | UI can be approximate, Core must be exact |
| Algorithm | Levenberg-Marquardt | DogLeg | LM faster for drag, DL more robust for final |
| Temporary constraints | During drag | On commit | UI always dragging, Core on final solve |

## Constraint Type Mapping

The WASM bridge (`apps/desktop-ui/src/lib/planegcsBridge.ts`) maps every
Polysmith constraint type to its planegcs equivalent:

| Polysmith | planegcs |
|---|---|
| `SketchLine.constraint = "horizontal"` | `horizontal_l` |
| `SketchLine.constraint = "vertical"` | `vertical_l` |
| `SketchLineRelation { parallel }` | `parallel` |
| `SketchLineRelation { perpendicular }` | `perpendicular_ll` |
| `SketchLineRelation { equal_length }` | `equal_length` |
| `SketchLineRelation { tangent_line_circle }` | `tangent_lc` |
| Coincident constraint | `p2p_coincident` |
| Concentric constraint | `p2p_coincident` (centers) |
| Midpoint anchor | `point_on_line_pl` |
| Point-line anchor | `point_on_line_pl` |
| `line_length` dimension | `p2p_distance` |
| `line_angle` dimension | `p2p_angle` |
| `circle_radius` dimension | `circle_radius` |
| `angle` (two lines) | `l2l_angle_ll` |
| `point_distance` | `p2p_distance` |
| `circle_center_distance` | `p2p_distance` (centers) |
| `circle_line_distance` | `c2ldistance` |

## Drag Flow

```
pointerdown
  │
  ├─ Start drag on non-fixed sketch point
  │
  ▼
pointermove (rAF-batched, 60fps)
  │
  ├─ 1. resolveSnappedSketchPoint() → raw snap target
  ├─ 2. Shallow-clone params, set dragged point at snap position
  ├─ 3. PlanegcsBridge.solve() — LOOSE config, synchronous WASM
  ├─ 4. Store solved position in dragSnapResultRef
  └─ 5. Render dashed preview lines (anchored → solved)
  │
  ▼
pointerup
  │
  ├─ Commit dragSnapResultRef to core via updateSketchPoint
  └─ Core runs native planegcs (EXACT) for final solve
```

## Key Files

| File | Role |
|---|---|
| `third_party/planegcs/planegcs/` | C++ sources (GCS.cpp, Geo.cpp, …) |
| `native/cad-core/src/core/sketch/constraint_solver.h` | C++ wrapper for native planegcs |
| `native/cad-core/src/core/sketch/constraint_solver.cpp` | C++ wrapper implementation |
| `native/cad-core/CMakeLists.txt` | Builds planegcs as static library |
| `apps/desktop-ui/src/lib/planegcsBridge.ts` | TS bridge — Polysmith ↔ planegcs mapping, solve, apply |
| `apps/desktop-ui/src/lib/planegcsSolver.ts` | Lazy singleton, WASM module lifecycle |
| `apps/desktop-ui/src/layout/ViewportPanel.tsx` | Integration point — `ensureBridge()` init + solve in drag rAF |
| `apps/desktop-ui/public/planegcs.wasm` | WASM binary (dev server) |

## Adding New Constraint Types

When a new constraint kind is added to the C++ planegcs sources:

1. **Update the C++ solver wrapper** — `native/cad-core/src/core/sketch/constraint_solver.cpp`. Add the new constraint kind to `build()` with the appropriate `system_->addConstraint*()` call.

2. **Update the TS bridge** — `apps/desktop-ui/src/lib/planegcsBridge.ts`. Add a new `case` in the constraint or dimensional constraint switch blocks mapping the Polysmith kind to the planegcs constraint type.

3. **Rebuild the WASM binary** — the npm package `@salusoft89/planegcs` ships a pre-built WASM from upstream FreeCAD sources. If the constraint was added locally, rebuild with:
   ```
   cd third_party/planegcs && npm run build:all
   ```
   Then copy the output WASM to `apps/desktop-ui/public/`.

4. **Update this document** — add the new mapping to the constraint table above.
