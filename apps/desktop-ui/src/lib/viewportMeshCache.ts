// Client-side cache for the viewport delta.
//
// Geometry: the core sends only bodies whose triangles changed (fingerprinted),
// plus every body's revision, the ids that disappeared, and the tiny per-body
// state (selection, colour) that changes far more often than geometry.
//
// Scene: `viewport.revision` is the shape/overlay version and does NOT bump on
// selection, so when the client says it already holds that revision the core
// omits the face table and the overlay entirely and sends only the selected
// face ids. Measured on the 28 MB board: 20.16 MB -> 0.03 MB for a selection.
import type { ViewportMeshPrimitive, ViewportState } from "@/types";

let cache = new Map<string, ViewportMeshPrimitive>();
let revisions = new Map<string, string>();
let lastViewport: ViewportState | null = null;
// Whether the request in flight told the core what we hold. A request WITHOUT
// fingerprints gets a full snapshot back, and a full snapshot must REPLACE the
// cache - merging it would leave the bodies of a previous document behind as
// ghost geometry (doubled outlines and floating ribbons in the viewport).
let requestIsDelta = false;
// Which document the cache belongs to. iewport.revision is document-local, so
// two documents can share a revision number; without this a reload of a saved
// file served the previous document's bodies and features looked lost.
let cachedDocumentId: string | null = null;

/**
 * Note the document the next messages belong to. A different id resets the
 * cache: body ids and revisions are only meaningful within one document.
 */
export function noteDocumentId(documentId: string): void {
  if (cachedDocumentId === documentId) {
    return;
  }
  cachedDocumentId = documentId;
  cache = new Map();
  revisions = new Map();
  lastViewport = null;
  requestIsDelta = false;
}

/** Call before building a `get_viewport_state`: remembers how it was asked. */
export function beginViewportRequest(): void {
  requestIsDelta = revisions.size > 0;
}

/** The fingerprints to send with the next `get_viewport_state`. */
export function knownMeshRevisions(): Record<string, string> {
  const known: Record<string, string> = {};
  for (const [id, revision] of revisions) {
    known[id] = revision;
  }
  return known;
}

/** The scene revision the client already holds, or null for a first fetch. */
export function knownSceneRevision(): number | null {
  return lastViewport?.revision ?? null;
}

/** Drop everything: the document was replaced, so no id can be trusted. */
export function resetViewportMeshCache(): void {
  cache = new Map();
  revisions = new Map();
  lastViewport = null;
}

/**
 * Merge a reply into what the client holds and return a complete viewport state.
 *
 * A payload without `mesh_revisions` is a full snapshot (an older core, or a
 * caller that sent no fingerprints): it replaces the cache.
 */
export function mergeViewportMeshes(viewport: ViewportState): ViewportState {
  const incomingRevisions = viewport.mesh_revisions;
  if (!incomingRevisions || !requestIsDelta) {
    // Full snapshot: replace, never merge.
    cache = new Map(viewport.meshes.map((mesh) => [mesh.primitive_id, mesh]));
    revisions = incomingRevisions ? new Map(Object.entries(incomingRevisions)) : new Map();
    lastViewport = viewport;
    return viewport;
  }

  for (const mesh of viewport.meshes) {
    cache.set(mesh.primitive_id, mesh);
  }
  for (const id of viewport.removed_meshes ?? []) {
    cache.delete(id);
  }
  revisions = new Map(Object.entries(incomingRevisions));

  // Selection and appearance travel separately; apply them to the cached
  // geometry so a click updates the highlight without re-sending triangles.
  for (const [id, state] of Object.entries(viewport.body_states ?? {})) {
    const cached = cache.get(id);
    if (
      cached &&
      (cached.is_selected !== state.is_selected ||
        cached.appearance_color !== state.appearance_color)
    ) {
      cache.set(id, {
        ...cached,
        is_selected: state.is_selected,
        appearance_color: state.appearance_color,
      });
    }
  }

  // Scene delta: the heavy arrays are omitted because the client already has
  // this exact revision. Reuse them, then apply what did change — the selection.
  let scene = viewport;
  if (viewport.scene_unchanged && lastViewport) {
    const selected = new Set(viewport.selected_faces ?? []);
    scene = {
      ...lastViewport,
      ...viewport,
      solid_faces: lastViewport.solid_faces.map((face) => {
        const isSelected = selected.has(`face_id` in face ? (face as { face_id: string }).face_id : "");
        return face.is_selected === isSelected ? face : { ...face, is_selected: isSelected };
      }),
      edges: lastViewport.edges,
      vertices: lastViewport.vertices,
      bodies: lastViewport.bodies,
      boxes: lastViewport.boxes,
      cylinders: lastViewport.cylinders,
      polygon_extrudes: lastViewport.polygon_extrudes,
      reference_planes: lastViewport.reference_planes,
      reference_axes: lastViewport.reference_axes,
      reference_points: lastViewport.reference_points,
      helices: lastViewport.helices,
      sketch_lines: lastViewport.sketch_lines,
      sketch_circles: lastViewport.sketch_circles,
      sketch_ellipses: lastViewport.sketch_ellipses,
      sketch_splines: lastViewport.sketch_splines,
      sketch_polygons: lastViewport.sketch_polygons,
      sketch_arcs: lastViewport.sketch_arcs,
      sketch_vertices: lastViewport.sketch_vertices,
      sketch_dimensions: lastViewport.sketch_dimensions,
      sketch_constraints: lastViewport.sketch_constraints,
      sketch_profiles: lastViewport.sketch_profiles,
      dof_statuses: lastViewport.dof_statuses,
      snap_candidates: lastViewport.snap_candidates,
      toolpaths: lastViewport.toolpaths,
      drawing_sheets: lastViewport.drawing_sheets,
      cut_previews: lastViewport.cut_previews,
    };
  }

  const merged: ViewportState = { ...scene, meshes: Array.from(cache.values()) };
  lastViewport = merged;
  return merged;
}
