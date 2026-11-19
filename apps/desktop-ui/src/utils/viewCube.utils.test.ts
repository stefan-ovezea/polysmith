import * as THREE from "three";
import { describe, expect, it } from "vitest";

import {
  applyCubeDragOrbit,
  chamferedCubeFacets,
  faceAxisLabel,
  getQuantizedCubeUp,
  orbitCameraQuarterTurns,
} from "./viewCube.utils";

const FACE_NORMALS: Record<string, THREE.Vector3> = {
  TOP: new THREE.Vector3(0, 0, 1),
  BOTTOM: new THREE.Vector3(0, 0, -1),
  FRONT: new THREE.Vector3(0, -1, 0),
  BACK: new THREE.Vector3(0, 1, 0),
  RIGHT: new THREE.Vector3(1, 0, 0),
  LEFT: new THREE.Vector3(-1, 0, 0),
};

// The view cube's drag is the part that regressed: dragging a corner to the
// right tipped the board sideways (the model turned about a HORIZONTAL axis)
// because the orbit was computed with three's Y-up Spherical in a Z-up world.
// These tests pin the two things that were wrong — which world axis turns, and
// which way the model appears to move — plus the pole clamp that keeps
// `lookAt` from rolling the picture.
function rig(position: [number, number, number], up: [number, number, number] = [0, 0, 1]) {
  const camera = new THREE.OrthographicCamera(-50, 50, 50, -50, 0.1, 10000);
  camera.up.set(...up);
  camera.position.set(...position);
  const target = new THREE.Vector3(0, 0, 0);
  camera.lookAt(target);
  camera.updateMatrixWorld();
  const controls = {
    target,
    update: () => camera.updateMatrixWorld(),
  };
  return { camera, controls };
}

/** A point on screen, in the camera's own right/up axes (screen units). */
function screenCoords(camera: THREE.Camera, world: THREE.Vector3) {
  camera.updateMatrixWorld();
  const right = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0);
  const up = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 1);
  return { x: world.dot(right), y: world.dot(up) };
}

describe("view cube drag orbit", () => {
  it("turns the model about the vertical axis, not a horizontal one", () => {
    // Front view: the camera sits on -Y looking at the model, Z up.
    const { camera, controls } = rig([0, -500, 0]);
    applyCubeDragOrbit(camera, controls, 40, 0, 0.005);

    const offset = camera.position.clone().sub(controls.target);
    // The eye stays at the model's height: a horizontal drag must not lift or
    // lower it (that was the sideways tip).
    expect(Math.abs(offset.z)).toBeLessThan(1e-6);
    expect(Math.hypot(offset.x, offset.y)).toBeCloseTo(500, 6);
    // It moved sideways in plan view, i.e. about +Z.
    expect(Math.abs(offset.x)).toBeGreaterThan(10);
  });

  it("makes the model follow the pointer: drag right, the front turns right", () => {
    const { camera, controls } = rig([0, -500, 0]);
    // A point on the front face, directly between the camera and the model.
    const frontPoint = new THREE.Vector3(0, -10, 0);
    const before = screenCoords(camera, frontPoint);
    applyCubeDragOrbit(camera, controls, 40, 0, 0.005);
    const after = screenCoords(camera, frontPoint);

    expect(after.x).toBeGreaterThan(before.x + 1);
  });

  it("lifts the eye when the pointer is dragged down (top view direction)", () => {
    const { camera, controls } = rig([0, -500, 0]);
    applyCubeDragOrbit(camera, controls, 0, 60, 0.005);
    expect(camera.position.z).toBeGreaterThan(10);
  });

  it("never crosses the pole, so the up axis can not roll the picture", () => {
    const { camera, controls } = rig([0, -500, 0]);
    const up = new THREE.Vector3(0, 0, 1);
    for (let step = 0; step < 200; step += 1) {
      applyCubeDragOrbit(camera, controls, 0, 60, 0.005);
      const offset = camera.position.clone().sub(controls.target).normalize();
      // The view direction must stay clear of the up axis, otherwise lookAt has
      // no stable roll and the model flips. The drag stops at POLE_EPSILON
      // (0.01 rad) instead of reaching the pole.
      const angleFromUp = Math.acos(Math.min(1, Math.abs(offset.dot(up))));
      expect(angleFromUp).toBeGreaterThanOrEqual(0.01 - 1e-6);
    }
  });

  it("orbits about the camera up axis after a view change to a Y-up frame", () => {
    // A Y-up camera (a sketch on XZ uses that fallback): the orbit must follow
    // camera.up rather than assuming Z.
    const { camera, controls } = rig([0, 0, 500], [0, 1, 0]);
    applyCubeDragOrbit(camera, controls, 40, 0, 0.005);
    const offset = camera.position.clone().sub(controls.target);
    expect(Math.abs(offset.y)).toBeLessThan(1e-6);
    expect(Math.abs(offset.x)).toBeGreaterThan(10);
  });
});

