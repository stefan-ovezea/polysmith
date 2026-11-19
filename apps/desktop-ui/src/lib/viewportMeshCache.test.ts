// The mesh delta: the core sends only bodies whose triangles changed, so the
// client has to merge the reply into what it already holds, and it must tell the
// core what it holds on each request. Both halves are pinned here because a
// regression in either produces a silently stale or empty viewport.
import { describe, expect, it, beforeEach } from "vitest";

import {
  knownMeshRevisions,
  mergeViewportMeshes,
  resetViewportMeshCache,
} from "./viewportMeshCache";
import type { ViewportMeshPrimitive, ViewportState } from "@/types";

function mesh(id: string, selected = false): ViewportMeshPrimitive {
  return {
    primitive_id: id,
    positions: "",
    normals: "",
    indices: "",
    is_selected: selected,
    appearance_color: null,
  };
}

function viewport(overrides: Partial<ViewportState>): ViewportState {
  return { meshes: [], ...overrides } as ViewportState;
}

describe("viewport mesh cache", () => {
  beforeEach(() => resetViewportMeshCache());

  it("treats a payload without revisions as a full snapshot", () => {
    mergeViewportMeshes(viewport({ meshes: [mesh("a"), mesh("b")] }));
    expect(Object.keys(knownMeshRevisions())).toEqual([]);
    expect(knownMeshRevisions()).toEqual({});
  });

  it("merges a delta into what the client already holds", () => {
    const full = mergeViewportMeshes(
      viewport({
        meshes: [mesh("a"), mesh("b")],
        mesh_revisions: { a: "1", b: "1" },
        removed_meshes: [],
        body_states: {
          a: { is_selected: false, appearance_color: null },
          b: { is_selected: false, appearance_color: null },
        },
      }),
    );
    expect(full.meshes.map((m) => m.primitive_id).sort()).toEqual(["a", "b"]);
    expect(knownMeshRevisions()).toEqual({ a: "1", b: "1" });

    // Only `b` changed; `a` must survive from the cache.
    const delta = mergeViewportMeshes(
      viewport({
        meshes: [mesh("b")],
        mesh_revisions: { a: "1", b: "2" },
        removed_meshes: [],
        body_states: {
          a: { is_selected: true, appearance_color: "#ff0000" },
          b: { is_selected: false, appearance_color: null },
        },
      }),
    );
    expect(delta.meshes.map((m) => m.primitive_id).sort()).toEqual(["a", "b"]);
    expect(knownMeshRevisions()).toEqual({ a: "1", b: "2" });
    // The small per-body state lands on the cached geometry.
    const a = delta.meshes.find((m) => m.primitive_id === "a")!;
    expect(a.is_selected).toBe(true);
    expect(a.appearance_color).toBe("#ff0000");
  });

  it("reuses the cached scene when the revision matched", () => {
    const full = mergeViewportMeshes(
      viewport({
        revision: 7,
        meshes: [mesh("a")],
        mesh_revisions: { a: "1" },
        removed_meshes: [],
        body_states: { a: { is_selected: false, appearance_color: null } },
        solid_faces: [
          { face_id: "f:face:0", is_selected: false },
          { face_id: "f:face:1", is_selected: false },
        ] as never,
        edges: [{ id: "e1" }] as never,
      }),
    );
    expect(full.solid_faces).toHaveLength(2);

    // Same revision: the core sent no faces and no edges, only the selection.
    const delta = mergeViewportMeshes(
      viewport({
        revision: 7,
        scene_unchanged: true,
        selected_faces: ["f:face:1"],
        meshes: [],
        mesh_revisions: { a: "1" },
        removed_meshes: [],
        body_states: {},
        solid_faces: [] as never,
        edges: [] as never,
      }),
    );
    expect(delta.solid_faces).toHaveLength(2);
    expect(delta.edges).toHaveLength(1);
    expect(delta.solid_faces[0].is_selected).toBe(false);
    expect(delta.solid_faces[1].is_selected).toBe(true);
  });

  it("drops bodies the core reports as removed", () => {
    mergeViewportMeshes(
      viewport({
        meshes: [mesh("a"), mesh("b")],
        mesh_revisions: { a: "1", b: "1" },
        removed_meshes: [],
        body_states: {},
      }),
    );
    const delta = mergeViewportMeshes(
      viewport({
        meshes: [],
        mesh_revisions: { a: "1" },
        removed_meshes: ["b"],
        body_states: {},
      }),
    );
    expect(delta.meshes.map((m) => m.primitive_id)).toEqual(["a"]);
  });
});
