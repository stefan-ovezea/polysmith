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

## Known regression — must be resolved before this can land on dev

Oversized imports now have **no per-face pick entries**, which breaks two
workflows that previously worked:

1. **Sketch on body face** — cannot start a sketch on a face of the imported
   board (face placement resolves through per-face pick entries).
2. **Project tool on the body** — projecting the board's silhouette/section into
   a sketch does nothing. Before these modifications the silhouette projection
   of the board worked very well and was a valued workflow. The UI's body-hit
   routing (`handleProjectFacePick` in
   `apps/desktop-ui/src/app/viewportFaceSelection.ts`) only maps body-id clicks
   to `project_body_into_sketch` for `mesh_import`/`mesh_to_body` kinds; it needs
   `step_import`/`iges_import` added.

Ideas for the follow-up: emit decimated per-face pick proxies for oversized
imports (precedent: `kMaxMeshFacePickTriangles` decimation for `mesh_to_body`),
and/or raise the pick budget so the board keeps face picking while the payload
stays bounded.
