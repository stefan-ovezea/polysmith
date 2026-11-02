import { describe, expect, it } from "vitest";

import { chooseArcSweep } from "./arcSweep";

const TWO_PI = Math.PI * 2;
const deg = (d: number) => (d * Math.PI) / 180;

// The defect this pins: a trimmed arc dragged past its own endpoint had its
// endpoints' angular order swap, the normalized gap became the complement, and
// the deleted part of the circle was drawn instead of the arc.
describe("chooseArcSweep", () => {
  it("keeps a major ccw arc when the endpoints' angular order swaps", () => {
    // Drawn as a 300° arc; the endpoints now read only 60° apart.
    const sweep = chooseArcSweep({
      startAngle: 0,
      endAngle: deg(60),
      ccw: true,
      previousSweep: deg(300),
    });
    // The complement of a 60° gap is the 300° arc, traversed backwards.
    expect(Math.abs(sweep)).toBeCloseTo(deg(300), 9);
    expect(sweep).toBeLessThan(0);
  });

  it("still draws the same point set after the swap", () => {
    const startAngle = deg(40);
    const sweep = chooseArcSweep({
      startAngle,
      endAngle: startAngle + deg(60),
      ccw: true,
      previousSweep: deg(300),
    });
    // Walking `sweep` from the start must land on the stated end, mod 2π.
    const landed = startAngle + sweep;
    const delta = (landed - (startAngle + deg(60))) % TWO_PI;
    expect(Math.min(Math.abs(delta), Math.abs(Math.abs(delta) - TWO_PI))).toBeCloseTo(
      0,
      9,
    );
  });

  it("tracks a legitimately shrinking arc instead of pinning it", () => {
    const sweep = chooseArcSweep({
      startAngle: 0,
      endAngle: deg(290),
      ccw: true,
      previousSweep: deg(300),
    });
    expect(sweep).toBeCloseTo(deg(290), 9);
  });

  it("mirrors the rule for a cw arc", () => {
    const sweep = chooseArcSweep({
      startAngle: 0,
      endAngle: -deg(60),
      ccw: false,
      previousSweep: -deg(300),
    });
    expect(Math.abs(sweep)).toBeCloseTo(deg(300), 9);
    expect(sweep).toBeGreaterThan(0);
  });

  it("falls back to the ccw-normalized gap when no span is known", () => {
    expect(
      chooseArcSweep({ startAngle: 0, endAngle: deg(60), ccw: true }),
    ).toBeCloseTo(deg(60), 9);
    expect(
      chooseArcSweep({ startAngle: 0, endAngle: deg(60), ccw: false }),
    ).toBeCloseTo(-deg(300), 9);
  });
});
