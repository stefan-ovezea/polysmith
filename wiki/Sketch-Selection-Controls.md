# Sketch Selection Controls

> **Status as of 2026-05-27:** Selection filter + checkbox panel shipped.
> Inference engine (coincident, concentric) shipped. DOF counting + entity
> coloring shipped. Constraint deletion UI shipped (select badge → Delete).
> Constraint badge highlight + right-click → Delete shipped. Rectangle drag
> selection shipped (window + crossing modes, now includes arcs). C++ snap
> engine wired with all collectors, snap_candidates through IPC. Parallel
> snap, Alt-key override, circle-nearest snap shipped. Arc profile fix for
> trimmed circles.
> Remaining: Object Snap Tracking, driven dimension proposal, DOF color
> legend.

> This page consolidates the three interrelated subsystems (Constraints,
> Snapping, Selection) into a single implementation roadmap. It defines
> not only *what* to build but *how* to build it in PolySmith's C++ core /
> React UI architecture.

---

## 1. Why These Three Are One System

In the industry standard, constraint, snap, and selection behavior are
not three independent features. They are three outcomes of a single
question: *what geometric elements can the cursor see right now?*

| User action | What the unified filter enables |
|---|---|
| **Selection** (click/drag) | Only filtered-in entity types are highlighted or chosen |
| **Snapping** (during drawing) | Only key points on filtered-in entities are snap targets |
| **Constraining** (applying rules) | Only filtered-in entities can receive constraints |

This means the user-facing controls — checkboxes in a settings panel —
are not "snap settings." They are a **selection filter** that
simultaneously governs all three behaviors.

**Architecture rule:** never build a snap toggle that leaves an entity
type unselectable but still snappable, or selectable but not
constrainable. They move together.

### Pre-existing State & Impact Analysis

This plan must preserve and build upon what already works. Below is what
is currently in production (verified from source on 2026-05-23) and how
each piece maps to this strategy.

#### Already in place — C++ core

**3D selection (fully operational, must be preserved):**
- `select_face` / `select_edge` / `select_vertex` — IPC commands with
  corresponding `DocumentState` fields (`selected_face_id`,
  `selected_edge_ids`, `selected_vertex_ids`). Edge and vertex
  selection support multi-select (shift-click toggle). Used by fillet,
  chamfer, and extrude target-body pickers.
- `select_feature` / `select_reference` — feature and reference plane
  selection.
- `ViewportState` carries `edges[]` and `vertices[]` arrays, each with
  stable topology ids. The three.js renderer raycasts vertices first,
  then edges, then faces.

**2D sketch selection (fully operational, must be preserved):**
- `select_sketch_point` / `select_sketch_entity` /
  `select_sketch_dimension` / `select_sketch_profile` — IPC commands.
  Single and multi-select (`selected_sketch_entity_ids`,
  `selected_sketch_profile_ids`).
- Sketch entity selection drives the inspector panel and is used by
  the Dimension tool for entity picking.

**Constraint system (partial but real, must be extended not replaced):**
- `SketchLine.constraint` — optional `"horizontal"` or `"vertical"`
  string on each line. Enforced during dimension solve via
  `apply_line_constraint()`.
- `SketchLineRelation` — `{ id, kind, first_line_id, second_line_id }`
  stored in `SketchFeatureParameters.line_relations[]`. Supports
  `"equal_length"` and `"perpendicular"` relations.
- `ViewportSketchConstraintPrimitive` — rendered in viewport with
  `constraint_id`, `kind`, `entity_id`, `related_entity_id`, position,
  and selection state. Emitted in `viewport_state.sketch_constraints[]`.
- Constraint badge rendering in `viewport.cpp`: dedicated primitives
  for horizontal, vertical, fixed, midpoint anchor, on-line anchor,
  equal-length, and perpendicular constraints.
- `SketchMidpointAnchor` / `SketchPointLineAnchor` — parametric
  anchoring of points to line midpoints / bodies, kept in sync by
  `refresh_sketch_derived_state`.

