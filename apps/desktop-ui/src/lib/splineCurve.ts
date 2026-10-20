// Clamped open-uniform B-spline math shared by the spline draft preview and the
// profile pick contour.
//
// This mirrors `native/cad-core/src/core/sketch/spline_math.h` exactly: degree =
// min(3, poles-1), parameter space normalised to [0, 1], knots clamped at both
// ends.  The UI needs it because profile picking must follow the REAL curve — a
// chord across a spline edge cuts corners, and where that chord crosses a
// neighbouring edge the pick polygon self-intersects and the region stops being
// selectable.

export function splineOpenUniformKnots(
  nPoles: number,
  degree: number,
): number[] {
  const interior = nPoles - degree - 1;
  const total = nPoles + degree + 1;
  const knots = new Array<number>(total).fill(0);
  for (let i = degree + 1; i < nPoles; i++) {
    knots[i] = (i - degree) / (interior + 1);
  }
  for (let i = nPoles; i < total; i++) {
    knots[i] = 1;
  }
  return knots;
}

// De Boor evaluation of a clamped B-spline, u in [0, 1].
export function splineEval(
  degree: number,
  knots: number[],
  poleXs: number[],
  poleYs: number[],
  u: number,
): [number, number] {
  const n = poleXs.length;
  if (n < 2) {
    return [poleXs[0] ?? 0, poleYs[0] ?? 0];
  }
  if (u <= 0) {
    return [poleXs[0], poleYs[0]];
  }
  if (u >= 1) {
    return [poleXs[n - 1], poleYs[n - 1]];
  }
  let span = n - 1;
  for (let i = 0; i + 1 < knots.length; i++) {
    if (u >= knots[i] && u < knots[i + 1]) {
      span = i;
      break;
    }
  }
  const dx = poleXs.slice();
  const dy = poleYs.slice();
  for (let r = 1; r <= degree; r++) {
    for (let i = span - degree + r; i <= span; i++) {
      const denom = knots[i + degree - r + 1] - knots[i];
      const alpha = denom > 1e-15 ? (u - knots[i]) / denom : 0;
      dx[i] = (1 - alpha) * dx[i - 1] + alpha * dx[i];
      dy[i] = (1 - alpha) * dy[i - 1] + alpha * dy[i];
    }
  }
  return [dx[span], dy[span]];
}
