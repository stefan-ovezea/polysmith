# Arc ↔ attached lines: systemic bugs (2026-11-02)

Living notes for a fresh session. Read this first, then the files at the bottom.

## Symptom matrix (from user testing)

| Geometry | Drag common end | Drag arc centre | Commit |
|---|---|---|---|
| **Trimmed arc** (circle → arc) + lines sharing its ends | arc preview **WRONG** (shows the deleted/complementary arc) | arc preview **WRONG**; sweep shrinks until < 180°, then renders in **reverse** | — |
| **Real arc** (`add_sketch_arc`) + 2 lines sharing its ends, common point | arc **correct** | arc **correct**, but lines look **disconnected** (they do follow) | lines **disconnect** (jump in place) |

Conclusions:
- **Wrong-arc preview is specific to the TRIMMED arc.** A real arc with identical topology previews correctly.
- **"Lines disconnect on commit" is systemic** — it happens with a real arc too, so it is independent of the trim.

## The most important difference (hypothesis)

`sketch_geometry_types.h` (SketchArc comment): a **real arc** is created with its endpoints
**`is_fixed = true`** ("v1 freezes the arc's shape at creation ... endpoints flagged
is_fixed=true"). A **trimmed arc** (from `trim_circle_entity_command.inc`) takes the shared
line endpoints instead (`resolve_shared_point`), so its endpoints are **not fixed**.

Consequence: on a centre drag the trimmed arc's endpoints are translated, but the shared
lines + solver/enforcement passes can move them, and the **sweep is derived from the moved
endpoints** (`updateSketchArcObject`: `sweep = endAngle - startAngle`, extended by `ccw`).
When the endpoints' angular order flips, `ccw` picks the *other* arc → the complement renders.
Once the sweep drops below 180° the branch decision visibly reverses.

## Known-present (fixed) before the open items

- `point_on_arc`: sweep-aware attachment (solver radius pin + `enforce_point_on_arc_constraints`).
- Arc follows a centre move (core centre-drag translates `point_on_arc` points + arc endpoints).
- "O" badge emitted for `point_on_arc` (`sketch_vertex_constraint_emit.inc`).
- Large-sweep fold bug: angle delta normalised **into the sweep direction** (`[0,2π)` ccw /
  `(-2π,0]` cw), not `[-π,π)`, in `point_lies_on_arc_sweep` and
  `enforce_point_on_arc_constraints`. Regression: `trim_test` large-arc case.
- Trim re-binds `point_on_circle` → `point_on_arc`; **skips the arc's own start/end** (the
  redundant constraint that caused the accumulating shrink).
- Preview rides **both** arc endpoints on a centre drag (`endpointDrag.ts`).
- **Reverted on purpose:** tangent-line transfer on trim. The tangency only exists at the
  trimmed arc's endpoints and cannot hold through a drag (lines elongate, angle changes),
  so `tangent_line_circle` is dropped with the circle and the "T" badge is lost.

## Open items

1. **Trimmed-arc preview renders the complementary arc during a drag** (real arc does not).
2. **Trimmed-arc sweep shrinks during a centre drag** until it flips (< 180° → reversed draw).
3. **Lines disconnect visually on commit** (systemic; also with a real arc). Likely the arc's
   cached `start_x/start_y` vs the shared vertex not converging in the committed scene.
4. "T" badge disappears on trim (accepted trade-off; a reference-only relation would keep the
   badge without driving — not implemented).

## Suspects / where to look

Core:
- `trim_circle_entity_command.inc` — how the survivor arc's start/end/`ccw` are chosen.
- `sketch_geometry_types.h` — `SketchArc` endpoint `is_fixed` policy (real vs trimmed).
- `sketch_line_vertex_update_commands.inc` — arc centre-drag path (lines ~170-195).
- `private_point_profile_helpers.inc` — `sync_entity_coords_from_vertices`, `find_coincident_endpoint`.
- `constraint_solver*.inc` — `arc_participates_in_solver` (plain arcs excluded → ArcRules not applied).
- `private_dimension_relation_sync.inc` — arc dimension enforcement.

UI:
- `endpointDrag.ts` — preview params / ride sets / active points.
- `sketchPreviewSceneUpdate.ts` — arc render during drag (uses solved points + `arc.ccw`).
- `sceneIncrementalUpdate.ts` — `updateSketchArcObject` sweep derivation.

## Suggested next steps

1. Reproduce with a **real arc** made by `add_sketch_arc` vs a **trimmed arc**, dump both in a
   saved file, and diff `is_fixed`, `ccw`, and start/end vertex ids.
2. Consider making the trimmed arc's render sweep derive from the **stored radius + ccw and a
   stable angular span**, not from the live endpoints, so a transient endpoint move cannot flip it.
3. For the commit disconnect: ensure `sync_entity_coords_from_vertices` runs after the final
   solver/enforcement pass for arcs, and that the arc's start/end are exactly the shared vertex
   positions on commit.
4. Add C++ tests: "real arc + 2 lines, centre drag keeps the sweep"; "trimmed arc + lines, centre
   drag keeps the sweep and does not render the complement".
