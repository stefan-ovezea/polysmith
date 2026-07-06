# Vertex Unification Migration Plan

> Target schema: `deepseek_json_20260706_beba85.json`
> Goal: unify all sketch point kinds (`endpoint`, `center`, `quadrant`, `projected`) into a single `vertices[]` array with clean `vertex-N` IDs, explicit `geometry_owner_ids`, and no coordinate duplication.

---

## Phase 0: CAM data structures ✅ _(completed 2026-07-06)_

Add empty/typed structs for CAM fields from the target schema. These are purely additive — no existing code touches them.

### 0.1: C++ CAM structs
- **File:** `native/cad-core/src/core/cam/` (new directory)
- `cam_types.h`: `CamSetup`, `ToolEntry`, `CamOperation`, `GeometryReference`, `Attestation`, `ToolpathCache`
- Add `std::vector<CamSetup> cam_setups` to `DocumentState`
- Add `std::vector<ToolEntry> tool_library` to `DocumentState`
- Add `std::vector<CamOperation> cam_operations` to `DocumentState`

### 0.2: IPC schema
- **File:** `protocol/schema/events.schema.json` — add CAM fields to document state
- **File:** `apps/desktop-ui/src/lib/schemas/ipc/documentStateSchema.ts` — Zod types

### 0.3: TS types
- **File:** `apps/desktop-ui/src/types/geometry/cam.ts` — mirror C++ types

---

## Phase 1: Add `geometry_owner_ids` and projection fields (additive, zero risk)

Add new fields to `SketchPoint` without changing existing behavior. Existing code ignores them.

### 1.1: C++ struct
- **File:** `native/cad-core/src/core/sketch/sketch_geometry_types.h`
- Add to `SketchPoint`:
  - `std::vector<std::string> geometry_owner_ids` (empty default)
  - `bool is_projected = false`
  - `std::optional<std::string> source_type` (e.g. `"edge_midpoint"`)
  - `std::optional<std::string> source_feature_id`
  - `std::optional<std::string> source_edge_id`

### 1.2: Populate in `rebuild_sketch_points`
- **File:** `native/cad-core/src/core/sketch/impl/private_point_profile_helpers.inc`
- For projected points: set `is_projected = true`, `source_type`, `source_feature_id`, `source_edge_id`
- For all other points: leave `geometry_owner_ids` empty for now (Phase 2 fills it)
- For projected points: set `geometry_owner_ids = [owning_line_id]` if the projection generates a line

### 1.3: C++ viewport emit
- **File:** `native/cad-core/src/core/viewport/impl/sketch_primitives.inc`
- Pass `geometry_owner_ids`, `is_projected`, `source_*` in `ViewportSketchPointPrimitive`
- Add fields to `ViewportSketchPointPrimitive` in `viewport_sketch_primitives.h`

### 1.4: TS viewport types
- **File:** `apps/desktop-ui/src/types/viewport.ts` — optional fields
- **File:** `apps/desktop-ui/src/lib/viewportScene.ts` — map new fields
- **File:** `apps/desktop-ui/src/lib/schemas/ipc/viewportStateSchema.ts` — Zod optional fields

---

## Phase 2: Populate `geometry_owner_ids` for all points

### 2.1: During rebuild, write owner IDs
- **File:** `native/cad-core/src/core/sketch/impl/private_point_profile_helpers.inc`
- Line endpoints → `geometry_owner_ids: [line_id]`
- Shared endpoints → `geometry_owner_ids: [line_id_1, line_id_2]` (or arc_id)
- Circle centers → `geometry_owner_ids: [circle_id]`
- Arc centers → `geometry_owner_ids: [arc_id]`
- Quadrants → `geometry_owner_ids: [circle_id]`

### 2.2: Use `geometry_owner_ids` in TS fallback
- **File:** `apps/desktop-ui/src/layout/viewport/dimensionToolPicking.ts`
- In `entityIdFromSketchPointId`: if `geometryOwnerIds` is non-empty and regex fails, return `geometryOwnerIds[0]`
- Keeps regex path for backward compat during transition

