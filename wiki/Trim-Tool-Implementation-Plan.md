# Trim Tool — Implementation Plan

> **Status:** Shipped (2026-05-24 through 2026-05-31). Line, circle→arc, and arc
> trimming are implemented in `trim_engine.cpp`. Core-driven hover preview via
> `trim_preview` IPC. Constraint re-evaluation and polygon dissolve are handled.
> Zero-length segment guards, arc-endpoint share-point-id fix, and intersection
> filtering landed in the 2026-05-31 stabilisation pass. This plan is retained as
> reference documentation — the implementation matches the design below.
> **Priority:** Done — bug fixes only.

## 1. What Trim Does in CAD

The Trim tool removes portions of sketch curves by cutting them at intersection
points with other curves. The user clicks on a segment they want to **delete**;
that segment is removed and the entity is shortened or split accordingly.

**Example:** Three lines forming a cross. Click the Trim tool, click the top arm
of the vertical line above the horizontal line → the top arm is deleted, the
vertical line is shortened to the intersection point. The horizontal line and
its segments are NOT touched. Only the clicked segment on the target entity
is removed.

**One trim operation affects exactly one entity at a time.** Trim does not
touch other entities — it only uses them as "cutting edges."

## 2. Core Algorithm

### 2.1 Intersection Detection

Given a target entity `E` (the one clicked) and all other non-construction
entities `{E1, E2, ...}`, find every intersection point between `E` and each
`Ei`. The entity kind pairs determine the intersection algorithm:

| Target | Other | Algorithm |
|---|---|---|
| Line | Line | 2D segment-segment intersection with parameterization. Return `t` along each segment. Exclude if `t` outside (0, 1). |
| Line | Circle | Line-circle intersection. Solve quadratic for `t` on the line. Keep solutions where the point lies within the line segment. |
| Line | Arc | Same as line-circle, then test if the intersection point's angle falls within the arc's sweep. |
| Circle | Circle | Circle-circle intersection. Solve for up to 2 points. |
| Circle | Arc | Circle-circle, filter by arc sweep. |
| Arc | Arc | Same as circle-circle with dual sweep filtering. |

All intersection computations use a configurable coincident-point tolerance
(`kCoincidentTolerance = 1e-6 mm`) for deduplication. Intersections within
this distance of each other are merged into a single point.

**Tangent intersections** (one point, touching at a single location): included.
These produce a split at the tangent point — the resulting segment has zero
length and should be deleted.

**Self-intersection:** Excluded. Don't test the entity against itself.

### 2.2 Splitting the Entity

After finding all `N` intersection points, sort them by their parameter along
the entity's parametric space:

| Entity | Parameter |
|---|---|
| Line | `t` in [0, 1] along the segment from start to end |
| Circle | Angular position in [0, 2π) from positive X axis |
| Arc | Angular position within the arc's sweep range |

This produces `N+1` segments (or `N` segments for closed curves like circles).
For a circle with 2 intersections, there are 2 candidate arcs.

### 2.3 Deleting the Clicked Segment

Determine which segment the click point falls on. Two methods:

**Primary (parameter-based):** Compute the click position's parameter along the
entity. Find which of the `N+1` parameter ranges contains it.

**Secondary (distance-based, fallback):** Project the click point onto each
segment and pick the one with minimum perpendicular distance.

Once identified, delete that segment. Keep all other segments. Update the
entity's stored coordinates to match the trimmed boundaries. If a middle
segment is deleted from a line, the line splits into two lines.

### 2.4 Entity Transformation

| Entity | After Trim | Implementation |
|---|---|---|
| **Line** (1 segment deleted) | Shortened line or split | If end segment deleted: update `start_x/y` or `end_x/y` to the adjacent intersection point. If middle segment deleted: original line shortened to left portion, new line created for right portion. Update point IDs. Delete old endpoint points no longer referenced. |
| **Line** (0 intersections) | Deleted | Remove line, constraints, and dimensions referencing it. |
| **Circle** (0 intersections) | Deleted | Remove circle, constraints, and dimensions referencing it. |
| **Circle** (1 arc deleted, 1 intersection = tangent) | No-op (degenerate) | Do nothing — a circle with a single tangent intersection remains a full circle. Silently exit. |
| **Circle** (1 arc deleted, 2+ intersections) | Arc | Create a `SketchArc` from the complementary (non-deleted) arc. Delete the `SketchCircle`. All dimensions/constraints referencing the circle become `dependency_broken`. |
| **Arc** (1 segment deleted) | Shortened arc | Update `start_x/y`, `end_x/y`, and the cached angular endpoints. Update point IDs. |
| **Polygon line** | Shortened line or deleted | Treated as a regular line. The polygon record is NOT updated — the polygon becomes a collection of independent lines. The polygon radius dimension becomes `dependency_broken`. |

