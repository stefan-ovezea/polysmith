# Sketch snapping — the matrix, and why it keeps breaking

Measured 2026-10-26 on `dev` @ `5ad6007`. The measurement lives in
`apps/desktop-ui/src/layout/viewport/snapMatrix.test.ts` — run it and it prints the
table; `contract` cells are asserted, `knownGaps` cells are printed only.

## How SolidCraft does it (reference implementation, read from its source)

`refine_snap()` (ui-egui) picks an inference and returns an **intent string**; the
core command consumes it and creates the point **and** its constraint atomically
(`snap_points()` in `engine/src/cmd/sketch_project.rs`):

| intent | core does |
|---|---|
| `vertex:x,y,z` | projects a model vertex (link) |
| `mid:<line>` | `add_point(midpoint)` + `ConstraintKind::Midpoint{p,l}` |
| `on:<curve>:x,y` | `add_point(shape.project(near))` + `PointOnCurve{p,c}` |
| `on_edge:x,y,z` | projects the edge, point on the projected curve |

Priority is **first match by kind**, not closest-wins across kinds:

```
model vertex (Pt) > midpoint (Mid) > tangent (T) > on curve (On, tol*0.8)
> H/V alignment with the previous point > parallel (//) / perpendicular (⊥) / extension (Ext) > grid
```

Two properties follow, and they are the ones this project is missing:

1. **An attachment cannot be lost**, because the constraint is created in the same
   command as the point. There is no snap-flavour-to-anchor pipeline.
2. **A line drawn onto geometry stays even when no snap fires**, because the core
   owns the decision and can re-infer from the geometry.

## How PolySmith does it

The core emits **candidate points** with the viewport state —
`endpoint, midpoint, center, quadrant, intersection, projected` — and the UI does
everything else: arbitration (closest-wins across a chain of dynamic snaps), the
dynamic geometry itself (line body, circle body, arc body, tangent, perpendicular,
parallel, alignment, axis lock, grid), a speculative planegcs-WASM solve for
tangent/perp/parallel, **and** the request to attach (`set_sketch_midpoint_anchor`,
`set_sketch_vertex_line_anchor`, relation commands) from snap bookkeeping that
crosses draft start → commit → effect through three refs.

So the UI snap is not only visual: it decides the committed position and whether an
attachment is written. When the bookkeeping is lost, the geometry still sits exactly
on the line and nothing holds it — which is the reported T-junction behaviour.

## The matrix, as measured

| target | snap label | placed at | binding |
|---|---|---|---|
| line endpoint | Endpoint | 100.00, 0.00 | `endpoint:line-1` ✓ |
| line midpoint | Midpoint | 50.00, 0.00 | `midpoint:line-1` ✓ |
| circle centre | **Vertical** | 140.00, 0.00 | — |
| arc quadrant | Quadrant | 20.00, 100.00 | — |
| line × line intersection | **Vertical** | 20.00, 40.00 | — |
| line body (t ≠ 0.5) | **Horizontal** | 30.50, 0.00 | — |
| circle perimeter | **Quadrant** | 160.00, 0.00 | — |
| arc perimeter | On circle | 14.14, 114.14 | `circleBody:arc-1` ✓ |
| vertical stem body | **Vertical** | 50.00, −40.50 | — |
| horizontal alignment | Horizontal | 70.50, 0.00 | — |
| parallel host | **Vertical** | 30.00, 6.00 | `axis:vertical` |
| perpendicular host | **Vertical** | 30.00, 6.00 | `axis:vertical` |

`binding` is what a later attachment can be built from. A correct position with an
empty binding is the failure mode: nothing to attach, so nothing holds.

## Findings

1. **Centre and quadrant snaps carry no host id at all**, so a line started at a
   circle centre or a quadrant can never be attached. (Bold label above = the label
   came from the axis lock / alignment, while the position came from the candidate —
   so the label is not trustworthy either.)
2. **The core-emitted `intersection` candidate is not mapped** in
   `appendCoreSnapCandidates` — it falls through to the default arm, so
   `snapIntersectionLineIds` stays null and an intersection snap cannot attach.
3. **Axis lock and alignment win the label over a feature snap** when both are near,
   which makes the reported kind contradict the position.
4. Only two cells are proven to work end to end: **endpoint** and **midpoint** — and
   even those only because the UI happens to carry their host ids in its own
   bookkeeping.

## The fix this points at (needs approval: it changes the IPC contract)

Make snapping an **intent** the core consumes, as SolidCraft does: point arguments
are `[x,y]` or `{on: curveId, t}` / `{mid: curveId}` / `{vertex: id}`, and the core
creates the point **plus** its constraint in the same command, with a geometric
fallback (a vertex within tolerance of a curve is attached to it) so a lost snap
flavour can never leave geometry unattached. The UI keeps its dynamic chain for the
*visual* side, but it becomes a hint, not the source of truth.
