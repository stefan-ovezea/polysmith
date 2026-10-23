// @vitest-environment node
// Draft-preview maths for the rounded-rectangle tool mode. The preview is a
// rubber band of straight segments (the committed geometry is the core's real
// fillet arcs), so the contract is: stay inside the dragged rectangle, keep
// the corner radius, and degenerate gracefully while the user drags out a
// rectangle that is still too small for the current radius.
import { describe, expect, it } from "vitest";

import {
  roundedPreviewCornerSegments,
  roundedRectangleOutline2d,
} from "./rectangleDraftPreview";

const START: [number, number] = [0, 0];
const END: [number, number] = [40, 20];

describe("roundedRectangleOutline2d", () => {
  it("replaces every corner with an arc of the requested radius", () => {
    const points = roundedRectangleOutline2d(START, END, 5);
    // 1 straight-edge start + 4 corners, each tessellated per the
    // sagitta bound.
    const perCorner = roundedPreviewCornerSegments(5);
    expect(perCorner).toBeGreaterThanOrEqual(8);
    expect(points).toHaveLength(1 + 4 * perCorner);

    // The sharp corners themselves are gone.
    for (const [x, y] of points) {
      const atSharpCorner =
        (x === 0 || x === 40) && (y === 0 || y === 20);
      expect(atSharpCorner).toBe(false);
    }

    // Every point is inside the rectangle...
    for (const [x, y] of points) {
      expect(x).toBeGreaterThanOrEqual(0);
      expect(x).toBeLessThanOrEqual(40);
      expect(y).toBeGreaterThanOrEqual(0);
      expect(y).toBeLessThanOrEqual(20);
    }

    // ...and each corner's points sit 5 mm from its own arc centre.
    const centres: Array<[number, number]> = [
      [35, 5],
      [35, 15],
      [5, 15],
      [5, 5],
    ];
    for (const [x, y] of points) {
      if (!(x === 0 || x === 40 || y === 0 || y === 20)) {
        continue;
      }
      const nearest = Math.min(
        ...centres.map(([cx, cy]) => Math.hypot(x - cx, y - cy)),
      );
      expect(nearest).toBeCloseTo(5, 6);
    }
  });

  it("keeps drawing when the radius is larger than half the shorter side", () => {
    // 20 mm tall rectangle: half the shorter side is 10. A 30 mm radius must
    // clamp instead of producing nonsense while the user is still dragging.
    const points = roundedRectangleOutline2d(START, END, 30);
    expect(points.length).toBeGreaterThan(5);
    for (const [, y] of points) {
      expect(y).toBeGreaterThanOrEqual(0);
      expect(y).toBeLessThanOrEqual(20);
    }
  });

  it("falls back to a plain rectangle when the radius is zero", () => {
    expect(roundedRectangleOutline2d(START, END, 0)).toEqual([
      [0, 0],
      [40, 0],
      [40, 20],
      [0, 20],
      [0, 0],
    ]);
  });

  it("tessellates finer as the radius grows (no faceted corners)", () => {
    // The sagitta bound: a bigger radius must get more chords per corner,
    // otherwise the draft reads as a chamfered polygon at large radii.
    expect(roundedPreviewCornerSegments(50)).toBeGreaterThan(
      roundedPreviewCornerSegments(5),
    );
    // And never a silly number of points.
    expect(roundedPreviewCornerSegments(5000)).toBeLessThanOrEqual(64);
    expect(roundedPreviewCornerSegments(0)).toBeGreaterThanOrEqual(8);
  });
});
