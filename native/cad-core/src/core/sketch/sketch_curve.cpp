#include "core/sketch/sketch_curve.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/sketch/sketch_types.h"
#include "core/sketch/spline_math.h"
#include "core/sketch/trim_engine.h"

namespace polysmith::core {

double exact_wrap_angle(double a) {
  while (a < 0.0) a += 2.0 * kExactPi;
  while (a >= 2.0 * kExactPi) a -= 2.0 * kExactPi;
  return a;
}

SketchProfilePoint exact_curve_point(const ExactCurve& c, double param) {
  if (c.kind == ExactCurve::Kind::kLine) {
    return SketchProfilePoint{.x = c.x0 + param * (c.x1 - c.x0),
                             .y = c.y0 + param * (c.y1 - c.y0)};
  }
  if (c.kind == ExactCurve::Kind::kSpline) {
    const auto knots = spline_open_uniform_knots(
        static_cast<int>(c.pole_xs.size()), c.spline_degree);
    const SplineSample s = spline_eval(c.spline_degree, knots, c.pole_xs,
                                       c.pole_ys, param);
    return SketchProfilePoint{.x = s.x, .y = s.y};
  }
  if (c.kind == ExactCurve::Kind::kEllipse) {
    const double cu = std::cos(c.rotation);
    const double su = std::sin(c.rotation);
    const double ct = std::cos(param);
    const double st = std::sin(param);
    return SketchProfilePoint{
        .x = c.cx + c.r * ct * cu - c.b * st * su,
        .y = c.cy + c.r * ct * su + c.b * st * cu,
    };
  }
  return SketchProfilePoint{.x = c.cx + c.r * std::cos(param),
                           .y = c.cy + c.r * std::sin(param)};
}

void exact_curve_tangent(const ExactCurve& c, double param, double& tx,
                         double& ty) {
  if (c.kind == ExactCurve::Kind::kLine) {
    const double len = std::sqrt((c.x1 - c.x0) * (c.x1 - c.x0) +
                                 (c.y1 - c.y0) * (c.y1 - c.y0));
    tx = (c.x1 - c.x0) / len;
    ty = (c.y1 - c.y0) / len;
    return;
  }
  if (c.kind == ExactCurve::Kind::kSpline) {
    const auto knots = spline_open_uniform_knots(
        static_cast<int>(c.pole_xs.size()), c.spline_degree);
    const SplineSample s = spline_eval_derivative(c.spline_degree, knots,
                                                  c.pole_xs, c.pole_ys, param);
    tx = s.x;
    ty = s.y;
    const double len = std::sqrt(tx * tx + ty * ty);
    if (len > 1e-12) {
      tx /= len;
      ty /= len;
    }
    return;
  }
  if (c.kind == ExactCurve::Kind::kEllipse) {
    const double cu = std::cos(c.rotation);
    const double su = std::sin(c.rotation);
    const double ct = std::cos(param);
    const double st = std::sin(param);
    tx = -c.r * st * cu - c.b * ct * su;
    ty = -c.r * st * su + c.b * ct * cu;
    const double len = std::sqrt(tx * tx + ty * ty);
    if (len > 1e-12) {
      tx /= len;
      ty /= len;
    }
    return;
  }
  const double s = std::sin(param);
  const double cc = std::cos(param);
  const bool inc_is_ccw = (c.kind == ExactCurve::Kind::kCircle) || c.ccw;
  if (inc_is_ccw) {
    tx = -s; ty = cc;
  } else {
    tx = s; ty = -cc;
  }
}

double exact_lift_to_sweep(const ExactCurve& c, double wrapped) {
  const bool inc_is_ccw = (c.kind == ExactCurve::Kind::kCircle) || c.ccw;
  double a = wrapped;
  if (inc_is_ccw) {
    while (a < c.sweep_start) a += 2.0 * kExactPi;
  } else {
    while (a > c.sweep_start) a -= 2.0 * kExactPi;
  }
  return a;
}

bool exact_angle_in_sweep(double wrapped, const ExactCurve& arc) {
  const double s = arc.sweep_start;
  const double e = arc.sweep_end;
  // A stored arc whose endpoints coincide is degenerate — a full
  // circle is always stored as a circle entity, never an arc. Without
  // this guard the coincident case lifts into a 2π span below and the
  // walker turns the broken arc into a full-circle face (the
  // "trimmed to a half circle, detected as a full circle" bug).
  if (std::abs(std::sin(s - e)) < 1e-9 &&
      std::abs(std::cos(s - e) - 1.0) < 1e-9) {
    return false;
  }
  if (arc.ccw) {
    double a = wrapped;
    if (a < s) a += 2.0 * kExactPi;
    double ee = e;
    if (ee <= s) ee += 2.0 * kExactPi;
    return a >= s - kExactParamEps && a <= ee + kExactParamEps;
  }
  double a = wrapped;
  if (a > s) a -= 2.0 * kExactPi;
  double ee = e;
  if (ee >= s) ee -= 2.0 * kExactPi;
  return a <= s + kExactParamEps && a >= ee - kExactParamEps;
}

std::optional<double> exact_curve_param_at_point(const ExactCurve& c,
                                                 double x, double y) {
  if (c.kind == ExactCurve::Kind::kEllipse) {
    // The parameter is the parametric angle of the point in the
    // ellipse frame.
    const double cu = std::cos(c.rotation);
    const double su = std::sin(c.rotation);
    const double lx = (x - c.cx) * cu + (y - c.cy) * su;
    const double ly = -(x - c.cx) * su + (y - c.cy) * cu;
    const double nx = lx / c.r;
    const double ny = ly / c.b;
    if (std::abs(std::hypot(nx, ny) - 1.0) > kProfileTolerance / c.r) {
      return std::nullopt;
    }
    const double ang = exact_wrap_angle(std::atan2(ly / c.b, lx / c.r));
    // A partial ellipse's material ends at its sweep — points on the
    // complementary span of the support ellipse are NOT on this curve.
    if (c.has_sweep && !exact_angle_in_sweep(ang, c)) return std::nullopt;
    return ang;
  }
  if (c.kind == ExactCurve::Kind::kLine) {
    const double abx = c.x1 - c.x0, aby = c.y1 - c.y0;
    const double len_sq = abx * abx + aby * aby;
    const double t = len_sq > 1e-18
                         ? std::max(0.0, std::min(1.0, ((x - c.x0) * abx +
                                                        (y - c.y0) * aby) /
                                                           len_sq))
                         : 0.0;
    const double qx = c.x0 + t * abx;
    const double qy = c.y0 + t * aby;
    if (std::hypot(x - qx, y - qy) > kProfileTolerance) return std::nullopt;
    return t;
  }
  if (c.kind == ExactCurve::Kind::kCircle) {
    const double dx = x - c.cx, dy = y - c.cy;
    if (std::abs(std::hypot(dx, dy) - c.r) > kProfileTolerance) {
      return std::nullopt;
    }
    return exact_wrap_angle(std::atan2(dy, dx));
  }
  if (c.kind == ExactCurve::Kind::kArc) {
    const double dx = x - c.cx, dy = y - c.cy;
    if (std::abs(std::hypot(dx, dy) - c.r) > kProfileTolerance) {
      return std::nullopt;
    }
    const double ang = exact_wrap_angle(std::atan2(dy, dx));
    if (!exact_angle_in_sweep(ang, c)) return std::nullopt;
    return ang;
  }
  return spline_profile_param_at_point(c.pole_xs, c.pole_ys,
                                       c.spline_degree, x, y,
                                       kProfileTolerance);
}

// The other curve of an intersection as a flattened description.
static SplineProfileCurve exact_other_curve(const ExactCurve& c) {
  SplineProfileCurve other;
  if (c.kind == ExactCurve::Kind::kLine) {
    other.kind = 0;
    other.values = {c.x0, c.y0, c.x1, c.y1};
  } else if (c.kind == ExactCurve::Kind::kCircle) {
    other.kind = 1;
    other.values = {c.cx, c.cy, c.r};
  } else if (c.kind == ExactCurve::Kind::kArc) {
    other.kind = 2;
    other.values = {c.cx, c.cy, c.r, c.sweep_start, c.sweep_end,
                    c.ccw ? 1.0 : 0.0};
  } else if (c.kind == ExactCurve::Kind::kEllipse) {
    other.kind = 4;
    other.values = {c.cx, c.cy, c.r, c.b, c.rotation};
  } else {  // spline
    other.kind = 3;
    other.pole_xs = c.pole_xs;
    other.pole_ys = c.pole_ys;
    other.degree = c.spline_degree;
  }
  return other;
}

void exact_spline_intersections(const ExactCurve& spline,
                                const ExactCurve& other,
                                std::vector<std::pair<double, double>>& out) {
  std::vector<SplineProfileIntersection> recs;
  spline_profile_intersections(spline.pole_xs, spline.pole_ys,
                               spline.spline_degree, exact_other_curve(other),
                               recs);
  for (const auto& rec : recs) {
    const auto oparam =
        exact_curve_param_at_point(other, rec.x, rec.y);
    if (oparam.has_value()) {
      out.push_back({rec.spline_param, oparam.value()});
    }
  }
}

// Line × ellipse intersection: substitutes the line into the ellipse's
// implicit equation (in the ellipse's local frame), which is a
// quadratic in the line parameter.  Returns (t on line, θ on ellipse)
// pairs with θ in [0, 2π).
static void line_ellipse_intersections(
    const ExactCurve& line, const ExactCurve& ellipse,
    std::vector<std::pair<double, double>>& out) {
  const double cu = std::cos(ellipse.rotation);
  const double su = std::sin(ellipse.rotation);
  // Line endpoints in the ellipse frame.
  const double x0 = (line.x0 - ellipse.cx) * cu + (line.y0 - ellipse.cy) * su;
  const double y0 = -(line.x0 - ellipse.cx) * su + (line.y0 - ellipse.cy) * cu;
  const double x1 = (line.x1 - ellipse.cx) * cu + (line.y1 - ellipse.cy) * su;
  const double y1 = -(line.x1 - ellipse.cx) * su + (line.y1 - ellipse.cy) * cu;
  const double dx = x1 - x0;
  const double dy = y1 - y0;
  const double a = ellipse.r, b = ellipse.b;
  const double Aq = (dx * dx) / (a * a) + (dy * dy) / (b * b);
  const double Bq = 2.0 * (x0 * dx / (a * a) + y0 * dy / (b * b));
  const double Cq = (x0 * x0) / (a * a) + (y0 * y0) / (b * b) - 1.0;
  const double disc = Bq * Bq - 4.0 * Aq * Cq;
  if (disc < 0.0) return;
  const double sq = std::sqrt(disc);
  for (const double t : {( -Bq + sq) / (2.0 * Aq), (-Bq - sq) / (2.0 * Aq)}) {
    if (t < -1e-12 || t > 1.0 + 1e-12) continue;
    const double tc = std::max(0.0, std::min(1.0, t));
    const double lx = x0 + tc * dx;
    const double ly = y0 + tc * dy;
    const double theta = std::atan2(ly / b, lx / a);
    out.push_back({tc, exact_wrap_angle(theta)});
  }
}

void sketch_curve_intersections(const ExactCurve& A, const ExactCurve& B,
                                std::vector<std::pair<double, double>>& out) {
  using K = ExactCurve::Kind;
  const auto line_param = [](double t) {
    return std::max(0.0, std::min(1.0, t));
  };
  const auto angle_param = [](double a) { return exact_wrap_angle(a); };

  // Sketch entity views for the trim-engine intersection functions.
  const auto as_line = [](const ExactCurve& c) {
    return SketchLine{.id = c.id, .start_x = c.x0, .start_y = c.y0,
                      .end_x = c.x1, .end_y = c.y1};
  };
  const auto as_circle = [](const ExactCurve& c) {
    return SketchCircle{.id = c.id, .center_x = c.cx, .center_y = c.cy,
                        .radius = c.r};
  };
  const auto as_arc = [](const ExactCurve& c) {
    return SketchArc{.id = c.id,
                     .center_x = c.cx,
                     .center_y = c.cy,
                     .radius = c.r,
                     .start_x = c.cx + c.r * std::cos(c.sweep_start),
                     .start_y = c.cy + c.r * std::sin(c.sweep_start),
                     .end_x = c.cx + c.r * std::cos(c.sweep_end),
                     .end_y = c.cy + c.r * std::sin(c.sweep_end),
                     .ccw = c.ccw};
  };

  if (A.kind == K::kLine && B.kind == K::kLine) {
    if (const auto is = intersect_line_line(as_line(A), as_line(B));
        is.has_value()) {
      out.push_back({line_param(is->param_on_target),
                     line_param(is->param_on_other)});
    }
  } else if (A.kind == K::kLine && B.kind == K::kCircle) {
    for (const auto& is : intersect_line_circle(as_line(A), as_circle(B))) {
      out.push_back({line_param(is.param_on_target),
                     angle_param(is.param_on_other)});
    }
  } else if (A.kind == K::kCircle && B.kind == K::kLine) {
    for (const auto& is : intersect_line_circle(as_line(B), as_circle(A))) {
      out.push_back({angle_param(is.param_on_other),
                     line_param(is.param_on_target)});
    }
  } else if (A.kind == K::kCircle && B.kind == K::kCircle) {
    for (const auto& is :
         intersect_circle_circle(as_circle(A), as_circle(B))) {
      out.push_back({angle_param(is.param_on_target),
                     angle_param(is.param_on_other)});
    }
  } else if (A.kind == K::kLine && B.kind == K::kArc) {
    for (const auto& is : intersect_arc_line(as_arc(B), as_line(A))) {
      out.push_back({line_param(is.param_on_other),
                     angle_param(is.param_on_target)});
    }
  } else if (A.kind == K::kArc && B.kind == K::kLine) {
    for (const auto& is : intersect_arc_line(as_arc(A), as_line(B))) {
      out.push_back({angle_param(is.param_on_target),
                     line_param(is.param_on_other)});
    }
  } else if (A.kind == K::kCircle && B.kind == K::kArc) {
    for (const auto& is : intersect_arc_circle(as_arc(B), as_circle(A))) {
      out.push_back({angle_param(is.param_on_other),
                     angle_param(is.param_on_target)});
    }
  } else if (A.kind == K::kArc && B.kind == K::kCircle) {
    for (const auto& is : intersect_arc_circle(as_arc(A), as_circle(B))) {
      out.push_back({angle_param(is.param_on_target),
                     angle_param(is.param_on_other)});
    }
  } else if (A.kind == K::kSpline && B.kind != K::kEllipse) {
    exact_spline_intersections(A, B, out);
  } else if (B.kind == K::kSpline && A.kind != K::kEllipse) {
    std::vector<std::pair<double, double>> recs;
    exact_spline_intersections(B, A, recs);
    for (const auto& [pa, pb] : recs) out.push_back({pb, pa});
  } else if (A.kind == K::kLine && B.kind == K::kEllipse) {
    line_ellipse_intersections(A, B, out);
    // A partial ellipse's material ends at its sweep — drop hits on
    // the complementary span of the support ellipse.
    if (B.has_sweep) {
      out.erase(std::remove_if(out.begin(), out.end(),
                               [&](const std::pair<double, double>& p) {
                                 return !exact_angle_in_sweep(p.second, B);
                               }),
                out.end());
    }
  } else if (A.kind == K::kEllipse && B.kind == K::kLine) {
    std::vector<std::pair<double, double>> recs;
    line_ellipse_intersections(B, A, recs);
    for (const auto& [pt, pa] : recs) {
      if (A.has_sweep && !exact_angle_in_sweep(pa, A)) continue;
      out.push_back({pa, pt});
    }
  } else if (A.kind == K::kEllipse || B.kind == K::kEllipse) {
    // Every remaining ellipse pairing (ellipse × circle / arc /
    // ellipse / spline) goes through OCCT's 2D intersection; the
    // caller re-derives both sides' parameters from the returned
    // points so the two sides can never disagree.
    std::vector<std::pair<double, double>> points;
    sketch_curve_pair_intersections_occt(exact_other_curve(A),
                                         exact_other_curve(B), points);
    for (const auto& [px, py] : points) {
      const auto pa = exact_curve_param_at_point(A, px, py);
      const auto pb = exact_curve_param_at_point(B, px, py);
      if (pa.has_value() && pb.has_value()) {
        out.push_back({pa.value(), pb.value()});
      }
    }
  } else {  // arc x arc
    for (const auto& is : intersect_arc_arc(as_arc(A), as_arc(B))) {
      out.push_back({angle_param(is.param_on_target),
                     angle_param(is.param_on_other)});
    }
  }
}

}  // namespace polysmith::core
