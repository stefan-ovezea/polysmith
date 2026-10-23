// @vitest-environment node
// The UI's B-spline math must match the core exactly: the draft preview, the
// profile pick contour and the fit-point solve all depend on it.
//
// Two regressions this pins:
//   * De Boor must walk DOWN from the span each round. The upward version (in
//     both this module and its C++ twin) blended values it had already
//     overwritten, so a simple 4-pole cubic did not pass through its own
//     control points.
//   * fitSplinePoles must reproduce the core's poles value for value, or the
//     preview would show a different curve than the one the core creates.
import { describe, expect, it } from "vitest";

import { fitSplinePoles, splineEval, splineOpenUniformKnots } from "./splineCurve";

describe("splineEval", () => {
  it("evaluates a single-segment cubic as its Bezier curve", () => {
    // 4 poles, degree 3 => one clamped segment: the curve IS the Bezier curve.
    const knots = splineOpenUniformKnots(4, 3);
    const xs = [0, 3, 7, 10];
    const ys = [0, 4, 5, 0];
    // B(1/3) = (8*P0 + 12*P1 + 6*P2 + P3) / 27.
    const [x, y] = splineEval(3, knots, xs, ys, 1 / 3);
    expect(x).toBeCloseTo((8 * 0 + 12 * 3 + 6 * 7 + 10) / 27, 12);
    expect(y).toBeCloseTo((8 * 0 + 12 * 4 + 6 * 5 + 0) / 27, 12);
  });

  it("clamps to the first and last pole", () => {
    const knots = splineOpenUniformKnots(6, 3);
    const xs = [0, 2, 4, 7, 9, 11];
    const ys = [0, 5, 7, 7, 5, 0];
    expect(splineEval(3, knots, xs, ys, 0)).toEqual([0, 0]);
    expect(splineEval(3, knots, xs, ys, 1)).toEqual([11, 0]);
  });

  it("is not fooled by a degree-5 curve", () => {
    const knots = splineOpenUniformKnots(6, 5);
    const xs = [0, 1, 2, 3, 4, 5];
    const ys = [0, 1, 0, 1, 0, 1];
    const [x, y] = splineEval(5, knots, xs, ys, 1 / 3);
    expect(Number.isFinite(x)).toBe(true);
    expect(Number.isFinite(y)).toBe(true);
  });
});

describe("fitSplinePoles", () => {
  it("reproduces the core's poles for a known fit", () => {
    // Cross-checked against tests/spline_test.cpp (16-bit exact fractions):
    // the fit through (0,0) (3,4) (7,5) (10,0) at degree 3 solves to
    // (0,0) (11/6, 9/2) (49/6, 9) (10,0).
    const poles = fitSplinePoles(
      [
        [0, 0],
        [3, 4],
        [7, 5],
        [10, 0],
      ],
      3,
    );
    expect(poles).toHaveLength(4);
    expect(poles[0][0]).toBeCloseTo(0, 12);
    expect(poles[0][1]).toBeCloseTo(0, 12);
    expect(poles[1][0]).toBeCloseTo(11 / 6, 12);
    expect(poles[1][1]).toBeCloseTo(9 / 2, 12);
    expect(poles[2][0]).toBeCloseTo(49 / 6, 12);
    expect(poles[2][1]).toBeCloseTo(9, 12);
    expect(poles[3][0]).toBeCloseTo(10, 12);
    expect(poles[3][1]).toBeCloseTo(0, 12);
  });

  it("interpolates every fit point at its Greville parameter", () => {
    const fit: Array<[number, number]> = [
      [0, 0],
      [2, 4],
      [4, 6],
      [7, 6.5],
      [10, 5],
      [12, 2],
      [14, 0],
    ];
    const degree = 5;
    const knots = splineOpenUniformKnots(fit.length, degree);
    const poles = fitSplinePoles(fit, degree);
    const xs = poles.map((p) => p[0]);
    const ys = poles.map((p) => p[1]);
    fit.forEach((point, i) => {
      let sum = 0;
      for (let j = 1; j <= degree; j++) sum += knots[i + j];
      const u = sum / degree;
      const [x, y] = splineEval(degree, knots, xs, ys, u >= 1 ? 1 : u);
      expect(x).toBeCloseTo(point[0], 6);
      expect(y).toBeCloseTo(point[1], 6);
    });
  });

  it("refuses a single point", () => {
    expect(() => fitSplinePoles([[1, 1]], 3)).toThrow(/at least 2/);
  });
});
