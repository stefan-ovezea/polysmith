// @vitest-environment node
import { describe, expect, it } from "vitest";

import { resolveCircleResizeRadius } from "./circleResizeDrag";

describe("resolveCircleResizeRadius", () => {
  it("returns the distance from the cursor to the fixed centre", () => {
    // Centre (0,0), cursor at (3,4) → radius 5.
    expect(resolveCircleResizeRadius([3, 4], 0, 0)).toBeCloseTo(5, 6);
  });

  it("is translation-invariant (centre moves, result is the relative distance)", () => {
    // Centre (5,5), cursor at (8,9) → radius 5 (the same 3,4 offset).
    expect(resolveCircleResizeRadius([8, 9], 5, 5)).toBeCloseTo(5, 6);
  });

  it("clamps to the minimum radius at the centre", () => {
    // Cursor exactly on the centre would otherwise collapse the circle.
    expect(resolveCircleResizeRadius([0, 0], 0, 0)).toBeGreaterThan(0);
    expect(resolveCircleResizeRadius([0, 0], 0, 0, 1e-3)).toBeCloseTo(1e-3, 9);
  });
});
