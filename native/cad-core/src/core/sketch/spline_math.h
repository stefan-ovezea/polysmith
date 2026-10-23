#pragma once

// Control-point B-spline helpers shared by the sketch spline entity,
// the profile engine, and the viewport emitter.
//
// The sketch spline is a CLAMPED (open-uniform) B-spline whose poles
// are the user's control points. The parameter space is normalised to
// [0, 1] regardless of pole count / degree. Degree = min(3, n_poles-1)
// so two poles still yield a valid degree-1 curve.
//
// These are deterministic closed-form routines (no OCCT, no solver) —
// the profile engine and the viewport emitter evaluate the exact same
// curve the wire builder constructs with Geom_BSplineCurve.

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace polysmith::core {

struct SplineSample {
  double x;
  double y;
};

// Clamped open-uniform knot vector on [0, 1]: degree+1 copies of 0,
// then equally spaced interior knots, then degree+1 copies of 1.
// Returns knots.size() == n_poles + degree + 1.
inline std::vector<double> spline_open_uniform_knots(int n_poles,
                                                     int degree) {
  const int interior = n_poles - degree - 1;  // >= 0 for valid inputs
  const int total = n_poles + degree + 1;
  std::vector<double> knots(total, 0.0);
  for (int i = degree + 1; i < n_poles; ++i) {
    knots[i] = static_cast<double>(i - degree) /
               static_cast<double>(interior + 1);
  }
  for (int i = n_poles; i < total; ++i) knots[i] = 1.0;
  return knots;
}

// De Boor evaluation of a clamped B-spline at u in [0, 1].
inline SplineSample spline_eval(int degree,
                                const std::vector<double>& knots,
                                const std::vector<double>& pole_xs,
                                const std::vector<double>& pole_ys,
                                double u) {
  const int n = static_cast<int>(pole_xs.size());
  if (n < 2) {
    return SplineSample{.x = n > 0 ? pole_xs[0] : 0.0,
                        .y = n > 0 ? pole_ys[0] : 0.0};
  }
  if (u <= 0.0) return SplineSample{.x = pole_xs.front(), .y = pole_ys.front()};
  if (u >= 1.0) return SplineSample{.x = pole_xs.back(), .y = pole_ys.back()};

  // Knot span: the largest k with knots[k] <= u < knots[k+1].
  int span = n - 1;
  for (int i = 0; i + 1 < static_cast<int>(knots.size()); ++i) {
    if (u >= knots[i] && u < knots[i + 1]) {
      span = i;
      break;
    }
  }

  std::vector<double> d(n);
  std::vector<double> e(n);
  for (int i = 0; i < n; ++i) {
    d[i] = pole_xs[i];
    e[i] = pole_ys[i];
  }
  // De Boor (Piegl & Tiller A2.1): each round must walk DOWN from the span, so
  // d[i-1] is still the previous round's value. Iterating upward blends a
  // value that this same round already overwrote and evaluates a different
  // curve (which is how a degree-3 spline through (0,0) (3,4) (7,5) (10,0)
  // came out at (1.89, 2.37) instead of (3,4) at u = 1/3).
  for (int r = 1; r <= degree; ++r) {
    for (int i = span; i >= span - degree + r; --i) {
      const double denom = knots[i + degree - r + 1] - knots[i];
      const double alpha = denom > 1e-15 ? (u - knots[i]) / denom : 0.0;
      d[i] = (1.0 - alpha) * d[i - 1] + alpha * d[i];
      e[i] = (1.0 - alpha) * e[i - 1] + alpha * e[i];
    }
  }
  return SplineSample{.x = d[span], .y = e[span]};
}

// First derivative dP/du: the B-spline derivative is the degree-1
// spline over the differenced poles with the first/last knots dropped.
inline SplineSample spline_eval_derivative(    int degree, const std::vector<double>& knots,
    const std::vector<double>& pole_xs, const std::vector<double>& pole_ys,
    double u) {
  const int n = static_cast<int>(pole_xs.size());
  if (degree <= 0 || n < 2) return SplineSample{.x = 0.0, .y = 0.0};
  std::vector<double> qx;
  std::vector<double> qy;
  qx.reserve(n - 1);
  qy.reserve(n - 1);
  for (int i = 0; i + 1 < n; ++i) {
    const double denom = knots[i + degree + 1] - knots[i + 1];
    const double w = denom > 1e-15 ? degree / denom : 0.0;
    qx.push_back(w * (pole_xs[i + 1] - pole_xs[i]));
    qy.push_back(w * (pole_ys[i + 1] - pole_ys[i]));
  }
  std::vector<double> k2(knots.begin() + 1, knots.end() - 1);
  return spline_eval(degree - 1, k2, qx, qy, u);
}

// Knot span containing u: the largest k with knots[k] <= u < knots[k+1]
// (Piegl & Tiller A2.1). For a clamped curve u == 1 maps to the last pole.
inline int spline_find_span(int pole_count,
                            int degree,
                            const std::vector<double>& knots,
                            double u) {
  if (u >= knots[static_cast<size_t>(pole_count)]) {
    return pole_count - 1;
  }
  int low = degree;
  int high = pole_count;
  int mid = (low + high) / 2;
  while (u < knots[static_cast<size_t>(mid)] ||
         u >= knots[static_cast<size_t>(mid + 1)]) {
    if (u < knots[static_cast<size_t>(mid)]) {
      high = mid;
    } else {
      low = mid;
    }
    mid = (low + high) / 2;
  }
  return mid;
}

// The degree+1 nonzero basis functions at u (Piegl & Tiller A2.3). Writes
// `values[0..degree]`, which belong to poles span-degree .. span, and returns
// the span. Computing the whole row in one pass is what keeps the indices of
// knots[] non-negative at the clamped ends — evaluating one basis function in
// isolation walks before the start of the knot vector when u sits on the
// curve's first knot.
inline int spline_basis_row(int pole_count,
                            int degree,
                            const std::vector<double>& knots,
                            double u,
                            std::vector<double>& values) {
  values.assign(static_cast<size_t>(degree) + 1, 0.0);
  const int span = spline_find_span(pole_count, degree, knots, u);
  std::vector<double> left(static_cast<size_t>(degree) + 1, 0.0);
  std::vector<double> right(static_cast<size_t>(degree) + 1, 0.0);
  values[0] = 1.0;
  for (int j = 1; j <= degree; ++j) {
    left[static_cast<size_t>(j)] = u - knots[static_cast<size_t>(span + 1 - j)];
    right[static_cast<size_t>(j)] = knots[static_cast<size_t>(span + j)] - u;
    double saved = 0.0;
    for (int r = 0; r < j; ++r) {
      const double denom = right[static_cast<size_t>(r + 1)] +
                           left[static_cast<size_t>(j - r)];
      const double temp =
          denom > 1e-15 ? values[static_cast<size_t>(r)] / denom : 0.0;
      values[static_cast<size_t>(r)] =
          saved + right[static_cast<size_t>(r + 1)] * temp;
      saved = left[static_cast<size_t>(j - r)] * temp;
    }
    values[static_cast<size_t>(j)] = saved;
  }
  return span;
}

// Largest point count the dense interpolation solve accepts (O(n^3)).
inline constexpr int kSplineFitMaxPoints = 256;

// Highest degree a sketch spline may request (T2.1). Degree is also bounded by
// pole_count - 1 at every use site.
inline constexpr int kSplineMaxDegree = 5;

// Poles of the clamped B-spline of the given degree that passes through
// `fit_points`, in order. This is "fit point spline" behaviour: the curve
// interpolates every clicked point.
//
// The knot vector is the same open-uniform one the rest of the pipeline
// assumes (`spline_open_uniform_knots(n, degree)`), and the parameters are the
// Greville abscissae of that knot vector — so the poles returned here are
// evaluated downstream by exactly the knots used to solve for them. (Solving
// at chord-length parameters would produce poles for a different knot vector
// and the curve would miss the points.)
//
// Throws when given fewer than 2 points or more than `kSplineFitMaxPoints`.
inline std::vector<SplineSample> spline_fit_poles(
    const std::vector<std::pair<double, double>>& fit_points,
    int degree) {
  const int n = static_cast<int>(fit_points.size());
  if (n < 2) {
    throw std::runtime_error("A fit-point spline needs at least 2 points");
  }
  if (n > kSplineFitMaxPoints) {
    throw std::runtime_error("A fit-point spline accepts at most " +
                             std::to_string(kSplineFitMaxPoints) + " points");
  }
  const int p = std::max(1, std::min(degree, n - 1));
  const std::vector<double> knots = spline_open_uniform_knots(n, p);

  // Greville abscissae: u_i = (knots[i+1] + ... + knots[i+p]) / p.
  std::vector<double> params(static_cast<size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    double sum = 0.0;
    for (int j = 1; j <= p; ++j) {
      sum += knots[static_cast<size_t>(i + j)];
    }
    params[static_cast<size_t>(i)] = sum / static_cast<double>(p);
  }

  // Collocation matrix: row i is the (degree+1)-wide basis row at parameter
  // u_i, written at its span offset. Getting the span from the same helper the
  // fit uses keeps the matrix exactly triangular-banded and non-singular.
  std::vector<std::vector<double>> matrix(
      static_cast<size_t>(n), std::vector<double>(static_cast<size_t>(n), 0.0));
  std::vector<double> row;
  for (int i = 0; i < n; ++i) {
    // The basis row is defined on the half-open span [u_k, u_{k+1}); the last
    // Greville parameter is exactly 1.0, which belongs to no span, so evaluate
    // it one floating-point step inside the final one.
    double u = params[static_cast<size_t>(i)];
    if (u >= 1.0) {
      u = std::nextafter(1.0, 0.0);
    }
    const int span = spline_basis_row(n, p, knots, u, row);
    for (int j = 0; j <= p; ++j) {
      matrix[static_cast<size_t>(i)][static_cast<size_t>(span - p + j)] =
          row[static_cast<size_t>(j)];
    }
  }

  // Solve in place for x and y with partial pivoting. n <= 256 keeps this
  // trivially fast; the matrix is banded but dense is simpler and honest.
  std::vector<double> rhs_x(static_cast<size_t>(n));
  std::vector<double> rhs_y(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    rhs_x[static_cast<size_t>(i)] = fit_points[static_cast<size_t>(i)].first;
    rhs_y[static_cast<size_t>(i)] = fit_points[static_cast<size_t>(i)].second;
  }
  for (int col = 0; col < n; ++col) {
    int pivot = col;
    double best = std::abs(matrix[static_cast<size_t>(col)][static_cast<size_t>(col)]);
    for (int row = col + 1; row < n; ++row) {
      const double value =
          std::abs(matrix[static_cast<size_t>(row)][static_cast<size_t>(col)]);
      if (value > best) {
        best = value;
        pivot = row;
      }
    }
    if (best < 1e-12) {
      throw std::runtime_error("Fit-point spline: singular interpolation system");
    }
    if (pivot != col) {
      std::swap(matrix[static_cast<size_t>(pivot)],
                matrix[static_cast<size_t>(col)]);
      std::swap(rhs_x[static_cast<size_t>(pivot)], rhs_x[static_cast<size_t>(col)]);
      std::swap(rhs_y[static_cast<size_t>(pivot)], rhs_y[static_cast<size_t>(col)]);
    }
    for (int row = col + 1; row < n; ++row) {
      const double factor =
          matrix[static_cast<size_t>(row)][static_cast<size_t>(col)] /
          matrix[static_cast<size_t>(col)][static_cast<size_t>(col)];
      if (factor == 0.0) continue;
      for (int k = col; k < n; ++k) {
        matrix[static_cast<size_t>(row)][static_cast<size_t>(k)] -=
            factor * matrix[static_cast<size_t>(col)][static_cast<size_t>(k)];
      }
      rhs_x[static_cast<size_t>(row)] -= factor * rhs_x[static_cast<size_t>(col)];
      rhs_y[static_cast<size_t>(row)] -= factor * rhs_y[static_cast<size_t>(col)];
    }
  }

  std::vector<SplineSample> poles(static_cast<size_t>(n), SplineSample{0.0, 0.0});
  for (int row = n - 1; row >= 0; --row) {
    double x = rhs_x[static_cast<size_t>(row)];
    double y = rhs_y[static_cast<size_t>(row)];
    for (int k = row + 1; k < n; ++k) {
      x -= matrix[static_cast<size_t>(row)][static_cast<size_t>(k)] *
           poles[static_cast<size_t>(k)].x;
      y -= matrix[static_cast<size_t>(row)][static_cast<size_t>(k)] *
           poles[static_cast<size_t>(k)].y;
    }
    const double diag = matrix[static_cast<size_t>(row)][static_cast<size_t>(row)];
    poles[static_cast<size_t>(row)] = SplineSample{x / diag, y / diag};
  }
  return poles;
}

}  // namespace polysmith::core