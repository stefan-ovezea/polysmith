# Active Task: Arc recovery in face projections (feature/projection-arcs)

> **Branch:** `feature/projection-arcs` (from `dev`, after #64)
> **Date:** 2026-08-21

## Status

Committed as `9fd125d` (verified by the user in the running app).
Squash-merge PR to `dev` pending.

## Backlog (next sessions — own branch each)

1. **Mesh outline circle/arc fitting for projections.** Mesh-converted
   body outlines are faceted polygons — a through-hole projects as
   ~150 segments instead of one circle. Detect near-circular loops
   (and arc-like contour runs) on mesh faces → emit `SketchCircle` /
   `SketchArc`. (Task #25)
2. **Strip selection state from save files.** The core serializes
   transient selection (`selected_sketch_*`, `selected_face_id`,
   `selected_edge_ids`, `selected_vertex_ids`) into `part.json` and
   restores it on load — reloaded parts come back with highlights
   intact. Clear/omit on save; round-trip test. (Task #26)
3. **Fix mesh-with-hole conversion crash in the test harness.**
   Pre-existing `0xC0000409` in `convert_mesh_to_body` on a synthetic
   box-with-through-hole; the mesh-projection-stays-healthy regression
   test in `tests/face_projection_arc_test.cpp` is disabled because of
   it (see comment there + Implementation-Log). Fix the crash, then
   enable the test. (Task #27)

## Problem (user-reported)

Projecting the top face of a rounded-rect extrude (4 fillets + through-
hole) produced 72 straight segments — fillet arcs became chords, the
hole circle a 64-segment polyline.

## Fix (implemented, 14/14 suites green, awaiting user verification)

- Face outlines carry curve identity (`polygon_segment_arcs` /
  `FaceOutlineArc` with an orientation-proof midpoint), seam-split
  circular wires are detected as full circles, merge/simplify are
  arc-aware.
- Face projection emits exact `SketchArc`s (ccw from sketch-local
  geometry) and `SketchCircle`s for holes; chord fallback only for
  non-parallel-plane curves. Projected arc endpoints/centers fixed
  (face + edge paths — fix loops run before the projection record is
  moved).
- Live re-derivation patches arcs on fillet edits (count validation
  extended to lines+arcs+circles).
- Tests: `cad_core_face_projection_arc_test` (3 cases). All 14 C++
  suites green.

## Verification checklist (user)

1. Reproduce the original scenario: rounded-rect extrude → fillet →
   hole → sketch on top → Project → fillet corners arrive as ARCS
   (radius preserved), hole as a CIRCLE, endpoints fixed.
2. Edit the fillet radius → projected arcs follow live.
3. Save/reload → arcs + records survive.
4. Project a curved edge (arc) via the edge tool → endpoints now fixed.

## Next session

- User verification → commit approval (squash-merge to `dev`).
