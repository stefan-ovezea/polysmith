// @vitest-environment node
import { describe, expect, it } from "vitest";

import type { SketchDimensionScene } from "@/types";

import {
  collectDragMoves,
  followDimensionAnchors,
  followDimensionsForMoves,
  snapshotPointPositions,
} from "./dimensionFollow";

import type { Point3 } from "./dimensionFollow";

function length(a: Point3, b: Point3) {
  return Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

/** 2D cross product of the two SEGMENTS' directions (zero when they are parallel). */
function crossOf(a0: Point3, a1: Point3, b0: Point3, b1: Point3) {
  const ax = a1[0] - a0[0];
  const ay = a1[1] - a0[1];
  const bx = b1[0] - b0[0];
  const by = b1[1] - b0[1];
  return ax * by - ay * bx;
}

function makeDimension(
  overrides: Partial<SketchDimensionScene> = {},
): SketchDimensionScene {
  return {
    dimensionId: "dim-1",
    planeId: "sketch-1",
    kind: "line_length",
    entityId: "line-1",
    label: "10 mm",
    rawValue: 10,
    unitSuffix: " mm",
    isSelected: false,
    anchorStart: [0, 0, 0],
    anchorEnd: [10, 0, 0],
    dimensionStart: [0, 5, 0],
    dimensionEnd: [10, 5, 0],
    labelPosition: [5, 5, 0],
    ...overrides,
  };
}

describe("followDimensionAnchors", () => {
  it("carries the end anchor that sits on the dragged endpoint", () => {
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [10, 0, 0], to: [10, -2, 0] },
    ]);

    expect(followed).not.toBeNull();
    expect(followed?.anchorEnd).toEqual([10, -2, 0]);
    expect(followed?.anchorStart).toEqual([0, 0, 0]);

    // The pair ROTATED (the far end dropped), so the leader rotates with it: each
    // dimension-line end keeps its 5 mm offset from ITS anchor, and the leader stays
    // parallel to the pair with the pair's new length. A plain translation would
    // leave the dimension line world-horizontal and skew the leader.
    expect(length(followed!.dimensionStart, followed!.anchorStart)).toBeCloseTo(5, 6);
    expect(length(followed!.dimensionEnd, followed!.anchorEnd)).toBeCloseTo(5, 6);
    expect(
      crossOf(
        followed!.dimensionStart,
        followed!.dimensionEnd,
        followed!.anchorStart,
        followed!.anchorEnd,
      ),
    ).toBeCloseTo(0, 6);
    expect(length(followed!.dimensionStart, followed!.dimensionEnd)).toBeCloseTo(
      length(followed!.anchorStart, followed!.anchorEnd),
      6,
    );
  });

  it("keeps a translating pair's leader exactly as it was", () => {
    // No rotation, no scaling: the leader simply goes along.
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [0, 0, 0], to: [0, 3, 0] },
      { from: [10, 0, 0], to: [10, 3, 0] },
    ]);

    expect(followed?.anchorStart).toEqual([0, 3, 0]);
    expect(followed?.anchorEnd).toEqual([10, 3, 0]);
    expect(followed?.dimensionStart?.[0]).toBeCloseTo(0, 6);
    expect(followed?.dimensionStart?.[1]).toBeCloseTo(8, 6);
    expect(followed?.dimensionEnd?.[1]).toBeCloseTo(8, 6);
  });

  it("keeps the label at its own offset from the leader", () => {
    // The fixture's label sits exactly on the leader's middle, so it must stay
    // there once the leader rotates.
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [10, 0, 0], to: [10, -2, 0] },
    ]);
    const middleX = (followed!.dimensionStart[0] + followed!.dimensionEnd[0]) / 2;
    const middleY = (followed!.dimensionStart[1] + followed!.dimensionEnd[1]) / 2;

    expect(followed?.labelPosition?.[0]).toBeCloseTo(middleX, 6);
    expect(followed?.labelPosition?.[1]).toBeCloseTo(middleY, 6);
  });

  it("re-anchors a rotated leader at both ends (a group translate cannot)", () => {
    // The line both rotated and changed length: start up 1, end down 1.
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [0, 0, 0], to: [2, 1, 0] },
      { from: [10, 0, 0], to: [10, -1, 0] },
    ]);

    // Each anchor lands exactly on its own moved point, not on a shared rigid
    // offset â€” that is the difference from translating the group.
    expect(followed?.anchorStart).toEqual([2, 1, 0]);
    expect(followed?.anchorEnd).toEqual([10, -1, 0]);
    // â€¦and the leader spans them, parallel, with both offsets intact.
    expect(length(followed!.dimensionStart, followed!.anchorStart)).toBeCloseTo(5, 6);
    expect(length(followed!.dimensionEnd, followed!.anchorEnd)).toBeCloseTo(5, 6);
    expect(
      crossOf(
        followed!.dimensionStart,
        followed!.dimensionEnd,
        followed!.anchorStart,
        followed!.anchorEnd,
      ),
    ).toBeCloseTo(0, 6);
    expect(length(followed!.dimensionStart, followed!.dimensionEnd)).toBeCloseTo(
      length(followed!.anchorStart, followed!.anchorEnd),
      6,
    );
  });

  it("returns null when the dimension does not touch the moved point", () => {
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [99, 99, 0], to: [98, 99, 0] },
    ]);

    expect(followed).toBeNull();
  });

  it("returns null when nothing moved", () => {
    expect(followDimensionAnchors(makeDimension(), [])).toBeNull();
  });

  it("carries the centre of a radial dimension and leaves its radius alone", () => {
    // circle_radius: the anchors are derived from arcCenter, which is the
    // circle's centre. Moving the circle must move the whole leader.
    const dimension = makeDimension({
      kind: "circle_radius",
      entityId: "circle-1",
      anchorStart: [20, 0, 0],
      anchorEnd: [10, 0, 0],
      dimensionStart: [20, 4, 0],
      dimensionEnd: [10, 4, 0],
      labelPosition: [15, 4, 0],
      arcCenter: [0, 0, 0],
      arcRadius: 10,
    });

    const followed = followDimensionAnchors(dimension, [
      { from: [0, 0, 0], to: [5, 3, 0] },
    ]);

    expect(followed?.arcCenter).toEqual([5, 3, 0]);
    expect(followed?.arcRadius).toBe(10);
  });

  it("ignores a move that only grazes the anchor beyond tolerance", () => {
    // 1e-3 off is not the same point at the default 1e-6 tolerance; the
    // dimensions of a sketch can share a neighbourhood, so matching must be
    // exact rather than nearest.
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [10.001, 0, 0], to: [20, 0, 0] },
    ]);

    expect(followed).toBeNull();
  });
});