**Sketch entities (fully operational):**
- Lines, circles, polygons, arcs with stable point IDs
- `is_construction` flag on all entities (construction geometry is
  excluded from profile detection but participates in snapping and
  constraints)
- `SketchFillet` — parametric corner fillets with trim points and
  generated arcs
- `SketchProjection` — live-linked projection from body faces/edges

#### Already in place — TypeScript

**Snap resolution (TS side, currently NOT in C++):**
- `resolveSnappedSketchPoint()` in `ViewportPanel.tsx` — resolves the
  nearest snap candidate on every mouse move during drawing.
- Supports: endpoint, midpoint, perpendicular-foot, and tangent snaps.
- Uses `sketchSnapCandidatesRef` built from viewport state entities.
- `SKETCH_SNAP_DISTANCE` constant (pixel-based tolerance).
- Snap labels displayed via `setSketchSnapLabel()`.

**No selection filter exists:** There is no user-facing checkbox panel
to toggle which entities/snaps are active. The `SelectionFilter` struct,
`SelectionFilterPanel.tsx`, and `update_selection_filter` IPC command
are all new work — they don't conflict with anything.

#### What changes and what stays

| Existing feature | Fate under this plan |
|---|---|
| `select_face/edge/vertex` IPC | **Untouched.** The filter governs what the cursor *sees*, but the IPC commands remain the same. |
| `select_sketch_*` IPC | **Untouched.** Filter toggles determine which entities appear in `viewport_state`, but the commands don't change. |
| `SketchLine.constraint` | **Extended.** The inline `constraint` field stays for horizontal/vertical. New constraint types go into a separate array. |
| `SketchLineRelation` | **Absorbed.** The existing relation struct maps naturally to the new constraint model. Backward-compat serialization. |
| `ViewportSketchConstraintPrimitive` | **Reused.** Already carries all fields needed (id, kind, entity_id, related_entity_id, position, is_selected). |
| `resolveSnappedSketchPoint()` | **Migrated to C++** over Phases 1-2. TS function becomes a renderer-side entry point delegating to core snap data. |
| `sketch_constraints` in ViewportState | **Reused as-is.** The pipeline already builds and emits constraint primitives. |
| `is_construction` flag | **Integrated with filter.** Toggling "Construction" in the panel hides/shows construction entities in viewport. |

---

## 2. Implementation Architecture

### 2.1 Boundary decision

The constraint solver and snap engine live in the **C++ core**, not in
React. This follows PolySmith's existing boundary:

- C++ core owns: constraint graph, solver, snap resolution, selection
  filter state
- React owns: settings UI (checkboxes), hover feedback, viewport
  rendering
- IPC carries: filter configuration down, selection/snap results up

```
User checks a checkbox in Settings Panel
  → IPC: update_selection_filter({ endpoints: true, midpoints: false, ... })
    → C++: stores filter in DocumentState or session state
    → C++: uses filter to resolve snaps during drawing
    → C++: uses filter to determine selectable entities
    → IPC: viewport_state returns filtered entities + snap candidates
  → React: renders highlights from viewport_state
```

### 2.2 Constraint solver choice

The industry standard uses one of two approaches:

| Approach | How it works | Best for |
|---|---|---|
| **Newton-Raphson / numerical** | Iterative solve of a system of equations | General-purpose, handles mixed geometric + dimensional |
| **Graph-based / DOF counting** | Walk constraint graph, solve subgraphs topologically | Fast, predictable, easier to debug |

For PolySmith v1, a **hybrid approach** is recommended:

1. **DOF counting pass** — walk the sketch graph to determine which
   subgraphs are fully/under/over-constrained
2. **Topological solve** — for pure geometric constraints (coincident,
   horizontal, vertical, parallel, perpendicular, tangent), use direct
   geometric computation in dependency order
3. **Numerical solve** — for dimensional constraints (distances, angles)
   where direct geometric computation is not possible, use a simple
   Newton solver on the affected subgraph

This avoids pulling in a full general-purpose solver library while
covering all the constraints in the TODO list.

### 2.3 Data structures

**Build on existing, don't replace.** The C++ core already has constraint
primitives. This plan extends them:

