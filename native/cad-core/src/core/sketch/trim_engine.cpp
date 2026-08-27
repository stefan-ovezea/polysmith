#include "core/sketch/trim_engine.h"

#include "core/sketch/sketch_curve.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include "core/diagnostics/logger.h"

namespace polysmith::core {
namespace {

constexpr double kSegmentSelectTolerance = 5.0;  // mm

// Wraps an angle into [0, 2π).
double wrap_angle(double a) {
  constexpr double k2Pi = 2.0 * M_PI;
  while (a < 0) a += k2Pi;
  while (a >= k2Pi) a -= k2Pi;
  return a;
}

// Returns the squared distance from point P to the line segment AB.
double point_segment_distance_sq(double px, double py,
                                 double ax, double ay,
                                 double bx, double by) {
  const double abx = bx - ax;
  const double aby = by - ay;
  const double ab_len_sq = abx * abx + aby * aby;
  if (ab_len_sq < kTrimCoincidentTolerance * kTrimCoincidentTolerance) {
    const double dx = px - ax;
    const double dy = py - ay;
    return dx * dx + dy * dy;
  }
  double t = ((px - ax) * abx + (py - ay) * aby) / ab_len_sq;
  t = std::clamp(t, 0.0, 1.0);
  const double proj_x = ax + t * abx;
  const double proj_y = ay + t * aby;
  const double dx = px - proj_x;
  const double dy = py - proj_y;
  return dx * dx + dy * dy;
}

// Coincident-point dedup (shared by all find_all_intersections).
void deduplicate(std::vector<TrimIntersection>& results) {
  if (results.size() <= 1) return;
  std::vector<TrimIntersection> deduped;
  deduped.reserve(results.size());
  deduped.push_back(std::move(results[0]));
  for (size_t i = 1; i < results.size(); ++i) {
    const double dx = results[i].x - deduped.back().x;
    const double dy = results[i].y - deduped.back().y;
    if (std::sqrt(dx * dx + dy * dy) > kTrimCoincidentTolerance) {
      deduped.push_back(std::move(results[i]));
    }
  }
  results = std::move(deduped);
}

}  // namespace

// Distance from a point to a trim segment, measured against the ARC
// itself rather than its chord. The old chord-based fallback
// misselected LARGE segments: for a click on a long arc the distance
// to its own chord is the sagitta, which exceeds the selection
// tolerance, so a neighbouring segment's chord could win and the trim
// deleted the wrong piece. Exposed (non-anonymous) so the trim test
// suite can pin the metric.
double point_trim_segment_distance_sq(const TrimSegment& segment,
                                      double px, double py) {
  if (segment.kind != TrimSegment::ARC_SEGMENT || segment.radius <= 1e-9) {
    return point_segment_distance_sq(px, py, segment.start_x, segment.start_y,
                                     segment.end_x, segment.end_y);
  }
  const double dx = px - segment.center_x;
  const double dy = py - segment.center_y;
  const double angle = wrap_angle(std::atan2(dy, dx));
  const double s = wrap_angle(segment.param_start);
  double e = wrap_angle(segment.param_end);
  double a = angle;
  bool in_span = false;
  if (segment.ccw) {
    if (e < s) e += 2.0 * M_PI;
    if (a < s) a += 2.0 * M_PI;
    in_span = a >= s && a <= e;
  } else {
    if (e > s) e -= 2.0 * M_PI;
    if (a > s) a -= 2.0 * M_PI;
    in_span = a <= s && a >= e;
  }
  if (in_span) {
    const double gap = std::abs(std::hypot(dx, dy) - segment.radius);
    return gap * gap;
  }
  const double d_start = point_segment_distance_sq(
      px, py, segment.start_x, segment.start_y, segment.start_x, segment.start_y);
  const double d_end = point_segment_distance_sq(
      px, py, segment.end_x, segment.end_y, segment.end_x, segment.end_y);
  return std::min(d_start, d_end);
}

// Shared-curve views for the target entities so the trim adapters can
// route new geometry kinds (ellipse, spline) through the same
// intersection dispatch the profile walk uses.
ExactCurve trim_target_curve(const SketchLine& line) {
  return ExactCurve{.kind = ExactCurve::Kind::kLine,
                    .id = line.id,
                    .x0 = line.start_x, .y0 = line.start_y,
                    .x1 = line.end_x, .y1 = line.end_y};
}

ExactCurve trim_target_curve(const SketchCircle& circle) {
  return ExactCurve{.kind = ExactCurve::Kind::kCircle,
                    .id = circle.id,
                    .cx = circle.center_x, .cy = circle.center_y,
                    .r = circle.radius,
                    .ccw = true};
}

ExactCurve trim_target_curve(const SketchArc& arc) {
  return ExactCurve{
      .kind = ExactCurve::Kind::kArc,
      .id = arc.id,
      .cx = arc.center_x, .cy = arc.center_y, .r = arc.radius,
      .ccw = arc.ccw,
      .sweep_start = exact_wrap_angle(
          std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x)),
      .sweep_end = exact_wrap_angle(
          std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x)),
  };
}

ExactCurve trim_target_curve(const SketchEllipse& ellipse) {
  return ExactCurve{
      .kind = ExactCurve::Kind::kEllipse,
      .id = ellipse.id,
      .cx = ellipse.center_x, .cy = ellipse.center_y,
      .r = ellipse.a,
      .b = ellipse.b,
      .rotation = ellipse.rotation,
      .ccw = ellipse.has_sweep ? ellipse.ccw : true,
      .sweep_start = ellipse.has_sweep ? ellipse.sweep_start_angle : 0.0,
      .sweep_end = ellipse.has_sweep ? ellipse.sweep_end_angle : 0.0,
      .has_sweep = ellipse.has_sweep,
      .start_vertex_id = ellipse.start_vertex_id,
      .end_vertex_id = ellipse.end_vertex_id,
  };
}

ExactCurve trim_target_curve(const SketchSpline& spline) {
  ExactCurve c{.kind = ExactCurve::Kind::kSpline,
               .id = spline.id,
               .spline_degree = spline.degree};
  c.pole_xs = spline.pole_xs;
  c.pole_ys = spline.pole_ys;
  return c;
}

#include "core/sketch/impl/trim_line_circle_intersections.inc"
#include "core/sketch/impl/trim_line_circle_segments.inc"
#include "core/sketch/impl/trim_arc_operations.inc"
#include "core/sketch/impl/trim_ellipse_operations.inc"
#include "core/sketch/impl/trim_spline_operations.inc"

}  // namespace polysmith::core