describe("collectDragMoves", () => {
  const base = snapshotPointPositions([
    { id: "vertex-1", position: [0, 0, 0] },
    { id: "vertex-2", position: [10, 0, 0] },
  ]);

  it("reports only the points the drag moved", () => {
    const moves = collectDragMoves(base, [
      { id: "vertex-1", position: [0, 0, 0] },
      { id: "vertex-2", position: [10, -2, 0] },
    ]);

    expect(moves).toEqual([{ from: [10, 0, 0], to: [10, -2, 0] }]);
  });

  it("ignores a point that appeared after the drag began", () => {
    // A point with no committed position has no anchor to carry, so it must not
    // be treated as moved from the origin.
    const moves = collectDragMoves(base, [
      { id: "vertex-9", position: [4, 4, 0] },
    ]);

    expect(moves).toEqual([]);
  });

  it("ignores sub-tolerance jitter", () => {
    const moves = collectDragMoves(base, [
      { id: "vertex-1", position: [1e-9, 0, 0] },
    ]);

    expect(moves).toEqual([]);
  });

  it("reports nothing when the drag has not moved anything yet", () => {
    expect(
      collectDragMoves(base, [
        { id: "vertex-1", position: [0, 0, 0] },
        { id: "vertex-2", position: [10, 0, 0] },
      ]),
    ).toEqual([]);
  });
});

describe("live value", () => {
  it("re-measures a length while the drag is still running", () => {
    // The core only measures on commit, so the label kept its pre-drag number.
    const followed = followDimensionAnchors(makeDimension(), [
      { from: [10, 0, 0], to: [10, 5, 0] },
    ]);

    // Anchors now 10 apart horizontally and 5 apart vertically: sqrt(125).
    expect(followed?.rawValue).toBeCloseTo(Math.sqrt(125), 6);
    expect(followed?.label).toBe("11.18 mm");
  });

  it("inherits the kind's convention instead of re-deriving it", () => {
    // A diameter dimension reads TWICE its anchor distance. Calibrating against
    // the committed pair (10 apart, value 20) keeps that factor, so the live
    // number stays a diameter rather than silently becoming a radius.
    const dimension = makeDimension({
      kind: "circle_radius",
      label: "D 20.00 mm",
      rawValue: 20,
      anchorStart: [10, 0, 0],
      anchorEnd: [0, 0, 0],
      dimensionStart: [10, 5, 0],
      dimensionEnd: [0, 5, 0],
      labelPosition: [5, 5, 0],
    });

    const followed = followDimensionAnchors(dimension, [
      { from: [10, 0, 0], to: [12, 0, 0] },
    ]);

    expect(followed?.rawValue).toBeCloseTo(24, 6);
    expect(followed?.label).toBe("D 24.00 mm");
  });

  it("leaves an angle's value alone", () => {
    // The anchor distance says nothing about an angle, so guessing would print a
    // wrong number.
    const dimension = makeDimension({
      kind: "angle",
      label: "45.00°",
      rawValue: 45,
    });

    const followed = followDimensionAnchors(dimension, [
      { from: [10, 0, 0], to: [10, 5, 0] },
    ]);

    expect(followed?.rawValue).toBe(45);
    expect(followed?.label).toBe("45.00°");
  });

  it("leaves the value alone when the pair has no baseline", () => {
    const dimension = makeDimension({
      anchorStart: [3, 3, 0],
      anchorEnd: [3, 3, 0],
    });

    const followed = followDimensionAnchors(dimension, [
      { from: [3, 3, 0], to: [4, 3, 0] },
    ]);

    expect(followed?.rawValue).toBe(10);
  });
});

describe("followDimensionsForMoves", () => {
  it("returns only the dimensions that moved, leaving the others alone", () => {
    const dimensions = [
      makeDimension({ dimensionId: "dim-line-1", entityId: "line-1" }),
      makeDimension({
        dimensionId: "dim-line-2",
        entityId: "line-2",
        anchorStart: [50, 0, 0],
        anchorEnd: [60, 0, 0],
        dimensionStart: [50, 5, 0],
        dimensionEnd: [60, 5, 0],
        labelPosition: [55, 5, 0],
      }),
    ];

    const followed = followDimensionsForMoves(dimensions, [
      { from: [10, 0, 0], to: [10, -2, 0] },
    ]);

    expect(followed.map((dimension) => dimension.dimensionId)).toEqual([
      "dim-line-1",
    ]);
  });
});
