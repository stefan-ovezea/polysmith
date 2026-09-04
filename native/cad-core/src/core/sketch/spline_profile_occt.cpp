#include "core/sketch/spline_profile_occt.h"

#include <cmath>

#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <NCollection_Array1.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Ax22d.hxx>
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
  NCollection_Array1<gp_Pnt2d> poles(1, n);
  for (int i = 0; i < n; ++i) {
    poles.SetValue(i + 1, gp_Pnt2d(pole_xs[i], pole_ys[i]));
  }
  const int interior = n - degree - 1;
  const int knot_count = interior + 2;
  NCollection_Array1<double> knots(1, knot_count);
  NCollection_Array1<int> mults(1, knot_count);
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
// full circles, ellipses via Geom2d_Ellipse (full sweep), splines via
// the shared convention.
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
  if (other.kind == 4) {  // ellipse — full sweep, ccw
    const double cu = std::cos(v[4]);
    const double su = std::sin(v[4]);
    gp_Ax22d axis(gp_Pnt2d(v[0], v[1]),
                  gp_Dir2d(cu, su),           // major axis direction
                  gp_Dir2d(-su, cu));         // minor axis direction
    return new Geom2d_Ellipse(axis, v[2], v[3]);
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

bool spline_segment_poles(const std::vector<double>& pole_xs,
                          const std::vector<double>& pole_ys, int degree,
                          double u0, double u1,
                          std::vector<double>& out_px,
                          std::vector<double>& out_py, int& out_degree) {
  if (pole_xs.size() < 2 || pole_xs.size() != pole_ys.size()) return false;
  if (u0 <= 1e-12 && u1 >= 1.0 - 1e-12) {
    out_px = pole_xs;
    out_py = pole_ys;
    out_degree = degree;
    return true;
  }
  // The vendored Geom2d_BSplineCurve::Segment MUTATES the curve in
  // place (void return — the reduced OCCT 8 API), so segment a copy.
  Handle(Geom2d_BSplineCurve) seg = make_spline_geom(pole_xs, pole_ys, degree);
  if (seg.IsNull()) return false;
  seg->Segment(u0, u1);
  const int n = seg->NbPoles();
  out_px.resize(n);
  out_py.resize(n);
  for (int i = 1; i <= n; ++i) {
    const gp_Pnt2d p = seg->Pole(i);
    out_px[i - 1] = p.X();
    out_py[i - 1] = p.Y();
  }
  int d = seg->Degree();
  if (d > n - 1) d = n - 1;
  if (d < 1) d = 1;
  out_degree = d;
  return true;
}

void sketch_curve_pair_intersections_occt(
    const SplineProfileCurve& a, const SplineProfileCurve& b,
    std::vector<std::pair<double, double>>& out_points) {
  Handle(Geom2d_Curve) ga = make_other_geom(a);
  Handle(Geom2d_Curve) gb = make_other_geom(b);
  Geom2dAPI_InterCurveCurve inter(ga, gb, 0.01);
  auto push_point = [&](const gp_Pnt2d& p) {
    out_points.push_back({p.X(), p.Y()});
  };
  for (int i = 1; i <= inter.NbPoints(); ++i) {
    push_point(inter.Point(i));
  }
  // Tangent overlaps arrive as segments, not points — take their
  // endpoints so a tangent pair still splits.
  for (int i = 1; i <= inter.NbSegments(); ++i) {
    Handle(Geom2d_Curve) c1;
    Handle(Geom2d_Curve) c2;
    inter.Segment(i, c1, c2);
    for (const auto* c : {c1.get(), c2.get()}) {
      gp_Pnt2d p;
      c->D0(c->FirstParameter(), p);
      push_point(p);
      c->D0(c->LastParameter(), p);
      push_point(p);
    }
  }
}

}  // namespace polysmith::core