---

## Phase 3: Add `constraint_id` link to dimensions

### 3.1: C++ struct
- **File:** `native/cad-core/src/core/sketch/sketch_dimension_types.h`
- Add `std::string constraint_id` to `SketchDimension` (empty default)

### 3.2: Populate during dimension creation
- When creating a dimension that maps to a constraint, store the constraint ID

---

## Phase 4: Rename point IDs to `vertex-N`

**Biggest change.** Touches line creation, arc creation, circle creation, point reconciliation, profile detection, and all regex-based point ID parsing.

### 4.1: C++ — change ID generation
- **Files:**
  - `line_entity_commands.inc` — `vertex-N` instead of `point-line-N-start/end`
  - `curve_primitives.inc` — `vertex-N` for arc endpoints + center
  - Circles: `vertex-N` for center (was `point-circle-N-center`)
  - Quadrants: `vertex-N` for quadrant points
- Add a shared `next_vertex_index` counter to `SketchFeatureParameters`

### 4.2: C++ — remove regex-based point ID parsing
- **File:** `sketch_line_point_update_commands.inc` — replace `point-circle-`/`point-arc-` prefix checks with `geometry_owner_ids` lookups
- **File:** `constraint_solver_entity_mapping.inc` — use `geometry_owner_ids` instead of ID prefix matching
- **File:** `private_sketch_point_lookup_helpers.inc` — all find-by-ID functions still work (ID is just a string)
- **File:** `dimension_update.inc`, `dimension_angle_update.inc` — arc center resolution

### 4.3: TS — update dimension tool
- **File:** `dimensionToolPicking.ts` — remove regex fallback paths, rely on `geometryOwnerIds`
- **File:** `endpointDrag.ts` — update `endpointDragAnchors` to use `geometryOwnerIds`

### 4.4: TS — update types
- **File:** `types/geometry/sketch.ts` — `point_id` → `vertex_id` in entries
- **File:** `types/scene.ts`, `types/viewport.ts` — update field names

---

## Phase 5: Remove coordinate duplication

### 5.1: Remove `start_x/y, end_x/y` from `SketchLine`
- **File:** `sketch_geometry_types.h` — drop coordinate fields
- Coordinates live ONLY on vertices
- The solver reads from vertices, applies to vertices

### 5.2: Remove `center_x/y` from `SketchArc`
- Arcs reference `center_vertex_id` instead of cached doubles
- `start_x/y, end_x/y` also removed — vertices hold coordinates

### 5.3: Remove `center_x/y` from `SketchCircle`
- Circles reference `center_vertex_id` instead of cached doubles

### 5.4: Update all consumers
- Everywhere that reads `line.start_x` → read from vertex instead
- Everywhere that reads `arc.center_x` → read from vertex instead

---

## Phase 6: Rename `points[]` → `vertices[]`

Pure search-and-replace.

### 6.1: C++ rename
- `SketchFeatureParameters::points` → `vertices`
- `SketchPoint` → `SketchVertex`
- All `parameters.points` → `parameters.vertices`
- All `find_sketch_point` → `find_sketch_vertex`

### 6.2: TS rename
- `SketchPointEntry` → `SketchVertexEntry`
- `SketchPointScene` → `SketchVertexScene`
- All `pointId` → `vertexId` in scene types

---

## Implementation order & dependencies

```
Phase 0 ──► independent, can ship anytime
Phase 1 ──► independent, can ship anytime
Phase 2 ──► depends on Phase 1
Phase 3 ──► independent
Phase 4 ──► depends on Phase 2 (needs geometry_owner_ids)
Phase 5 ──► depends on Phase 4
Phase 6 ──► depends on Phase 4-5
```

## Target schema reference

Source: `deepseek_json_20260706_beba85.json` — vertices, lines, arcs, circles, constraints, dimensions, features, CAM setup, tool library, CAM operations.
