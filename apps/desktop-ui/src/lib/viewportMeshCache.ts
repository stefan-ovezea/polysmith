// Client-side mesh cache for the viewport delta.
//
// The core used to re-send every body's triangles on every refetch — 16 MB of a
// 20 MB snapshot on the 28 MB board, spent again for a selection click or a
// sketch edit, neither of which changes a triangle. Now the request carries the
// fingerprints the client already has (`known_meshes`) and the reply carries
// only the bodies whose geometry changed, plus every body's revision, the ids
// that disappeared, and the tiny per-body state (selection, colour) that changes
// far more often than geometry.
import type { ViewportMeshPrimitive, ViewportState } from "@/types";

let cache = new Map<string, ViewportMeshPrimitive>();

/** The fingerprints to send with the next `get_viewport_state`. */
export function knownMeshRevisions(): Record<string, string> {
  const known: Record<string, string> = {};
  for (const [id, revision] of revisions) {
    known[id] = revision;
  }
  return known;
}

let revisions = new Map<string, string>();

/** Drop everything: the document was replaced, so no id can be trusted. */
export function resetViewportMeshCache(): void {
  cache = new Map();
  revisions = new Map();
}

/**
 * Merge a delta reply into the cache and return a complete viewport state.
 *
 * A payload without `mesh_revisions` is a full snapshot (an older core, or a
 * caller that sent no fingerprints): it replaces the cache.
 */
export function mergeViewportMeshes(viewport: ViewportState): ViewportState {
  const incomingRevisions = viewport.mesh_revisions;
  if (!incomingRevisions) {
    cache = new Map(viewport.meshes.map((mesh) => [mesh.primitive_id, mesh]));
    revisions = new Map();
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

  return { ...viewport, meshes: Array.from(cache.values()) };
}
