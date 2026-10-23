// @vitest-environment node
// Slot pick-mode arithmetic. The three modes differ only in what the two
// clicks mean; all of them must produce a payload the core accepts, whose
// hardest rule is `length >= 2 * radius` (a stadium needs a straight section
// at least as long as its caps).
import { describe, expect, it } from "vitest";

import {
  defaultArcSlotRadius,
  defaultSlotOverallRadius,
  defaultSlotRadius,
  slotArcDraftGeometry,
  slotDraftGeometry,
} from "./slotDraftPreview";

describe("slotDraftGeometry", () => {
  it("center_point: the first click is the slot centre, the second an arc centre", () => {
    const geometry = slotDraftGeometry("center_point", [10, 10], [20, 10]);
    expect(geometry).not.toBeNull();
    expect(geometry!.center).toEqual([10, 10]);
    expect(geometry!.length).toBeCloseTo(20, 9);
    expect(geometry!.radius).toBeCloseTo(defaultSlotRadius(20), 9);
    expect(geometry!.rotation).toBeCloseTo(0, 9);
  });

  it("center_to_center: the clicks are the two arc centres", () => {
    const geometry = slotDraftGeometry("center_to_center", [10, 10], [30, 10]);
    expect(geometry).not.toBeNull();
    expect(geometry!.center).toEqual([20, 10]);
    expect(geometry!.length).toBeCloseTo(20, 9);
    expect(geometry!.rotation).toBeCloseTo(0, 9);
  });

  it("overall: the clicks are the cap tips, so length is span - 2r", () => {
    const geometry = slotDraftGeometry("overall", [0, 0], [40, 0]);
    expect(geometry).not.toBeNull();
    expect(geometry!.center).toEqual([20, 0]);
    // Overall span = centre-to-centre length + the two caps.
    expect(geometry!.length + 2 * geometry!.radius).toBeCloseTo(40, 6);
    expect(geometry!.radius).toBeCloseTo(defaultSlotOverallRadius(40), 9);
  });

  it("keeps the core's length >= 2r rule for every mode across a span sweep", () => {
    for (const mode of ["center_point", "center_to_center", "overall"] as const) {
      for (let span = 0.5; span <= 200; span += 0.5) {
        const geometry = slotDraftGeometry(mode, [0, 0], [span, 0]);
        expect(geometry, `${mode} span ${span}`).not.toBeNull();
        expect(geometry!.length, `${mode} span ${span}`).toBeGreaterThanOrEqual(
          2 * geometry!.radius - 1e-9,
        );
        expect(geometry!.radius, `${mode} span ${span}`).toBeGreaterThan(0);
      }
    }
  });

  it("returns null for a degenerate drag", () => {
    expect(slotDraftGeometry("center_point", [5, 5], [5, 5])).toBeNull();
  });
});

// T2.3 arc slots: click 1 is the ARC centre, click 2 a point on the centreline
// (which fixes the centreline radius and the start angle); the sweep is the
// panel value. The half width must keep the inner boundary positive.
describe("slotArcDraftGeometry", () => {
  const sweep = Math.PI / 2;

  it("uses the first click as the arc centre and the second for radius + start angle", () => {
    const geometry = slotArcDraftGeometry([0, 0], [10, 0], sweep);
    expect(geometry).not.toBeNull();
    expect(geometry!.center).toEqual([0, 0]);
    expect(geometry!.arcRadius).toBeCloseTo(10, 9);
    expect(geometry!.startAngle).toBeCloseTo(0, 9);
    expect(geometry!.sweepAngle).toBeCloseTo(sweep, 9);
    expect(geometry!.radius).toBeCloseTo(defaultArcSlotRadius(10, sweep), 9);
  });

  it("takes the start angle from the direction of the second click", () => {
    const geometry = slotArcDraftGeometry([5, 5], [5, 15], sweep);
    expect(geometry!.startAngle).toBeCloseTo(Math.PI / 2, 9);
    expect(geometry!.arcRadius).toBeCloseTo(10, 9);
  });

  it("keeps the half width below the centreline radius (positive inner boundary)", () => {
    // A very short centreline radius would otherwise make the default width
    // larger than the radius itself.
    const geometry = slotArcDraftGeometry([0, 0], [0.6, 0], sweep);
    expect(geometry).not.toBeNull();
    expect(geometry!.radius).toBeLessThan(geometry!.arcRadius);
  });

  it("clamps the sweep into (0, 2*pi)", () => {
    expect(slotArcDraftGeometry([0, 0], [10, 0], 0)!.sweepAngle).toBeGreaterThan(0);
    expect(
      slotArcDraftGeometry([0, 0], [10, 0], 2 * Math.PI)!.sweepAngle,
    ).toBeLessThan(2 * Math.PI);
  });

  it("returns null for a degenerate drag", () => {
    expect(slotArcDraftGeometry([5, 5], [5, 5], sweep)).toBeNull();
  });
});
