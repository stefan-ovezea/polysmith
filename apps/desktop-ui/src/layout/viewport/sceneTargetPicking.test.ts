import * as THREE from "three";
import { describe, expect, it } from "vitest";

import { edgePickToleranceWorld, pickModelTarget } from "./sceneTargetPicking";

// Regression guard for "some surfaces cannot be selected to make a sketch".
//
// The model pick tests edges BEFORE faces, and THREE.Line picking measures
// `raycaster.params.Line.threshold` in WORLD units. That threshold used to be a
// fixed 1.2, which on an imported PCB is wider than most parts: 98 % of the
// planar faces of a real board had their centre within 1.2 mm of an edge, so
// every click resolved to an edge and the face was unreachable — hover lit up
// edges instead of surfaces, and the double-click that starts a sketch on a
// face never fired. The tolerance is now derived from the zoom (a fixed number
// of SCREEN pixels), so these tests pin both directions of the change:
// a nearby-but-not-under-the-cursor edge must lose to the face, and an edge
// under the cursor must still win.

function faceMesh(faceId: string): THREE.Mesh {
  const mesh = new THREE.Mesh(
    new THREE.PlaneGeometry(10, 10),
    new THREE.MeshBasicMaterial({ side: THREE.DoubleSide }),
  );
  mesh.userData.faceId = faceId;
  return mesh;
}

function edgeLine(y: number): THREE.Line {
  const geometry = new THREE.BufferGeometry().setFromPoints([
    new THREE.Vector3(-5, y, 0),
    new THREE.Vector3(5, y, 0),
  ]);
  const line = new THREE.Line(
    geometry,
    new THREE.LineBasicMaterial({ depthTest: false }),
  );
  line.userData.edgeId = "feature-1:edge:0";
  return line;
}

function pickAtWorldOrigin(edgeY: number, edgeToleranceWorld: number) {
  const raycaster = new THREE.Raycaster();
  raycaster.set(new THREE.Vector3(0, 0, 5), new THREE.Vector3(0, 0, -1));
  return pickModelTarget({
    raycaster,
    referencePlaneMeshes: [],
    vertexObjects: [],
    edgeLineObjects: [edgeLine(edgeY)],
    faceMeshes: [faceMesh("feature-1:face:0")],
    meshes: [],
    edgeToleranceWorld,
  });
}

describe("body face pick vs edge pick", () => {
  it("derives the edge tolerance from the zoom, in screen pixels", () => {
    // 0.05 mm/px is a PCB at a working zoom (~50 mm across a 1000 px canvas).
    expect(edgePickToleranceWorld(0.05)).toBeCloseTo(0.25, 6);
    // Zoomed out, the same 5 px is a wider world radius; zoomed in, narrower.
    expect(edgePickToleranceWorld(0.5)).toBeGreaterThan(
      edgePickToleranceWorld(0.05),
    );
    // Degenerate scales must not make every click an edge click.
    expect(edgePickToleranceWorld(0)).toBe(0);
    expect(edgePickToleranceWorld(Number.NaN)).toBe(0);
    expect(edgePickToleranceWorld(-1)).toBe(0);
  });

  it("picks the face when the nearest edge is off the cursor", () => {
    // 0.5 mm away is 10 px at PCB zoom: outside the 5 px edge radius, so the
    // click belongs to the surface. With the old fixed 1.2 mm threshold this
    // returned the edge — the bug this test exists for.
    const hit = pickAtWorldOrigin(0.5, edgePickToleranceWorld(0.05));
    expect(hit).toEqual({ kind: "face", id: "feature-1:face:0" });
  });

  it("still picks an edge that is under the cursor", () => {
    const hit = pickAtWorldOrigin(0.1, edgePickToleranceWorld(0.05));
    expect(hit).toEqual({ kind: "edge", id: "feature-1:edge:0" });
  });

  it("leaves the caller's line threshold untouched", () => {
    const raycaster = new THREE.Raycaster();
    raycaster.set(new THREE.Vector3(0, 0, 5), new THREE.Vector3(0, 0, -1));
    raycaster.params.Line.threshold = 0.37;
    pickModelTarget({
      raycaster,
      referencePlaneMeshes: [],
      vertexObjects: [],
      edgeLineObjects: [edgeLine(0.5)],
      faceMeshes: [faceMesh("feature-1:face:0")],
      meshes: [],
      edgeToleranceWorld: edgePickToleranceWorld(0.05),
    });
    expect(raycaster.params.Line.threshold).toBe(0.37);
  });
});