**Existing — preserved as-is:**
```cpp
// Already in feature.h — horizontal/vertical on individual lines
struct SketchLine {
    // ...
    std::optional<std::string> constraint;  // "horizontal" | "vertical"
};

// Already in feature.h — pairwise relations between lines
struct SketchLineRelation {
    std::string id;
    std::string kind;                // "equal_length" | "perpendicular"
    std::string first_line_id;
    std::string second_line_id;
};

// Already in feature.h — point anchors stored in SketchFeatureParameters
std::vector<SketchMidpointAnchor> midpoint_anchors;
std::vector<SketchPointLineAnchor> point_line_anchors;
```

**New — added alongside existing:**
```cpp
// A general constraint (for types not covered by SketchLineRelation)
struct SketchConstraint {
    std::string constraint_id;       // "constraint-{N}"
    std::string kind;
    std::vector<std::string> target_ids;
    double value = 0.0;             // for dimensional constraints
    bool driven = false;
};

// Selection filter — new, no conflicts
struct SelectionFilter {
    bool select_curves        = true;
    bool select_points        = true;
    bool select_construction  = true;
    bool select_constraints   = true;

    bool snap_endpoint        = true;
    bool snap_midpoint        = true;
    bool snap_center          = true;
    bool snap_intersection    = true;
    bool snap_nearest         = true;
    bool snap_quadrant        = false;
    bool snap_perpendicular   = false;
    bool snap_parallel        = false;
    bool snap_tangent         = true;
    bool snap_grid            = true;

    int tolerance_px           = 10;
    std::vector<std::string> snap_priority;
    bool magnetic_pull         = true;
};

// Add to SketchFeatureParameters
struct SketchFeatureParameters {
    // ... existing fields preserved ...
    std::vector<SketchConstraint> constraints;   // NEW
};
```

**Migration path for SketchLineRelation:** The existing
`line_relations[]` vector stays during Phase 1. New constraint types
(`coincident`, `parallel`, `distance`, etc.) are added to the new
`constraints[]` vector. In a later phase, `SketchLineRelation` entries
can be merged into `constraints[]` for a single unified representation.
All serialization is backward-compatible.

**Migration path for snap resolution:** The existing TS-side
`resolveSnappedSketchPoint()` continues to work during Phase 1 while the
C++ snap engine is built. In Phase 2, the viewport state gains a
`snap_candidates[]` array emitted by C++. The TS function switches from
computing candidates locally to reading them from `viewport_state`.
The TS function's signature and call sites do not change.

### 2.4 Solver engine design

The solver runs inside `refresh_sketch_derived_state`, after point
rebuilding and before dimension re-evaluation:

```
refresh_sketch_derived_state(feature):
    1. rebuild_sketch_points()          // existing
    2. apply_geometric_constraints()    // NEW — phase 1 solve
    3. apply_dimensional_constraints()  // NEW — phase 2 solve
    4. sync_driven_dimensions()         // existing
    5. rebuild_sketch_profiles()        // existing
```

**Phase 1 — Geometric constraints (topological solve):**
- Build adjacency graph from `constraints[]` + `line_relations[]` +
  inline `SketchLine.constraint` fields + `midpoint_anchors[]` +
  `point_line_anchors[]`
- Topologically sort subgraphs
- For each subgraph:
  - Apply coincident → merge points
  - Apply horizontal/vertical → set coordinate
  - Apply parallel/perpendicular → adjust angle
  - Apply tangent → geometric tangent placement
  - Apply midpoint/on-line anchors → project anchored point
  - Apply equal-length → compute and set shared length
- Each step is a direct geometric computation

**Phase 2 — Dimensional constraints (numerical solve):**
- Collect dimensional constraints (distance, radius, angle)
- For each constraint:
  - If the affected subgraph is fully constrained and has 1 DOF left,
    solve directly
  - Otherwise, run a bounded Newton iteration
- Convergence tolerance: 1e-6 mm, max 50 iterations
- On failure: leave geometry unchanged, flag `dependency_broken`

### 2.5 Inference engine (auto-constrain at creation time)

Inference runs at the moment a sketch entity is committed. The existing
horizontal/vertical inference already works — this extends it:

