// The face pick proxies are no longer shipped: the body mesh carries
// `face_ids` (one face ordinal per triangle) and the scene carves each face's
// proxy out of that mesh. If this derivation breaks, face picking and the face
// highlight break with it — silently, because the body still renders.
import { describe, expect, it } from "vitest";

import { createViewportScene } from "./viewportScene";
import type { ViewportState } from "@/types";

const EMPTY: never[] = [];

// base64 float32 / uint32 blobs, the wire format since phase E4.
const f32 = (values: number[]) =>
  Buffer.from(new Float32Array(values).buffer).toString("base64");
const u32 = (values: number[]) =>
  Buffer.from(new Uint32Array(values).buffer).toString("base64");

function makeViewport(): ViewportState {
  // One body: a quad split into two triangles. Triangle 0 belongs to face 0,
  // triangle 1 to face 1 — so each face proxy must come out with exactly one
  // triangle, and only its own vertices.
  const positions = [
    0, 0, 0, // vertex 0
    10, 0, 0, // vertex 1
    10, 10, 0, // vertex 2
    0, 10, 0, // vertex 3
  ];
  const indices = [0, 1, 2, 0, 2, 3];
  return {
    meshes: [
      {
        primitive_id: "feature-1",
        positions: f32(positions),
        normals: f32(new Array(12).fill(0)),
        indices: u32(indices),
        face_ids: u32([0, 1]),
        colors: "",
        is_selected: false,
        appearance_color: null,
      },
    ],
    solid_faces: [0, 1].map((ordinal) => ({
      face_id: `feature-1:face:${ordinal}`,
      owner_id: "feature-1",
      owner_kind: "extrude",
      label: `Face ${ordinal + 1}`,
      sketchability: "planar",
      center: { x: 5, y: 5, z: 0 },
      normal: { x: 0, y: 0, z: 1 },
      plane_frame: {
        origin: { x: 0, y: 0, z: 0 },
        x_axis: { x: 1, y: 0, z: 0 },
        y_axis: { x: 0, y: 1, z: 0 },
        normal: { x: 0, y: 0, z: 1 },
      },
      size: { width: 10, height: 10, radius: 0 },
      is_selected: false,
      appearance_color: null,
      surface_kind: "planar",
    })),
    // Everything else the scene builder walks.
    boxes: EMPTY,
    cylinders: EMPTY,
    polygon_extrudes: EMPTY,
    edges: EMPTY,
    vertices: EMPTY,
    helices: EMPTY,
    reference_planes: EMPTY,
    reference_axes: EMPTY,
    reference_points: EMPTY,
    sketch_lines: EMPTY,
    sketch_circles: EMPTY,
    sketch_polygons: EMPTY,
    sketch_arcs: EMPTY,
    sketch_ellipses: EMPTY,
    sketch_splines: EMPTY,
    sketch_vertices: EMPTY,
    sketch_dimensions: EMPTY,
    sketch_constraints: EMPTY,
    sketch_profiles: EMPTY,
    cut_previews: EMPTY,
    toolpaths: EMPTY,
    bodies: EMPTY,
    dof_statuses: EMPTY,
    snap_candidates: EMPTY,
    drawing_sheets: EMPTY,
    scene_width: 10,
    scene_height: 10,
    scene_depth: 1,
    scene_bounds: {
      center: { x: 5, y: 5, z: 0 },
      size: { x: 10, y: 10, z: 1 },
      max_dimension: 10,
    },
  } as unknown as ViewportState;
}

describe("face pick geometry from the body mesh", () => {
  const scene = createViewportScene(makeViewport(), {});
  const faces = scene.solidFaces;

  it("gives every face the triangles its pick map assigns", () => {
    expect(faces).toHaveLength(2);
    for (const face of faces) {
      expect(face.triangleIndices).toHaveLength(3);
      expect(face.trianglePositions).toHaveLength(9);
    }
  });

  it("keeps each proxy to its own triangle's vertices", () => {
    const first = faces[0];
    const second = faces[1];
    // Face 0 covers (0,0) (10,0) (10,10); face 1 covers (0,0) (10,10) (0,10).
    expect(Array.from(first.trianglePositions)).toEqual([
      0, 0, 0, 10, 0, 0, 10, 10, 0,
    ]);
    expect(Array.from(second.trianglePositions)).toEqual([
      0, 0, 0, 10, 10, 0, 0, 10, 0,
    ]);
    // Indices are local to the proxy, not the body mesh.
    expect(Array.from(first.triangleIndices)).toEqual([0, 1, 2]);
    expect(Array.from(second.triangleIndices)).toEqual([0, 1, 2]);
  });

  it("leaves a face without a proxy when the map has no triangle for it", () => {
    const viewport = makeViewport();
    // Drop the second triangle: face 1 has no geometry any more, face 0 keeps
    // its one triangle.
    viewport.meshes[0].indices = u32([0, 1, 2]);
    viewport.meshes[0].face_ids = u32([0]);
    const partial = createViewportScene(viewport, {});
    expect(partial.solidFaces[0].triangleIndices).toHaveLength(3);
    expect(partial.solidFaces[1].triangleIndices).toHaveLength(0);
  });
});
