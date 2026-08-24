#include "core/sketch/spline_profile_occt.h"

#include <cmath>

#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt2d.hxx>

namespace polysmith::core {

namespace {

constexpr double kSplineProfilePi = 3.14159265358979323846;

// Clamped open-uniform Geom2d_BSplineCurve matching spline_math.h's
// convention — the walk, the viewport and the wire builder evaluate
// the same curve. OCCT takes DISTINCT knot values with per-value
// multiplicities (the expanded knot sequence used by the de Boor
// evaluator would be misread as a near-zero interval).
Handle(Geom2d_BSplineCurve) make_spline_geom(
    const std::vector<double>& pole_xs, const std::vector<double>& pole_ys,
    int degree) {
  const int n = static_cast<int>(pole_xs.size());
  if (degree < 1 || degree > n - 1) degree = n - 1;
  if (degree < 1) degree = 1;
  TColgp_Array1OfPnt2d poles(1, n);
  for (int i = 0; i < n; ++i) {
    poles.SetValue(i + 1, gp_Pnt2d(pole_xs[i], pole_ys[i]));
  }
  const int interior = n - degree - 1;
  const int knot_count = interior + 2;
  TColStd_Array1OfReal knots(1, knot_count);
  TColStd_Array1OfInteger mults(1, knot_count);
  knots.SetValue(1, 0.0);
  mults.SetValue(1, degree + 1);
  for (int i = 1; i <= interior; ++i) {
    knots.SetValue(i + 1, static_cast<double>(i) /
                              static_cast<double>(interior + 1));
    mults.SetValue(i + 1, 1);
  }
  knots.SetValue(knot_count, 1.0);
  mults.SetValue(knot_count, degree + 1);
  return new Geom2d_BSplineCurve(poles, knots, mults, degree);
}

// The `other` curve as a bounded OCCT 2D curve: lines as trimmed
// lines, arcs as trimmed circles (bounded by their sweep), circles as
// full circles, splines via the shared convention.
Handle(Geom2d_Curve) make_other_geom(const SplineProfileCurve& other) {
  const auto& v = other.values;
  if (other.kind == 0) {  // line
    const double len = std::hypot(v[2] - v[0], v[3] - v[1]);
    return new Geom2d_TrimmedCurve(
        new Geom2d_Line(gp_Pnt2d(v[0], v[1]), gp_Dir2d(v[2] - v[0], v[3] - v[1])),
        0.0, len);
  }
  if (other.kind == 1) {  // circle
    return new Geom2d_Circle(gp_Ax2d(gp_Pnt2d(v[0], v[1]), gp_Dir2d(1.0, 0.0)),
                             v[2]);
  }
  if (other.kind == 2) {  // arc
    double first = v[3];
    double last = v[4];
    const bool ccw = v[5] > 0.5;
    if (ccw && last < first) last += 2.0 * kSplineProfilePi;
    if (!ccw && last > first) last -= 2.0 * kSplineProfilePi;
    return new Geom2d_TrimmedCurve(
        new Geom2d_Circle(gp_Ax2d(gp_Pnt2d(v[0], v[1]), gp_Dir2d(1.0, 0.0)),
                          v[2]),
        first, last);
  }
  return make_spline_geom(other.pole_xs, other.pole_ys, other.degree);
}

}  // namespace

void spline_profile_intersections(
    const std::vector<double>& pole_xs, const std::vector<double>& pole_ys,
    int degree, const SplineProfileCurve& other,
    std::vector<SplineProfileIntersection>& out) {
  if (pole_xs.size() < 2) return;
  Geom2dAPI_InterCurveCurve inter(make_spline_geom(pole_xs, pole_ys, degree),
                                  make_other_geom(other), 0.01);
  // The vendored Geom2dAPI_InterCurveCurve exposes points but no
  // parameter accessor — re-derive the spline param by projecting the
  // intersection point (the same point-based convention the caller
  // uses for the other side).
  const auto push_point = [&](const gp_Pnt2d& p) {
    const auto sparam = spline_profile_param_at_point(
        pole_xs, pole_ys, degree, p.X(), p.Y(), 0.01);
    if (sparam.has_value()) {
      out.push_back(SplineProfileIntersection{
          .spline_param = sparam.value(), .x = p.X(), .y = p.Y()});
    }
  };
  for (int i = 1; i <= inter.NbPoints(); ++i) {
    push_point(inter.Point(i));
  }
  // Tangent overlaps arrive as segments, not points — take their
  // endpoints so a spline tangent to a line/circle still splits.
  for (int i = 1; i <= inter.NbSegments(); ++i) {
    Handle(Geom2d_Curve) c1;
    Handle(Geom2d_Curve) c2;
    inter.Segment(i, c1, c2);
    gp_Pnt2d p;
    c1->D0(c1->FirstParameter(), p);
    push_point(p);
    c1->D0(c1->LastParameter(), p);
    push_point(p);
  }
}

std::optional<double> spline_profile_param_at_point(
    const std::vector<double>& pole_xs, const std::vector<double>& pole_ys,
    int degree, double px, double py, double tolerance) {
  if (pole_xs.size() < 2) return std::nullopt;
  Geom2dAPI_ProjectPointOnCurve proj(
      gp_Pnt2d(px, py), make_spline_geom(pole_xs, pole_ys, degree));
  if (proj.NbPoints() < 1) return std::nullopt;
  if (proj.NearestPoint().Distance(gp_Pnt2d(px, py)) > tolerance) {
    return std::nullopt;
  }
  return proj.LowerDistanceParameter();
}

}  // namespace polysmith::core