```
on_sketch_entity_committed(entity):
    // 1. Check endpoint proximity to existing points
    for each endpoint in entity:
        nearest = find_nearest_point(endpoint, tolerance)
        if nearest within tolerance:
            add_coincident_constraint(endpoint, nearest)

    // 2. Check orientation for horizontal/vertical
    //    (already works via SketchLine.constraint — no change needed)
    if entity is line:
        if abs(dy) < angle_tolerance:
            set line.constraint = "horizontal"   // existing mechanism
        else if abs(dx) < angle_tolerance:
            set line.constraint = "vertical"     // existing mechanism

    // 3. Check tangency to arcs/circles
    if entity is line and snap was tangent:
        add_tangent_constraint(line, arc)
```

### 2.6 Snap resolution engine

The snap engine runs on every mouse move during active drawing. In Phase
1 the TS-side `resolveSnappedSketchPoint()` stays; in Phase 2 the C++
core emits snap candidates in `viewport_state`:

```
resolve_snap(cursor_pos, active_tool, filter):
    candidates = []

    if filter.snap_endpoint:
        for each enabled entity type:
            find nearest endpoint → add to candidates
    if filter.snap_midpoint:
        for each enabled entity type:
            find nearest midpoint → add to candidates
    // ... all snap types ...

    candidates.sort(priority_order, distance)
    return candidates[0] if distance < tolerance_px else null
```

The priority order (configurable, default shown):
1. Endpoint
2. Center
3. Midpoint
4. Intersection
5. Quadrant
6. Perpendicular
7. Tangent
8. Nearest

---

## 3. Settings UI Design

### 3.1 Panel structure

A single "Selection & Snap" settings panel, accessible from the app
header (gear icon). Three sections:

```
+-------------------------------------+
|  Selection & Snap Filter            |
|                                     |
|  -- Sketch Geometry --------------  |
|  [x] Curves (lines, arcs, circles)  |
|  [x] Points (endpoints, centers)    |
|  [x] Construction geometry          |
|  [x] Constraints (click to edit)    |
|                                     |
|  -- Snap Types -------------------  |
|  [x] Endpoint      [ ] Quadrant     |
|  [x] Midpoint      [ ] Perpendic.   |
|  [x] Center        [ ] Parallel     |
|  [x] Intersection  [x] Tangent      |
|  [x] Nearest       [x] Grid         |
|                                     |
|  -- 3D Snaps (future) ------------  |
|  [x] Vertex        [x] Face plan.   |
|  [x] Edge          [ ] Face offset  |
|                                     |
|  -- Global -----------------------  |
|  Tolerance: [10 px ========     ]   |
|  [x] Magnetic pull                  |
|  [x] Snap indicator                 |
|  [x] Dynamic highlight              |
|  Snap priority: [reorderable list]  |
+-------------------------------------+
```

### 3.2 Where the settings live

**Option A (v1):** Per-session, persisted in `localStorage`. Simple,
no IPC needed for persistence.

**Option B (later):** Per-document, stored in `DocumentState`. Allows
different filter configurations per project.

v1 should use Option A for speed. The filter is sent to the core via
a single `update_selection_filter` IPC command on every change.

---

## 4. Implementation Phases

### Phase 1 — Foundation (C++ core + minimal UI)

**Goal:** constraint solver + snap engine working, no UI checkboxes yet.

| Step | Files | Description |
|---|---|---|
| 1.1 | `feature.h` | Add `SketchConstraint` struct, `SelectionFilter` struct, `constraints` vector to `SketchFeatureParameters`. **Preserve** `SketchLineRelation`, `SketchLine.constraint`, and all anchor structs. |
| 1.2 | `constraint_solver.h/.cpp` | **NEW** — topological geometric constraint solver. Reads from `constraints[]` **and** `line_relations[]` **and** inline `SketchLine.constraint`. |
| 1.3 | `snap_engine.h/.cpp` | **NEW** — snap resolution engine. |
| 1.4 | `sketch_feature.cpp` | Wire `apply_geometric_constraints()` into `refresh_sketch_derived_state`. |
| 1.5 | `document.h/.cpp` | `add_constraint`, `delete_constraint` methods with undo/redo. |
| 1.6 | `app.cpp` | `add_constraint`, `delete_constraint` IPC handlers. |
| 1.7 | `commands.schema.json` | Register `add_constraint`, `delete_constraint`. |
| 1.8 | `serialization.cpp` | Serialize/deserialize `constraints[]`. **Keep** existing `line_relations` serialization. |
| 1.9 | TS types + IPC + hooks | Standard wiring for constraint CRUD. |
| 1.10 | `CMakeLists.txt` | Add new source files. |

