# ADR-0002: Core-Driven Drag Preview

**Status:** Accepted  
**Date:** 2026-05-31

## Context

The current endpoint drag implementation in `ViewportPanel.tsx` mutates
Three.js meshes directly during `handlePointerMove` and duplicates
constraint math (H/V, relations) client-side. This was built as a
workaround for IPC latency in an earlier architecture where the core was
slower and the UI tried to be smarter.

The architecture has since moved to: **UI is a pure renderer; the core
owns all geometry, constraints, and snap computation.**  The Mirror
tool, extrude depth editing, and fillet radius preview already follow
this pattern correctly — they send intermediate positions to the core
and render whatever comes back.

The client-side drag preview has several problems:
- **Maintenance burden:** constraint math lives in two places (C++ core
  and TypeScript UI), and they drift apart.
- **Snap-backs:** the client-side preview doesn't have access to the
  full constraint graph, dimension driving, or BFS-based relation
  cascades. The core re-enforces everything on commit, producing a
  different position and a visible jump.
- **Missing snap highlights:** the core is the authority on snap points
  (endpoint, midpoint, center, tangent, intersection, etc.).  A
  client-side preview cannot compute these because it doesn't have
  access to full sketch geometry, occluded edges, or 3D reference
  geometry.  Snap highlighting during drag must come from the core.
- **Visual glitches:** mutating Three.js mesh geometry directly then
  having the viewport rebuild dispose and recreate everything produces
  flicker and phantom frames.

## Decision

**Use approach A: send `update_sketch_point` (existing IPC) on every
pointer-move during a drag, and render the core's response.**

The UI will:
1. On `pointerDown`: capture the hit point id, set drag state (no IPC).
2. On `pointerMove`: resolve cursor to sketch-local coordinates, send
   `update_sketch_point(pointId, x, y)` to the core.  Throttle to
   at most one in-flight request (drop intermediate frames if the
   previous request hasn't completed).
3. On `pointerUp`: send the final `update_sketch_point` — same
   command, but the core treats it as the committed position (undo
   checkpoint).
4. The core responds with a document state event, which triggers a
   viewport snapshot event, which triggers the existing scene rebuild
   in the UI — no special preview rendering code.

All client-side constraint math, mesh mutation, and `endpointDragRef`
Three.js manipulation will be removed.

## Rejected Alternative: Lightweight preview IPC (approach B)

Adding a `preview_sketch_point` IPC command that skips the full
`refresh_sketch_derived_state` pipeline was considered but rejected
for now because:

- **Loses snap computation.** The core's snap engine needs the full
  geometry graph to highlight snap targets (endpoints, midpoints,
  tangents, intersections) near the cursor.  A lightweight preview
  that skips snap computation would degrade the drag experience.
- **Premature optimization.** Parametric CAD sketches are typically
  small (tens of entities, not thousands) and split across multiple
  sketch planes.  The core's constraint enforcement and viewport
  snapshot generation are fast enough for 60 fps on realistic
  sketches.
- **The core computes the viewport anyway.** Every document state
  change already triggers a viewport snapshot.  Sending
  `update_sketch_point` on pointer-move just means more frequent
  snapshots — but the snapshot is the same computation the core
  already does.
- **Adds a second code path.** Approach B would require a parallel
  "preview" pipeline that mirrors the commit pipeline but with
  omissions — exactly the duplication we're trying to eliminate.

If profiling shows the full `update_sketch_point` is too slow at
60 fps on real-world sketches, approach B can be reconsidered with
a throttled hybrid: send preview at 20 fps and interpolate
client-side, then commit with the full pipeline.

## Consequences

- **Removes ~300 lines** of client-side constraint math and mesh
  mutation from `ViewportPanel.tsx` (the functions added in the
  May 31 constraint-preview attempt, plus the existing
  `handlePointerMove` mesh mutation block).
- **Unifies the drag path** with the Mirror, Extrude, and Fillet
  preview patterns — all go through core → IPC → render.
- **Enables snap highlighting during drag** as a natural side effect
  (the core's viewport snapshot includes snap data).
- **Potential latency:** if the core cannot keep up at 60 fps, the
  drag will feel sluggish.  Mitigation: throttle to one in-flight
  request, drop intermediate frames.
- **Higher IPC volume:** each pointer-move during a drag becomes an
  IPC round-trip.  For `stdin`/`stdout` IPC on the same machine,
  this is sub-millisecond and should not be a bottleneck.

## Implementation Notes (2026-05-31)

Implemented with the following additions beyond the original decision:

- **Incremental scene update:** the viewport rebuild effect detects an
  active drag (`endpointDragRef` set, no pending commit) and updates
  existing Three.js meshes in-place from the new `sceneData` instead
  of full dispose+rebuild.  Lines, circles, and points are patched by
  entity id.  On pointer-up, the full rebuild runs to finalize.
- **Throttle:** `inFlight` flag on `EndpointDrag` prevents queuing
  IPC commands faster than the core can respond.
- **Preview preview survives rebuild:** `endpointDragRef` is cleared
  AFTER all new meshes are added, eliminating the 1-frame flash of
  stale geometry.
- Performance at 60 fps is adequate for typical parametric sketches
  (tens of entities).  If profiling shows lag on larger sketches,
  the incremental update can be extended to primitives and references.
