# Self-Reference Snap Problem

> **This is a recurring class of bugs.** Any snap that matches against the
> entity currently being manipulated produces a constraint that is always
> true and provides zero useful information. "This line is parallel to
> itself." "This circle is concentric with itself." The snap fires
> continuously and blocks every other snap type.

---

## What It Looks Like

During endpoint drag on a line:

- The snap label shows "Parallel" regardless of cursor direction.
- "Perpendicular" appears everywhere, even when the cursor is far from
  any other line.
- The dragged endpoint refuses to move unless you pull faster than the
  snap tolerance per frame (it snaps to its own previous position).

The same pattern will appear for any future manipulation:

- Dragging a circle center → concentric-with-self, tangent-to-self.
- Dragging a shared corner of two lines → parallel/perpendicular against
  both connecting lines.
- Dragging an arc endpoint → concentric with the arc's own center.

---

## Why It Happens

The snap engine treats the sketch as a static data structure. It has no
concept of "this entity is currently being manipulated." When
`drag_sketch_point` calls `resolve_discrete_snaps`, the sketch still
contains the dragged point at its previous-frame position. Every
collector function enumerates ALL entities — including the ones being
dragged — and finds candidates that trivially match:

| Snap kind | Self-reference | Why it's always true |
|---|---|---|
| Endpoint | `point_id` of candidate = dragged point | Cursor is within tolerance of the point's old position (slower than 2 mm/frame) |
| Midpoint | Midpoint of the line being dragged | Midpoint of a line whose endpoint you're moving |
| Parallel | Line direction = cursor direction | A line is always parallel to itself |
| Perpendicular (direction) | Line direction ± 90° = cursor direction | A line is always perpendicular to itself (rotated 90°) |
| Perpendicular (foot) | Foot of cursor onto the dragged line's infinite extension | Any cursor position has a perpendicular projection onto any line |
| Nearest (line body) | Projection onto the dragged line's body | Cursor near the line segment |

---

## The Fix Pattern

### Current (ad-hoc)

Individual exclusions bolted onto each resolver:

```
resolve_discrete_snaps:   filter by point_id
resolve_direction_snaps:  filter by entity_id (parallel, perpendicular_direction)
resolve_continuous_snaps: filter by entity_id (perpendicular foot)
```

Each new self-reference bug requires adding another field and another
filter clause in a different function. This will keep biting us.

### Recommended (structural)

Introduce a **manipulation context** passed alongside the selection
filter:

```cpp
struct ManipulationContext {
    std::string point_id;                  // the point being dragged
    std::vector<std::string> entity_ids;   // every entity that moves
                                           // when this point moves
};
```

Each category resolver accepts `const ManipulationContext*` (null =
no manipulation in progress, skip filtering). After collecting
candidates, apply a **single universal filter:**

> Drop any candidate where `c.point_id == ctx.point_id`
> OR `c.entity_id ∈ ctx.entity_ids`.

No per-kind carve-outs. One rule, applied uniformly. When a new feature
type is added (circle center drag, arc reshape, polygon vertex move),
the builder of the `ManipulationContext` walks the new entity types to
collect their IDs, and the exclusion is automatic.

### Building the context

In `drag_sketch_point` (and any future entry point that manipulates
geometry):

```cpp
ManipulationContext ctx;
ctx.point_id = point_id;

// Walk every entity type that can reference a point.
for (const auto& line : params.lines) {
    if (line.start_point_id == point_id || line.end_point_id == point_id)
        ctx.entity_ids.push_back(line.id);
}
for (const auto& circle : params.circles) {
    if (circle.center_point_id == point_id)       // once centers are point-based
        ctx.entity_ids.push_back(circle.id);
}
for (const auto& arc : params.arcs) {
    if (arc.start_point_id == point_id ||
        arc.end_point_id == point_id ||
        arc.center_point_id == point_id)
        ctx.entity_ids.push_back(arc.id);
}
// polygons, splines, etc.
```

The walk is cheap (a few dozen entity comparisons per drag frame) and
lives in one place.

---

## Checklist for New Manipulations

When adding a new geometry manipulation (circle center drag, arc
reshape, polygon vertex move, etc.):

- [ ] Does the new entry point build a `ManipulationContext` with the
      manipulated point and all dependent entity IDs?
- [ ] Does the snap call pass that context to all three category
      resolvers?
- [ ] Are there entity types in the sketch that reference the
      manipulated point but aren't walked by the context builder?
      Update the builder.
- [ ] Manual test: during manipulation, does the snap label ever show
      a constraint against the manipulated entity itself? If yes, the
      context is incomplete.

---

## Related

- `wiki/Snap-Engine-Fix-Plan.md` — category-based ranking refactor
- `wiki/Topological-Naming-Problem.md` — the project's other mantra
  (different problem, similar "don't trust stored references" philosophy)
- `native/cad-core/src/core/snap_engine.h` — `ManipulationContext` (future)
- `native/cad-core/src/core/document.cpp` — `drag_sketch_point`
