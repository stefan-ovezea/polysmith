# STEP import viewport performance — work notes (branch `step-import-viewport-perf`)

Context: importing a real-world KiCad PCB assembly STEP (~30 MB, 291 solids,
14,898 faces, 78,167 unique edges) took **~30 minutes** before the body appeared
in the viewport, and the app was then sluggish. This branch fixes the load time
in two steps. Both are kept here for evaluation — see "Known regression" before
deciding whether to keep this work.

## Step 1 — make the import load in ~10 seconds instead of ~30 minutes

The viewport's edge/vertex enumeration had two quadratic probes in
`native/cad-core/src/core/viewport/impl/body_edge_vertex_helpers.inc`:

- the seam-line probe scanned **every face for every edge** (O(E×F) ≈ 1.2 billion
  `IsClosed` calls on the board), and
- the circle-seam probe rebuilt the body's edge map for **every vertex** and then
  scanned all edges (O(V×E) ≈ 12 billion operations).

Both were replaced by `BodySeamAnalysis`, which precomputes the seam topology per
body in one incidence pass (face→edge walk + edge→endpoint walk) with identical
semantics.

Measured on the board (`build_viewport_state`):

| | before | after step 1 |
|---|---|---|
| viewport rebuild | 1,879,387 ms (~31 min) | 2,861 ms |
| total import → on screen | ~20–30 min | ~15 s (8 s of it is reading/meshing the 30 MB file) |

## Step 2 — shrink the viewport event / scene-object count (sluggishness)

Even at ~3 s the rebuild shipped **47.5 MB of JSON** per refresh and the UI built
~250,000 scene objects (78k edge lines + 156k vertex sprites + 15k face meshes) —
that made everything after load sluggish.

`viewport.cpp` now gives **oversized imported bodies** (STEP/IGES imports above
`kMaxImportPickEdgeCount = 8000` unique edges) the same body-level-only picking
model `mesh_import` bodies already use: only the body mesh primitive is emitted,
no per-edge/per-vertex/per-face pick entries.

Measured on the board:

| | before step 2 | after step 2 |
|---|---|---|
| viewport rebuild | 2,861 ms | 833 ms |
| viewport event size | 47.5 MB | 16.8 MB (mesh only) |
| event serialization | 3.3 s | 0.4 s |
| scene objects for the body | ~250,000 | 1 |

Small imports (≤ 8000 edges) are unaffected and keep full face/edge/vertex
picking.

## Known regression — RESOLVED by decimated per-face pick proxies

Step 2 (the pick-entry threshold) removed the per-face pick entries for
oversized imports, which broke two workflows. **Both worked after step 1**
(the load was fast and the viewport sluggish, but sketch-on-face and the
silhouette projection of the board still functioned — confirmed by the user who
tested the intermediate state):

1. **Sketch on body face** — cannot start a sketch on a face of the imported
   board (face placement resolves through per-face pick entries).
2. **Project tool on the body** — projecting the board's silhouette/section into
   a sketch does nothing. The UI's body-hit routing
   (`handleProjectFacePick` in
   `apps/desktop-ui/src/app/viewportFaceSelection.ts`) only maps body-id clicks
   to `project_body_into_sketch` for `mesh_import`/`mesh_to_body` kinds.

Fix (uncommitted, verified in core tests only — pending in-app verification):

- **Core** — oversized imports now emit a DECIMATED per-face pick proxy per
  face (`viewport.cpp` passes `decimate_pick_faces` to
  `enumerate_body_faces`). Faces above `kMaxMeshFacePickTriangles` (48) ship a
  centroid fan over the outer wire (planar faces — exact, boundary strided to
  the budget) or a strided triangulation subset (curved faces, where a fan
  would span only one cross-section). Small faces ship their full (tiny)
  triangulation. Per-edge/per-vertex entries stay skipped — the proxy payload
  is a few MB versus the original 47.5 MB flood. Face ids + plane frames +
  surface classification all flow, so sketch-on-face placement, face
  selection, and face-based features work again.
- **UI** — `handleProjectFacePick` now maps body-id clicks to
  `project_body_into_sketch` for `step_import`/`iges_import` too, restoring the
  silhouette/section projection of the board.
- **Tests** — `viewport_seam_enumeration_test` test 4 rewritten: an oversized
  compound (900 boxes + big cylinder + 100-gon prism) asserts every face proxy
  stays within the 48-triangle budget, planar fans exist (sketch placement),
  and curved faces keep their surface kind + radius witness. Verified
  fail-before (old gate: 0 face entries) / pass-after. Full suite: 50/50,
  `tsc --noEmit` clean.

Since step 1 alone (commit 873ff65) is a pure speed win with no behavior change,
it can be kept independently of step 2 if desired.