describe("view cube face axis labels", () => {
  it("names the world axis each face looks along, Z-up", () => {
    expect(faceAxisLabel("TOP")).toBe("Z+");
    expect(faceAxisLabel("BOTTOM")).toBe("Z-");
    expect(faceAxisLabel("FRONT")).toBe("Y-");
    expect(faceAxisLabel("BACK")).toBe("Y+");
    expect(faceAxisLabel("RIGHT")).toBe("X+");
    expect(faceAxisLabel("LEFT")).toBe("X-");
  });
});

// FreeCAD 1.1's navigator is a CHAMFERED cube: the corners are not boxes, they
// are triangles between the faces, and the edges are strips. These tests pin the
// decomposition (the 26 views) and the chamfer arithmetic, because a wrong facet
// is a view you cannot reach by clicking.
describe("chamfered cube facets", () => {
  const facets = chamferedCubeFacets(0.12);

  it("is the 26 views: 6 faces, 12 edges, 8 corners", () => {
    expect(facets.filter((f) => f.kind === "face")).toHaveLength(6);
    expect(facets.filter((f) => f.kind === "edge")).toHaveLength(12);
    expect(facets.filter((f) => f.kind === "corner")).toHaveLength(8);
    expect(facets).toHaveLength(26);
  });

  it("insets each face by the chamfer and keeps it on its plane", () => {
    const top = facets.find(
      (f) => f.kind === "face" && f.faces[0] === "TOP",
    )!;
    expect(top.vertices).toHaveLength(4);
    for (const [, , z] of top.vertices) {
      expect(z).toBeCloseTo(0.5, 9);
    }
    // 0.5 - 0.12 = 0.38
    expect(top.vertices.map(([x]) => Math.abs(x))).toEqual([
      0.38, 0.38, 0.38, 0.38,
    ]);
  });

  it("gives every edge strip two adjacent faces and four vertices", () => {
    for (const facet of facets.filter((f) => f.kind === "edge")) {
      expect(facet.faces).toHaveLength(2);
      expect(facet.vertices).toHaveLength(4);
      const normalA = FACE_NORMALS[facet.faces[0]];
      const normalB = FACE_NORMALS[facet.faces[1]];
      const axisA = [normalA.x, normalA.y, normalA.z].findIndex((v) => v !== 0);
      const axisB = [normalB.x, normalB.y, normalB.z].findIndex((v) => v !== 0);
      expect(axisA).not.toBe(axisB);
      for (const vertex of facet.vertices) {
        // One coordinate reaches the outer plane, the other stops at the inset.
        const values = vertex.map((v) => Math.abs(v)).sort();
        expect(values[2]).toBeCloseTo(0.5, 9);
        expect(values[1]).toBeCloseTo(0.38, 9);
        // Nothing sticks out of the cube.
        expect(Math.max(...vertex.map((v) => Math.abs(v)))).toBeLessThanOrEqual(
          0.5 + 1e-9,
        );
      }
    }
  });

  it("makes each corner a triangle between three different faces", () => {
    const corners = facets.filter((f) => f.kind === "corner");
    for (const facet of corners) {
      expect(facet.faces).toHaveLength(3);
      expect(new Set(facet.faces).size).toBe(3);
      expect(facet.vertices).toHaveLength(3);
      // Each triangle touches the outer plane on one different axis: that is
      // what makes it the corner between those three faces.
      const outerAxes = facet.vertices.map((vertex) =>
        vertex.map((value) => Math.abs(value)).findIndex((v) => v > 0.49),
      );
      expect(outerAxes.sort()).toEqual([0, 1, 2]);
    }
  });

  it("shrinks the faces as the chamfer grows", () => {
    const small = chamferedCubeFacets(0.05);
    const large = chamferedCubeFacets(0.3);
    // The RIGHT face's first vertex: x is the outer plane, y is the inset.
    const faceInset = (list: typeof facets) =>
      Math.abs(list.find((f) => f.kind === "face")!.vertices[0][1]);
    expect(faceInset(small)).toBeGreaterThan(faceInset(large));
    expect(faceInset(small)).toBeCloseTo(0.45, 9);
    expect(faceInset(large)).toBeCloseTo(0.2, 9);
    // Clamped: a chamfer past 0.45 cannot turn the cube inside out.
    const clamped = chamferedCubeFacets(0.9);
    expect(
      Math.min(...clamped.flatMap((f) => f.vertices.flat().map(Math.abs))),
    ).toBeGreaterThan(0);
  });
});