**Key rule:** When a circle is converted to an arc, ALL constraints on that
circle become `dependency_broken`. The user must manually re-add constraints
to the arc. This is how professional CAD systems handle the type change.

## 3. Architecture

### 3.1 New Native Module: `trim_engine`

```
native/cad-core/src/core/trim_engine.h
native/cad-core/src/core/trim_engine.cpp
```

**Responsibilities:**
- `find_all_intersections(target_entity, all_other_entities) → vector<IntersectionPoint>`
- `split_entity_at_intersections(entity, intersections) → vector<EntitySegment>`
- `select_clicked_segment(segments, click_position) → segment_index`
- `apply_trim(feature, entity_id, kept_segment) → void`

```cpp
struct IntersectionPoint {
    double x;          // sketch-local coordinate
    double y;
    double param_on_target;   // parameter along target entity
    double param_on_other;    // parameter along other entity
    std::string other_entity_id;
};

struct EntitySegment {
    enum Kind { LINE_SEGMENT, ARC_SEGMENT };
    Kind kind;
    double param_start;
    double param_end;
    // Cached coordinates
    double start_x, start_y;
    double end_x, end_y;
    // For arcs: center and radius from the source entity
    double center_x, center_y, radius;
    bool ccw;
};
```

### 3.2 Integration Points

| Layer | File | Change |
|---|---|---|
| **C++ — Feature structs** | `feature.h` | No changes needed. Existing structs are sufficient — trim mutates them in place. |
| **C++ — Trim engine** | `trim_engine.h/.cpp` | **NEW** |
| **C++ — Sketch feature** | `sketch_feature.h/.cpp` | Add `trim_sketch_entity(feature, entity_id, keep_start, keep_end)` |
| **C++ — Document** | `document.h/.cpp` | Add `trim_sketch_entity(entity_id, click_x, click_y)` with undo/redo |
| **C++ — App** | `app.cpp` | Register `trim_sketch_entity` command handler |
| **Protocol** | `commands.schema.json` | Add `trim_sketch_entity` to command enum |
| **TS — Types** | `types/ipc.ts` | Add `TrimSketchEntityCommand` |
| **TS — IPC builder** | `lib/ipcProtocol.ts` | `makeTrimSketchEntityCommand` |
| **TS — Hooks** | `hooks/useCadCore.ts` | `trimSketchEntity` hook |
| **TS — Viewport** | `ViewportPanel.tsx` | Trim tool entry in toolbar + click handler |
| **TS — Toolbar** | `SketchToolbar.tsx` | Trim tool button |
| **CMake** | `CMakeLists.txt` | Add `trim_engine.cpp` |

### 3.3 Undo/Redo

Trim is a single undoable action. The document layer captures the full sketch
state before the trim and pushes it onto the undo stack. On undo, the entire
sketch is restored. This is simpler than trying to reverse the trim operation
piecemeal.

## 4. Constraint and Dimension Handling

### 4.1 Lines

| Constraint | Fate after trim |
|---|---|
| `horizontal` / `vertical` on the line | Re-computed: if the trimmed line is still axis-aligned (within tolerance), constraint stays. Otherwise, removed. |
| `equal_length` with another line | `dependency_broken` — the length changed. |
| `perpendicular` / `parallel` relation | Re-evaluated: if directions still match within tolerance, kept. Otherwise removed. |
| Coincident constraints on trimmed-away endpoints | Deleted. |
| Dimensions referencing the line (`dim-line-*`) | Value re-measured from the new length. Expression stays. |

### 4.2 Circles → Arcs

| Constraint | Fate |
|---|---|
| All constraints on the circle | `dependency_broken` — entity type changed. |
| `circle_radius` dimension | Deleted — arcs use a different dimension kind. |
| `circle_center_distance` dimension | `dependency_broken` — source entity type changed. |
| `concentric` with another circle | `dependency_broken`. |

### 4.3 Arcs

| Constraint | Fate |
|---|---|
| Arc endpoint coincident constraints | Re-evaluated: kept if the new endpoints still match. |
| Tangent constraints involving the arc | Re-evaluated against the new arc geometry. |
| `dependency_broken` on all others | Conservative default — arc constraints are not yet fully implemented in v1. |

### 4.4 Polygons

When a polygon line is trimmed, the polygon is dissolved into its constituent
lines. The polygon record (`SketchPolygon`) and its `polygon_radius` dimension
are deleted. The remaining polygon lines become independent sketch entities.

## 5. UI Design

### 5.1 Toolbar Entry

- **Icon:** Scissors or trim glyph (standard CAD convention)
- **Hotkey:** `T`
- **Label:** "Trim" / `translate("toolbar.trim")`