**Exit criteria:** Coincident, horizontal, and vertical constraints work
end-to-end. Drawing a line near horizontal snaps to exactly horizontal
and a constraint icon appears. **All existing selection IPC, sketch
entity selection, and `SketchLineRelation` functionality is untouched.**

### Phase 2 — Snap Migration & Inference

**Goal:** move snap resolution to C++, auto-detect constraints.

| Step | Files | Description |
|---|---|---|
| 2.1 | `viewport.h/.cpp` | Emit `snap_candidates[]` in viewport state. |
| 2.2 | `ViewportPanel.tsx` | Switch `resolveSnappedSketchPoint()` to read from `viewport_state.snap_candidates` instead of computing locally. |
| 2.3 | `inference_engine.h/.cpp` | **NEW** — proximity + orientation detection. |
| 2.4 | `sketch_feature.cpp` | Call inference after each non-construction entity commit. |

**Exit criteria:** TS-side snap resolution reads from core state.
Coincident constraint auto-created on endpoint snap.

### Phase 3 — Dimensional Constraints

**Goal:** distance, radius, and angle constraints.

| Step | Files | Description |
|---|---|---|
| 3.1 | `constraint_solver.cpp` | Extend with dimensional constraint support (Newton solver). |
| 3.2 | `sketch_feature.cpp` | Wire `apply_dimensional_constraints()`. |
| 3.3 | `document.h/.cpp` | `update_constraint_value` method. |
| 3.4 | UI | Constraint editor (click constraint → inline value edit). |

### Phase 4 — Selection Filter UI

| Step | Files | Description |
|---|---|---|
| 4.1 | `SelectionFilterPanel.tsx` | **NEW** — checkbox panel. |
| 4.2 | `settingsStore.ts` | Persist filter to localStorage. |
| 4.3 | `useCadCore.ts` | `updateSelectionFilter` hook. |
| 4.4 | `app.cpp` | `update_selection_filter` IPC handler. |
| 4.5 | `app_header.tsx` | Settings gear icon. |

**Exit criteria:** Unchecking "Endpoint" disables endpoint snaps
during active drawing. No checkboxes exist yet.

### Phase 5 — Polish & Edge Cases

| Step | Description |
|---|---|
| 5.1 | Over-constraint detection + UI warning. |
| 5.2 | Constraint deletion UI (click constraint → Delete). |
| 5.3 | DOF coloring (blue = under, black = fully, red = over). |
| 5.4 | Unify `SketchLineRelation` into `constraints[]` (backward-compat serialization). |
| 5.5 | Driven dimension proposal: when the user tries to add a dimension to an already-fully-constrained entity, offer to create a driven (reference) dimension instead. |
| 5.6 | Help menu / legend for DOF colors (blue = fully constrained, red = over-constrained, yellow = under-constrained). |
| 5.7 | Constraint badge hover/selection highlight: the badge sprite should change color or glow when hovered/selected. Currently only the status bar text confirms selection. |

---

## 5. Files Changed (Full Projection)