// A snap has to land in a STANDARD view. The bug behind "I get stuck in between
// axes at 45 degrees": the up vector was quantized in 45° steps, so a corner
// snap stored a diagonal up and every later face click inherited it — clicking
// TOP gave a top view rolled 45°.
describe("view cube snap orientation", () => {
  const TOP = new THREE.Vector3(0, 0, 1);
  const FRONT = new THREE.Vector3(0, -1, 0);

  function isAxisAligned(v: THREE.Vector3) {
    const components = [Math.abs(v.x), Math.abs(v.y), Math.abs(v.z)];
    const significant = components.filter((c) => c > 1e-6);
    return significant.every((c) => Math.abs(c - 1) < 1e-6) && significant.length === 1;
  }

  it("gives an upright top view from an upright camera", () => {
    const up = getQuantizedCubeUp(TOP, new THREE.Vector3(0, 0, 1));
    expect(up.x).toBeCloseTo(0, 9);
    expect(up.y).toBeCloseTo(1, 9);
    expect(up.z).toBeCloseTo(0, 9);
  });

  it("keeps a front view upright", () => {
    const up = getQuantizedCubeUp(FRONT, new THREE.Vector3(0, 0, 1));
    expect(up.z).toBeCloseTo(1, 9);
  });

  it("never returns a diagonal up, whatever the camera was rolled to", () => {
    // Every 5°, including the 45° rolls a corner snap used to leave behind.
    for (let degrees = 0; degrees < 360; degrees += 5) {
      const rolled = new THREE.Vector3(0, 0, 1).applyAxisAngle(
        new THREE.Vector3(1, 0, 0),
        THREE.MathUtils.degToRad(degrees),
      );
      for (const direction of [TOP, FRONT, new THREE.Vector3(0, 0, -1)]) {
        const up = getQuantizedCubeUp(direction, rolled.clone());
        expect(isAxisAligned(up)).toBe(true);
        // And it must be perpendicular to the view direction.
        expect(Math.abs(up.dot(direction.clone().normalize()))).toBeLessThan(1e-6);
      }
    }
  });

  it("gives a corner six orientations (60°), not four", () => {
    // A corner's chamfer facet is a triangle: its symmetry is threefold, so the
    // roll quantizes in 60° steps (FreeCAD's navigator does the same). A 90°
    // step would leave the corner view rotated by 30°.
    const corner = new THREE.Vector3(1, 1, 1).normalize();
    const baseUp = new THREE.Vector3(0, 0, 1)
      .addScaledVector(corner, -corner.dot(new THREE.Vector3(0, 0, 1)))
      .normalize();

    for (let degrees = 0; degrees < 360; degrees += 7) {
      const rolled = new THREE.Vector3(0, 0, 1).applyAxisAngle(
        new THREE.Vector3(1, 0, 0),
        THREE.MathUtils.degToRad(degrees),
      );
      const up = getQuantizedCubeUp(corner, rolled);
      // Perpendicular to the view direction.
      expect(Math.abs(up.dot(corner))).toBeLessThan(1e-6);
      // And one of the six 60° steps away from the base orientation of the
      // projected world up. (A 90° grid would produce 30°-ish residuals.)
      const angle = THREE.MathUtils.radToDeg(
        Math.acos(THREE.MathUtils.clamp(up.dot(baseUp), -1, 1)),
      );
      const residual = Math.abs(angle / 60 - Math.round(angle / 60)) * 60;
      // Degrees of error: a 90° grid would leave ~30° here, so this is a wide
      // margin against float noise in acos.
      expect(residual).toBeLessThan(1e-3);
    }
  });

  it("keeps the nearest quarter-turn heading for a face", () => {
    // Looking from the top with the camera rolled 20° about the vertical: the
    // nearest quarter turn is +Y, so the view keeps its heading.
    const rolled = new THREE.Vector3(0, 1, 0).applyAxisAngle(
      new THREE.Vector3(0, 0, 1),
      THREE.MathUtils.degToRad(20),
    );
    const nearUpright = getQuantizedCubeUp(TOP, rolled);
    expect(nearUpright.y).toBeCloseTo(1, 9);

    // Rolled 80° the nearest quarter turn is -X: still axis-aligned, and still
    // the heading the user is closest to.
    const rolledFar = new THREE.Vector3(0, 1, 0).applyAxisAngle(
      new THREE.Vector3(0, 0, 1),
      THREE.MathUtils.degToRad(80),
    );
    const quarterTurn = getQuantizedCubeUp(TOP, rolledFar);
    expect(quarterTurn.x).toBeCloseTo(-1, 9);
  });
});
// The four orbit arrows and the backside flip. Camera-centric: the eye moves the
// way the arrow points, by whole quarter turns, around the pivot.
describe("orbitCameraQuarterTurns", () => {
  function rig(position: [number, number, number]) {
    const camera = new THREE.OrthographicCamera(-5, 5, 5, -5, 0.1, 1000);
    camera.up.set(0, 0, 1);
    camera.position.set(...position);
    const target = new THREE.Vector3(0, 0, 0);
    camera.lookAt(target);
    const controls = {
      target,
      update: () => camera.lookAt(target),
    };
    return { camera, controls };
  }

  it("moves east and west around the vertical", () => {
    const east = rig([0, -500, 0]);
    orbitCameraQuarterTurns(east.camera, east.controls, "up", 1);
    expect(east.camera.position.x).toBeCloseTo(500, 6);
    expect(east.camera.position.y).toBeCloseTo(0, 6);

    const west = rig([0, -500, 0]);
    orbitCameraQuarterTurns(west.camera, west.controls, "up", -1);
    expect(west.camera.position.x).toBeCloseTo(-500, 6);
  });

  it("lifts the eye north and drops it south, keeping the distance", () => {
    const north = rig([0, -500, 0]);
    orbitCameraQuarterTurns(north.camera, north.controls, "right", -1);
    expect(north.camera.position.z).toBeCloseTo(500, 6);
    expect(north.camera.position.length()).toBeCloseTo(500, 6);

    const south = rig([0, -500, 0]);
    orbitCameraQuarterTurns(south.camera, south.controls, "right", 1);
    expect(south.camera.position.z).toBeCloseTo(-500, 6);
  });

  it("flips to the other side for the backside button", () => {
    const { camera, controls } = rig([0, -500, 0]);
    orbitCameraQuarterTurns(camera, controls, "up", 2);
    expect(camera.position.y).toBeCloseTo(500, 6);
    expect(camera.position.x).toBeCloseTo(0, 6);
  });

  it("never puts the eye exactly on the pole", () => {
    const { camera, controls } = rig([0, -500, 0]);
    // Two north turns: straight up, where an unclamped lookAt would roll.
    orbitCameraQuarterTurns(camera, controls, "right", -1);
    orbitCameraQuarterTurns(camera, controls, "right", -1);
    const elevation = Math.asin(
      THREE.MathUtils.clamp(
        camera.position.clone().normalize().dot(new THREE.Vector3(0, 0, 1)),
        -1,
        1,
      ),
    );
    expect(elevation).toBeLessThanOrEqual(Math.PI / 2 - 0.009);
  });

  it("keeps the pivot where it was", () => {
    const { camera, controls } = rig([0, -500, 0]);
    controls.target.set(10, 20, 30);
    camera.position.set(10, -480, 30);
    orbitCameraQuarterTurns(camera, controls, "up", 1);
    expect(controls.target.x).toBeCloseTo(10, 9);
    expect(controls.target.y).toBeCloseTo(20, 9);
    expect(controls.target.z).toBeCloseTo(30, 9);
  });
});