### 5.2 Click Behavior

1. User activates Trim tool (`T`)
2. Cursor changes to scissors crosshair
3. User clicks on a sketch entity segment
4. The entity is trimmed at all intersection points
5. Only the clicked segment survives
6. Tool stays in trim mode for repeated trimming (CAD convention)
7. Exit with Escape or switch to another tool

### 5.3 Visual Feedback

During trim tool hover:
- The entity under the cursor highlights in a "trim preview" color
- Intersection points show as small dots/crosses
- The segment that would be deleted is highlighted

After trim:
- The entity updates immediately
- Constraints and dimensions update in the next refresh pass

### 5.4 Floating Info Panel

```
┌──────────────────────────────┐
│ Trim (T)                     │
│ Click a curve segment to     │
│ trim it at intersections     │
└──────────────────────────────┘
```

## 6. Implementation Phases

### Phase 1 — Line trimming (MVP)

| Step | Description |
|---|---|
| 1.1 | `trim_engine.h/.cpp` — intersection detection: line-line |
| 1.2 | `trim_engine` — line splitting at intersection points |
| 1.3 | `trim_engine` — segment selection by click position |
| 1.4 | `sketch_feature.cpp` — `trim_sketch_entity` for lines only |
| 1.5 | `document.cpp` — `trim_sketch_entity` with undo/redo |
| 1.6 | `app.cpp` — IPC handler |
| 1.7 | `commands.schema.json` — register command |
| 1.8 | TS types + IPC builder + hooks |
| 1.9 | `ViewportPanel.tsx` + `SketchToolbar.tsx` — UI |
| 1.10 | `CMakeLists.txt` — add new sources |

**Exit criteria:** Click a line that crosses another line → the clicked
segment is deleted, the line is shortened to the intersection point.
Constraints on the trimmed line are re-evaluated.

### Phase 2 — Circle → arc conversion

| Step | Description |
|---|---|
| 2.1 | `trim_engine` — line-circle and circle-circle intersection |
| 2.2 | `sketch_feature.cpp` — circle → arc conversion |
| 2.3 | Constraint cleanup for circles converted to arcs |

**Exit criteria:** Click a circle segment between two line crossings → the
circle becomes an arc between the two intersection points.

### Phase 3 — Arc trimming

| Step | Description |
|---|---|
| 3.1 | `trim_engine` — arc-arc, arc-line, arc-circle intersections |
| 3.2 | `sketch_feature.cpp` — arc shortening at intersections |

### Phase 4 — Polish

| Step | Description |
|---|---|
| 4.1 | Trim preview (hover highlighting + intersection dots) |
| 4.2 | Multi-click repeat mode |
| 4.3 | Tangent intersection handling |
| 4.4 | Polygon line trim → polygon dissolve |

## 7. Open Questions

1. **Should trim also support clicking on the portion to DELETE (power trim)?**
   - Power trim: click → drag → crosses multiple entities → deletes all
     intersected segments. v1 scope: single-click trim only.

2. **Should construction lines be trimmable?**
   - Yes. Construction lines participate in the same intersection set.

3. **Should projected entities be trimmable?**
   - Probably not in v1. Trimming a projected entity would break the
     projection link. If the user needs to modify a projected outline, they
     should delete the projection and draw manually.

4. **What happens to a circle with 0 intersections?**
   - Nothing. Trim is a no-op on an entity with no intersections.

5. **What happens to a circle with exactly 1 intersection?**
   - The intersection is a tangent point. The circle remains whole (no change)
     OR could be converted to a 360° arc. v1: no-op.

## 8. Design Decisions

- **Trim engine is a separate module** — keeps intersection math isolated
  from the growing sketch_feature.cpp. The sketch feature module calls into
  the trim engine, not the other way around.
- **One entity at a time** — trim affects only the clicked entity. If the
  user wants to trim both sides of a cross, they click each arm separately.
  This is the industry standard behavior and keeps undo/redo simple.
- **No constraint solver integration in v1** — constraints on affected
  entities are either re-evaluated by simple geometric checks or marked
  `dependency_broken`. A full re-solve pass is a future improvement.
- **Polygon dissolve** — trimming a polygon line breaks the polygon record.
  This is a deliberate simplification: maintaining parametric polygons
  through arbitrary trims is significantly more complex (the polygon's
  `sides`, `center`, `mode` would need recomputation).

## 9. Related Pages

- [2D Sketch System Architecture](2D-Sketch-System-Architecture)
- [2D Sketch Constraint System — TODO](2D-Sketch-Constraint-System)
- [Sketch Selection Controls](Sketch-Selection-Controls)
- `Sketch-Tool-Implementation` — file checklist for new sketch tools
- `Implementation-Log` — tracks shipped and pending items
