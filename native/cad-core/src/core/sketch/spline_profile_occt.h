#pragma once

// Spline intersection / projection helpers for the profile engine.
//
// The profile engine's exact-curve walk (sketch_profile_exact.inc) is
// otherwise pure closed-form C++; spline intersections delegate to
// OCCT's Geom2dAPI in a separate TU so the header chain stays
// OCCT-free (the lightweight sketch_profile_test target compiles the
// walk without pulling OCCT headers, and links the same real
// implementation).
//
// Conventions match spline_math.h: clamped open-uniform B-splines,
// parameter space [0, 1], poles are the control points.

#include <optional>
#include <vector>

namespace polysmith::core {

// The other side of an intersection, flattened to plain data.
struct SplineProfileCurve {
  int kind = 0;  // 0 line, 1 circle, 2 arc, 3 spline, 4 ellipse
  // line: x0, y0, x1, y1
  // circle: cx, cy, r
  // arc: cx, cy, r, start_angle, end_angle, ccw (0/1)
  // ellipse: cx, cy, major_r, minor_r, rotation (ccw, full sweep)
  std::vector<double> values;
  // spline only
  std::vector<double> pole_xs;
  std::vector<double> pole_ys;
  int degree = 3;
};

struct SplineProfileIntersection {
  double spline_param = 0.0;  // param on the spline (in [0, 1])
  double x = 0.0;
  double y = 0.0;
};

// All intersections between a spline and another planar curve.  The
// other side's parameter is re-derived from the returned coordinates
// by the caller (exact_curve_param_at_point) so the two sides can
// never disagree.
void spline_profile_intersections(
    const std::vector<double>& pole_xs, const std::vector<double>& pole_ys,
    int degree, const SplineProfileCurve& other,
    std::vector<SplineProfileIntersection>& out);

// Closest parameter of (px, py) on the spline; nullopt when the
// closest point is farther than `tolerance`.
std::optional<double> spline_profile_param_at_point(
    const std::vector<double>& pole_xs, const std::vector<double>& pole_ys,
    int degree, double px, double py, double tolerance);

// Intersection points between two arbitrary planar curves (the
// SplineProfileCurve descriptions above, including ellipses).  Fills
// `out` with point coordinates; the caller re-derives each side's
// parameter from the point so the two sides can never disagree.
void sketch_curve_pair_intersections_occt(
    const SplineProfileCurve& a, const SplineProfileCurve& b,
    std::vector<std::pair<double, double>>& out_points);

// Control poles of the spline's [u0, u1] parameter sub-span, in the
// project's clamped open-uniform convention. Uses OCCT's exact
// knot-insertion segment (Boehm), NOT a re-fit — the sub-curve is the
// same geometry, so the joint between two trimmed pieces lands exactly
// on the intersection. Returns false when the inputs are unusable.
bool spline_segment_poles(const std::vector<double>& pole_xs,
                          const std::vector<double>& pole_ys, int degree,
                          double u0, double u1,
                          std::vector<double>& out_px,
                          std::vector<double>& out_py, int& out_degree);

}  // namespace polysmith::core
