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
  // De Boor walks DOWN from the span each round, so dx[i-1] is still the
  // previous round's value. An upward loop blends a value it has already
  // overwritten and evaluates a different curve — the C++ twin
  // (native/cad-core/src/core/sketch/spline_math.h) had the same bug, so both
  // sides were drawing/sampling a curve that missed its own poles.
  for (let r = 1; r <= degree; r++) {
    for (let i = span; i >= span - degree + r; i--) {
      const denom = knots[i + degree - r + 1] - knots[i];
      const alpha = denom > 1e-15 ? (u - knots[i]) / denom : 0;
      dx[i] = (1 - alpha) * dx[i - 1] + alpha * dx[i];
      dy[i] = (1 - alpha) * dy[i - 1] + alpha * dy[i];
    }
  }
  return [dx[span], dy[span]];
}

/** Knot span containing u (Piegl & Tiller A2.1); u == 1 maps to the last pole. */
function splineFindSpan(
  poleCount: number,
  degree: number,
  knots: number[],
  u: number,
): number {
  if (u >= knots[poleCount]) {
    return poleCount - 1;
  }
  let low = degree;
  let high = poleCount;
  let mid = Math.floor((low + high) / 2);
  while (u < knots[mid] || u >= knots[mid + 1]) {
    if (u < knots[mid]) {
      high = mid;
    } else {
      low = mid;
    }
    mid = Math.floor((low + high) / 2);
  }
  return mid;
}

/**
 * The degree+1 non-zero basis values at u (Piegl & Tiller A2.3), for poles
 * span-degree .. span. Returns the span; `values` is length degree+1.
 *
 * Computing the whole row in one pass is what keeps the knot indices
 * non-negative at the clamped ends — evaluating a single basis function in
 * isolation walks before the start of the knot vector when u sits on the
 * curve's first knot.
 */
function splineBasisRow(
  poleCount: number,
  degree: number,
  knots: number[],
  u: number,
): { span: number; values: number[] } {
  const span = splineFindSpan(poleCount, degree, knots, u);
  const values = new Array<number>(degree + 1).fill(0);
  const left = new Array<number>(degree + 1).fill(0);
  const right = new Array<number>(degree + 1).fill(0);
  values[0] = 1;
  for (let j = 1; j <= degree; j++) {
    left[j] = u - knots[span + 1 - j];
    right[j] = knots[span + j] - u;
    let saved = 0;
    for (let r = 0; r < j; r++) {
      const denom = right[r + 1] + left[j - r];
      const temp = denom > 1e-15 ? values[r] / denom : 0;
      values[r] = saved + right[r + 1] * temp;
      saved = left[j - r] * temp;
    }
    values[j] = saved;
  }
  return { span, values };
}

/** Highest degree a sketch spline may request (mirrors kSplineMaxDegree). */
export const SPLINE_MAX_DEGREE = 5;

/** Largest point count the interpolation solve accepts (mirrors kSplineFitMaxPoints). */
export const SPLINE_FIT_MAX_POINTS = 256;

/**
 * Poles of the clamped B-spline of `degree` that passes through every
 * `fitPoints` entry, at the Greville abscissae of the open-uniform knot vector.
 *
 * Mirrors `spline_fit_poles` in native/cad-core/src/core/sketch/spline_math.h
 * value for value: the preview must show the curve the core will create, not
 * an approximation of it. The knot vector is the same open-uniform one the
 * pipeline evaluates with, so the solved poles are meaningful downstream.
 */
export function fitSplinePoles(
  fitPoints: ReadonlyArray<readonly [number, number]>,
  degree: number,
): Array<[number, number]> {
  const n = fitPoints.length;
  if (n < 2) {
    throw new Error("A fit-point spline needs at least 2 points");
  }
  if (n > SPLINE_FIT_MAX_POINTS) {
    throw new Error(
      `A fit-point spline accepts at most ${SPLINE_FIT_MAX_POINTS} points`,
    );
  }
  const p = Math.max(1, Math.min(degree, n - 1));
  const knots = splineOpenUniformKnots(n, p);

  // Greville abscissae: u_i = (knots[i+1] + ... + knots[i+p]) / p.
  const params = new Array<number>(n).fill(0);
  for (let i = 0; i < n; i++) {
    let sum = 0;
    for (let j = 1; j <= p; j++) {
      sum += knots[i + j];
    }
    params[i] = sum / p;
  }

  const matrix: number[][] = Array.from({ length: n }, () =>
    new Array<number>(n).fill(0),
  );
  for (let i = 0; i < n; i++) {
    // The basis row is defined on the half-open span [u_k, u_{k+1}); the last
    // Greville parameter is exactly 1, which belongs to no span.
    let u = params[i];
    if (u >= 1) {
      u = 1 - Number.EPSILON;
    }
    const { span, values } = splineBasisRow(n, p, knots, u);
    for (let j = 0; j <= p; j++) {
      matrix[i][span - p + j] = values[j];
    }
  }

  const rhsX = fitPoints.map((point) => point[0]);
  const rhsY = fitPoints.map((point) => point[1]);
  for (let col = 0; col < n; col++) {
    let pivot = col;
    let best = Math.abs(matrix[col][col]);
    for (let row = col + 1; row < n; row++) {
      const value = Math.abs(matrix[row][col]);
      if (value > best) {
        best = value;
        pivot = row;
      }
    }
    if (best < 1e-12) {
      throw new Error("Fit-point spline: singular interpolation system");
    }
    if (pivot !== col) {
      [matrix[pivot], matrix[col]] = [matrix[col], matrix[pivot]];
      [rhsX[pivot], rhsX[col]] = [rhsX[col], rhsX[pivot]];
      [rhsY[pivot], rhsY[col]] = [rhsY[col], rhsY[pivot]];
    }
    for (let row = col + 1; row < n; row++) {
      const factor = matrix[row][col] / matrix[col][col];
      if (factor === 0) continue;
      for (let k = col; k < n; k++) {
        matrix[row][k] -= factor * matrix[col][k];
      }
      rhsX[row] -= factor * rhsX[col];
      rhsY[row] -= factor * rhsY[col];
    }
  }

  const poles = new Array<[number, number]>(n);
  for (let row = n - 1; row >= 0; row--) {
    let x = rhsX[row];
    let y = rhsY[row];
    for (let k = row + 1; k < n; k++) {
      x -= matrix[row][k] * poles[k][0];
      y -= matrix[row][k] * poles[k][1];
    }
    const diag = matrix[row][row];
    poles[row] = [x / diag, y / diag];
  }
  return poles;
}