### C++ Core
| File | Change |
|---|---|
| `native/cad-core/src/core/feature.h` | `SketchConstraint`, `SelectionFilter`, `constraints` in `SketchFeatureParameters`. **Preserve** existing structs. |
| `native/cad-core/src/core/constraint_solver.h` | **NEW** — solver header |
| `native/cad-core/src/core/constraint_solver.cpp` | **NEW** — geometric + dimensional solver |
| `native/cad-core/src/core/snap_engine.h` | **NEW** — snap resolution header |
| `native/cad-core/src/core/snap_engine.cpp` | **NEW** — snap resolution impl |
| `native/cad-core/src/core/inference_engine.h` | **NEW** — auto-constrain header |
| `native/cad-core/src/core/inference_engine.cpp` | **NEW** — auto-constrain impl |
| `native/cad-core/src/core/sketch_feature.h` | Declare constraint helpers |
| `native/cad-core/src/core/sketch_feature.cpp` | Wire solver into refresh pass, call inference |
| `native/cad-core/src/core/document.h` | `add_constraint`, `delete_constraint`, `update_constraint_value`, `update_selection_filter` |
| `native/cad-core/src/core/document.cpp` | Implement with undo/redo |
| `native/cad-core/src/app.cpp` | Register new command handlers |
| `native/cad-core/src/core/viewport.h` | Add `snap_candidates[]` to ViewportState |
| `native/cad-core/src/core/viewport.cpp` | Emit snap candidates + filtered entity lists |
| `native/cad-core/src/protocol/serialization.cpp` | Serialize `constraints[]` + filter. **Preserve** `line_relations` serialization. |
| `native/cad-core/CMakeLists.txt` | Add new source files |

### Protocol
| File | Change |
|---|---|
| `protocol/schema/commands.schema.json` | `add_constraint`, `delete_constraint`, `update_constraint_value`, `update_selection_filter` |

### TypeScript
| File | Change |
|---|---|
| `apps/desktop-ui/src/types/ipc.ts` | Constraint + filter command interfaces |
| `apps/desktop-ui/src/types/geometry/sketch.ts` | `SketchConstraintEntry`, `SelectionFilter` types |
| `apps/desktop-ui/src/lib/ipcProtocol.ts` | Command builders |
| `apps/desktop-ui/src/hooks/useCadCore.ts` | Constraint + filter hooks |
| `apps/desktop-ui/src/lib/schemas/ipcSchema.ts` | Zod schemas |

### UI
| File | Change |
|---|---|
| `apps/desktop-ui/src/layout/SelectionFilterPanel.tsx` | **NEW** — checkbox panel |
| `apps/desktop-ui/src/state/settingsStore.ts` | **NEW** or extend — filter persistence |
| `apps/desktop-ui/src/layout/ViewportPanel.tsx` | Switch snap resolution to read from core state. **Preserve** `resolveSnappedSketchPoint()` signature. |
| `apps/desktop-ui/src/layout/header/AppHeader.tsx` | Settings button |
| `apps/desktop-ui/src/i18n/en.json` | Constraint + filter strings |

### Documentation
| File | Change |
|---|---|
| `IPC-Protocol` | Document new commands |
| `AI-CAD-Command-Language` | Document constraint commands |

---

## 6. Design Decisions & Trade-offs

### Why solver in C++, not a library

PolySmith already links OpenCascade, and a general-purpose constraint
solver is a heavy dependency with its own build complexity. A hand-rolled
solver for PolySmith's constraint set is ~1500-2000 lines of C++ and
avoids a new third-party build target. The existing `SketchLineRelation`
and `SketchLine.constraint` machinery means the solver starts with
real data to walk, not from zero.

### Why selection filter is session-scoped (v1)

Per-document filter persistence means every `.polysmith` file carries
filter state — but the filter is really a user preference, not document
geometry. Session-scoped (`localStorage`) keeps the document format
clean and is sufficient for v1.

### Why constraint solving runs in the refresh pass

Constraint solving is a form of recompute — it depends on upstream
geometry and produces output geometry. Running it inside
`refresh_sketch_derived_state` means it automatically benefits from
undo/redo, the dependency walker, and the existing bump/revision
machinery.

### Why build on existing, not replace

The C++ core already has `SketchLineRelation`, inline `constraint`
fields, `midpoint_anchors`, `point_line_anchors`, and a full
`ViewportSketchConstraintPrimitive` rendering pipeline. Replacing these
would break existing sketch behavior (horizontal/vertical constraint
enforcement, midpoint snaps, on-line anchors) and create unnecessary
churn in the viewport renderer. Extending them is lower risk and faster.

