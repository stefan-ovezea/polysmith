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
  for (int r = 1; r <= degree; ++r) {
    for (int i = span - degree + r; i <= span; ++i) {
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
inline SplineSample spline_eval_derivative(
    int degree, const std::vector<double>& knots,
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

}  // namespace polysmith::core
