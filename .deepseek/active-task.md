# Active Task: Arc recovery in face projections (feature/projection-arcs)

> **Branch:** `feature/projection-arcs` (from `dev`, after #64)
> **Date:** 2026-08-21

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
