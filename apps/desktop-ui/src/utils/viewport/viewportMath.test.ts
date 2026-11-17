import { describe, expect, it } from "vitest";

import type { SketchPlaneFrame } from "@/types";

import { sketchCameraUp } from "./viewportMath";

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