### Over-constraint handling

The industry standard approach is:
1. Detect over-constrained subgraphs during DOF counting
2. Highlight them in red
3. Let the user delete or suppress constraints until the sketch resolves

PolySmith v1 will detect over-constraint and emit a warning. The solver
solves the largest consistent subset and leaves the rest flagged.

### Snap migration: TS → C++

The existing `resolveSnappedSketchPoint()` runs entirely in TypeScript.
Moving snap resolution to C++ is the right long-term call (it's geometry
logic, not presentation), but the migration is staged:
- Phase 1: C++ builds the snap engine internally; TS function unchanged.
- Phase 2: C++ emits `snap_candidates[]` in viewport state; TS reads from
  it instead of computing locally.
- The TS function's call sites (5 in `ViewportPanel.tsx`) are not
  restructured — only the data source changes.

---

## 7. Rectangle Selection (Planning — 2026-05-26)

### Goal

Add rectangular drag selection in the sketch viewport following CAD
industry standard:
- **Left → Right drag (window):** selects entities fully inside the rectangle
- **Right → Left drag (crossing):** selects entities touching or crossing the rectangle

### Implementation approach

All frontend — no core or IPC changes needed:

- Use existing `clear_selection` + `select_sketch_entity { additive: true }` commands
- Selection rectangle rendered as an HTML `<div>` overlay (fast, no Three.js overhead)
- Entity screen positions computed via `projectWorldPointToViewport()`
- 2D rectangle hit testing on mouse-up

### Selection rules per entity

| Entity | Window (L→R) | Crossing (R→L) |
|---|---|---|
| Line | Both screen-space endpoints inside | Any endpoint inside, or segment-crossing |
| Circle | Entire bounding box inside | Center or any quadrant point inside |
| Arc | Both endpoints + midpoint inside | Any endpoint or arc point inside |
| Point | Point inside | Point inside |
| Polygon | All vertices inside | Any vertex inside or edge-crossing |

### Files

- `ViewportPanel.tsx` — all changes (drag state, overlay div, hit test logic)
- No new dependencies, no core changes, no schema changes

## 8. Object Snap Tracking (Planned — 2026-05-27)

### Goal

Project temporary guide lines from existing geometry endpoints when the
user briefly hovers over them while drawing. The guide lines extend along
cardinal (horizontal/vertical) axes, and the active draft snaps to the
intersection of the guide and the rubber-band direction.

### Use case

An existing vertical line A has its top endpoint at Y=5. The user starts
a new vertical line B at X=3, briefly hovers near A's top endpoint
(acquiring it), then drags upward. A horizontal dotted guide line extends
from Y=5. Line B snaps to end at exactly Y=5, matching line A's height
without an explicit dimension.

This also works vertically: hovering near the endpoint of a horizontal
line while drawing another horizontal line snaps the X coordinate.

### Implementation approach

All frontend — no core or IPC changes needed:

1. **Acquire phase** — during pointer-move while a draft is active,
   detect when the cursor is within `SKETCH_SNAP_DISTANCE` of any
   existing sketch entity endpoint or center. Store the matching point
   in a transient `trackedPoints[]` list.

2. **Render phase** — for each tracked point, draw a dashed horizontal
   and vertical guide line from the point to a generous bounding box,
   using a dedicated dashed-line Three.js material. Show only the
   axis-aligned guides.

3. **Snap phase** — in `resolveSnappedSketchPoint()`, for each tracked
   point, compute the intersection of the rubber-band ray (start →
   cursor) with each guide line. If within tolerance, snap to that
   intersection.

4. **Cleanup** — clear tracked points when the draft tool commits or
   cancels (on `add_sketch_line` IPC or Escape).

## 8. Related Pages

- [2D Sketch System Architecture](2D-Sketch-System-Architecture) — design overview
- [2D Sketch Constraint System — Implementation TODO](2D-Sketch-Constraint-System) — constraint list
- [Snap Settings — Configuration Options](Snap-Settings-%E2%80%90-Configuration-Options) — snap types reference