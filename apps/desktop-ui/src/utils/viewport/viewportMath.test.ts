import { describe, expect, it } from "vitest";
import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";

import { fitCameraToBounds, sketchCameraUp, targetLeftTheModel } from "./viewportMath";

function makeFrame(
  normal: [number, number, number],
  yAxis: [number, number, number],
): SketchPlaneFrame {
  const vec = ([x, y, z]: [number, number, number]) => ({ x, y, z });
  // x = y x n, the same right-handed basis the core derives.
  const [yx, yy, yz] = yAxis;
  const [nx, ny, nz] = normal;
  return {
    origin: vec([0, 0, 0]),
    x_axis: vec([yy * nz - yz * ny, yz * nx - yx * nz, yx * ny - yy * nx]),
    y_axis: vec(yAxis),
    normal: vec(normal),
  };
}

// The defect this pins (2026-11-17): framing a sketch on a face whose
// normal is vertical used a hardcoded world -Y as the camera up, so every
// face-based sketch opened rolled by 180 degrees — the ViewCube drew its
// "Top" label upside down even though the sketch axes were correct.
describe("sketchCameraUp", () => {
  it("keeps a top-face sketch upright (+Y up, not -Y)", () => {
    const up = sketchCameraUp(makeFrame([0, 0, 1], [0, 1, 0]));
    expect(up.x).toBeCloseTo(0, 9);
    expect(up.y).toBeCloseTo(1, 9);
    expect(up.z).toBeCloseTo(0, 9);
  });

  it("keeps a bottom-face sketch upright as well", () => {
    const up = sketchCameraUp(makeFrame([0, 0, -1], [0, 1, 0]));
    expect(up.y).toBeCloseTo(1, 9);
  });

  it("follows the sketch +Y on a vertical face", () => {
    const up = sketchCameraUp(makeFrame([1, 0, 0], [0, 0, 1]));
    expect(up.x).toBeCloseTo(0, 9);
    expect(up.y).toBeCloseTo(0, 9);
    expect(up.z).toBeCloseTo(1, 9);
  });

  it("follows a tilted frame instead of snapping to a world axis", () => {
    const k = Math.SQRT1_2;
    const up = sketchCameraUp(makeFrame([k, 0, k], [-k, 0, k]));
    expect(up.x).toBeCloseTo(-k, 9);
    expect(up.y).toBeCloseTo(0, 9);
    expect(up.z).toBeCloseTo(k, 9);
  });

  it("falls back to the ViewCube rule when the frame is degenerate", () => {
    // +Y parallel to the normal cannot orient the view; a vertical view
    // direction falls back to world +Y (getDefaultUpForDirection).
    const up = sketchCameraUp(makeFrame([0, 0, 1], [0, 0, 1]));
    expect(up.y).toBeCloseTo(1, 9);
  });
});

// The orbit pivot was only ever derived on the FIRST geometry: import a board
// into a document that already had a model and the target stayed at the old
// model's place, so every turn swung the new one out of the view.
describe("targetLeftTheModel", () => {
  const vec = (x: number, y: number, z: number) => new THREE.Vector3(x, y, z);

  it("is false while the camera still looks at the model", () => {
    // 200 mm board centred at (150, 100, 0): a target inside it is fine.
    expect(targetLeftTheModel(vec(150, 100, 0), [150, 100, 0], 200)).toBe(false);
    // Even a target 20 mm off (panning, an edge) keeps the user's frame.
    expect(targetLeftTheModel(vec(150, 120, 0), [150, 100, 0], 200)).toBe(false);
  });

  it("is true once the content is elsewhere", () => {
    // The old model sat at the origin; the import is 150 mm away.
    expect(targetLeftTheModel(vec(0, 0, 0), [150, 100, 0], 200)).toBe(true);
  });

  it("treats a tiny model with a sub-millimetre floor", () => {
    expect(targetLeftTheModel(vec(0.4, 0, 0), [0, 0, 0], 0.1)).toBe(false);
    expect(targetLeftTheModel(vec(0.6, 0, 0), [0, 0, 0], 0.1)).toBe(true);
  });
});

// Fit must frame the model WITHOUT reorienting it — that is the whole difference
// between the cube's Fit and its Home button.
describe("fitCameraToBounds", () => {
  function rig(position: [number, number, number]) {
    const camera = new THREE.OrthographicCamera(-50, 50, 50, -50, 0.1, 10000);
    camera.up.set(0, 0, 1);
    camera.position.set(...position);
    const target = new THREE.Vector3(0, 0, 0);
    camera.lookAt(target);
    const controls = {
      target,
      update: () => camera.lookAt(target),
    } as unknown as Parameters<typeof fitCameraToBounds>[1];
    return { camera, controls, target };
  }

  it("moves the pivot to the model and keeps the direction", () => {
    const { camera, controls, target } = rig([700, 500, 1000]);
    const before = camera.position.clone().sub(target).normalize();
    fitCameraToBounds(camera, controls, [150, 100, 0], 200);
    const after = camera.position.clone().sub(controls.target).normalize();
    expect(controls.target.x).toBeCloseTo(150, 6);
    expect(controls.target.y).toBeCloseTo(100, 6);
    expect(after.x).toBeCloseTo(before.x, 6);
    expect(after.y).toBeCloseTo(before.y, 6);
    expect(after.z).toBeCloseTo(before.z, 6);
  });

  it("zooms so the model fills the view", () => {
    const { camera, controls } = rig([700, 500, 1000]);
    fitCameraToBounds(camera, controls, [0, 0, 0], 200);
    // viewHeight = max(200 * 2.4, 120) = 480; frustum height = 100
    expect(camera.zoom).toBeCloseTo(100 / 480, 9);
    // Distance follows the same rule as frameCamera.
    expect(camera.position.distanceTo(controls.target)).toBeCloseTo(360, 6);
  });
});
