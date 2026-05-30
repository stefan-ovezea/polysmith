#include "core/trim_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>
#include <utility>

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

// ================================================================
// intersect_line_line
// ================================================================

std::optional<TrimIntersection> intersect_line_line(
    const SketchLine& target,
    const SketchLine& other) {
  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.start_x,  cy = other.start_y;
  const double dx = other.end_x,    dy = other.end_y;

  const double abx = bx - ax, aby = by - ay;
  const double cdx = dx - cx, cdy = dy - cy;
  const double denom = abx * cdy - aby * cdx;

  if (std::abs(denom) < kTrimCoincidentTolerance) {
    return std::nullopt;
  }

  const double acx = cx - ax, acy = cy - ay;
  const double t = (acx * cdy - acy * cdx) / denom;
  const double u = (acx * aby - acy * abx) / denom;

  constexpr double kEps = 1e-12;
  if (t < -kEps || t > 1.0 + kEps || u < -kEps || u > 1.0 + kEps) {
    return std::nullopt;
  }

  return TrimIntersection{
      .x = ax + t * abx,
      .y = ay + t * aby,
      .param_on_target = std::clamp(t, 0.0, 1.0),
      .param_on_other = std::clamp(u, 0.0, 1.0),
      .other_entity_id = other.id,
  };
}

// ================================================================
// intersect_line_circle
// ================================================================

std::vector<TrimIntersection> intersect_line_circle(
    const SketchLine& target,
    const SketchCircle& other) {
  std::vector<TrimIntersection> result;

  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.center_x, cy = other.center_y;
  const double r = other.radius;

  // Vector from target start to circle center.
  const double dx = ax - cx;
  const double dy = ay - cy;

  // Line direction (AB).
  const double abx = bx - ax;
  const double aby = by - ay;

  // Quadratic: a*t² + b*t + c = 0  where t is [0,1] along AB.
  const double a = abx * abx + aby * aby;
  const double b = 2.0 * (dx * abx + dy * aby);
  const double c_val = dx * dx + dy * dy - r * r;

  if (a < kTrimCoincidentTolerance * kTrimCoincidentTolerance) {
    // Degenerate line — no intersection.
    return result;
  }

  const double disc = b * b - 4.0 * a * c_val;
  if (disc < -kTrimCoincidentTolerance) {
    return result;
  }

  const double sqrt_disc = disc <= 0.0 ? 0.0 : std::sqrt(disc);
  const double inv_2a = 1.0 / (2.0 * a);
  const double t1 = (-b - sqrt_disc) * inv_2a;
  const double t2 = (-b + sqrt_disc) * inv_2a;

  constexpr double kEps = 1e-12;

  for (double t : {t1, t2}) {
    if (t >= -kEps && t <= 1.0 + kEps) {
      const double tc = std::clamp(t, 0.0, 1.0);
      const double ix = ax + tc * abx;
      const double iy = ay + tc * aby;
      const double angle = wrap_angle(std::atan2(iy - cy, ix - cx));

      result.push_back(TrimIntersection{
          .x = ix,
          .y = iy,
          .param_on_target = tc,
          .param_on_other = angle,
          .other_entity_id = other.id,
      });
    }
  }

  return result;
}

// ================================================================
// intersect_circle_line
// ================================================================

std::vector<TrimIntersection> intersect_circle_line(
    const SketchCircle& target,
    const SketchLine& other) {
  // Swap: call intersect_line_circle and swap param fields.
  auto results = intersect_line_circle(other, target);
  for (auto& r : results) {
    std::swap(r.param_on_target, r.param_on_other);
    r.other_entity_id = other.id;
  }
  return results;
}

// ================================================================
// intersect_circle_circle
// ================================================================

std::vector<TrimIntersection> intersect_circle_circle(
    const SketchCircle& target,
    const SketchCircle& other) {
  std::vector<TrimIntersection> result;

  const double cx1 = target.center_x, cy1 = target.center_y;
  const double r1 = target.radius;
  const double cx2 = other.center_x, cy2 = other.center_y;
  const double r2 = other.radius;

  const double dx = cx2 - cx1;
  const double dy = cy2 - cy1;
  const double d = std::sqrt(dx * dx + dy * dy);

  // No intersection: circles too far apart or one inside the other.
  if (d > r1 + r2 + kTrimCoincidentTolerance ||
      d < std::abs(r1 - r2) - kTrimCoincidentTolerance) {
    return result;
  }

  // Distance from circle 1 center to the line connecting intersection points.
  const double a_val = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
  const double h_sq = r1 * r1 - a_val * a_val;
  const double h = h_sq <= 0.0 ? 0.0 : std::sqrt(h_sq);

  const double px = cx1 + a_val * dx / d;
  const double py = cy1 + a_val * dy / d;

  // Two intersection points (or one for tangent).
  const double hx = -dy * h / d;
  const double hy =  dx * h / d;

  const std::pair<double, double> pts[2] = {
      {px + hx, py + hy},
      {px - hx, py - hy},
  };

  for (const auto& [ix, iy] : pts) {
    const double angle = wrap_angle(std::atan2(iy - cy1, ix - cx1));
    const double other_angle = wrap_angle(std::atan2(iy - cy2, ix - cx2));
    result.push_back(TrimIntersection{
        .x = ix,
        .y = iy,
        .param_on_target = angle,
        .param_on_other = other_angle,
        .other_entity_id = other.id,
    });
  }

  // If h == 0 (tangent), we have two identical points. Dedup.
  if (result.size() == 2) {
    const double dxp = result[0].x - result[1].x;
    const double dyp = result[0].y - result[1].y;
    if (std::sqrt(dxp * dxp + dyp * dyp) < kTrimCoincidentTolerance) {
      result.pop_back();
    }
  }

  return result;
}

// ================================================================
// find_all_intersections (line target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchLine& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  int total_lines = 0, skipped_constr = 0, found = 0;

  for (const auto& line : params.lines) {
    if (line.id == target.id) continue;
    ++total_lines;
    if (line.is_construction) { ++skipped_constr; continue; }
    auto isect = intersect_line_line(target, line);
    if (isect.has_value()) { ++found; results.push_back(std::move(*isect)); }
  }

  for (const auto& circle : params.circles) {
    if (circle.is_construction) continue;
    auto isects = intersect_line_circle(target, circle);
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    if (arc.is_construction) continue;
    auto isects = intersect_arc_line(arc, target);
    for (auto& is : isects) {
      std::swap(is.param_on_target, is.param_on_other);
      is.other_entity_id = arc.id;
      results.push_back(std::move(is));
    }
  }

  if (results.empty()) return results;

  std::sort(results.begin(), results.end(),
            [](const TrimIntersection& a, const TrimIntersection& b) {
              return a.param_on_target < b.param_on_target;
            });

  deduplicate(results);

  // Filter out intersections at the line's own endpoints — they
  // produce zero-length segments and prevent meaningful trimming.
  // Same protection the arc path already applies at line ~674.
  results.erase(
      std::remove_if(results.begin(), results.end(),
                     [](const TrimIntersection& is) {
                       return is.param_on_target < kTrimCoincidentTolerance ||
                              is.param_on_target > 1.0 - kTrimCoincidentTolerance;
                     }),
      results.end());

  return results;
}

// ================================================================
// find_all_intersections (circle target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchCircle& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  int total_lines = 0, total_circles = 0, skipped_constr = 0, found = 0;

  for (const auto& line : params.lines) {
    ++total_lines;
    if (line.is_construction) { ++skipped_constr; continue; }
    auto isects = intersect_circle_line(target, line);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& circle : params.circles) {
    if (circle.id == target.id) continue;
    ++total_circles;
    if (circle.is_construction) { ++skipped_constr; continue; }
    auto isects = intersect_circle_circle(target, circle);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    if (arc.is_construction) continue;
    auto isects = intersect_arc_circle(arc, target);
    for (auto& is : isects) {
      std::swap(is.param_on_target, is.param_on_other);
      is.other_entity_id = arc.id;
      results.push_back(std::move(is));
      ++found;
    }
  }

  if (results.empty()) return results;

  std::sort(results.begin(), results.end(),
            [](const TrimIntersection& a, const TrimIntersection& b) {
              return a.param_on_target < b.param_on_target;
            });

  deduplicate(results);
  return results;
}

// ================================================================
// split_line_at_intersections
// ================================================================

std::vector<TrimSegment> split_line_at_intersections(
    const SketchLine& line,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  const double abx = line.end_x - line.start_x;
  const double aby = line.end_y - line.start_y;

  auto pt = [&](double t) -> std::pair<double, double> {
    return {line.start_x + t * abx, line.start_y + t * aby};
  };

  std::vector<TrimSegment> segments;

  auto [sx, sy] = pt(0.0);
  auto [ex, ey] = pt(intersections[0].param_on_target);
  segments.push_back({TrimSegment::LINE_SEGMENT, 0.0,
                      intersections[0].param_on_target, sx, sy, ex, ey});

  for (size_t i = 0; i + 1 < intersections.size(); ++i) {
    auto [s, sY] = pt(intersections[i].param_on_target);
    auto [e, eY] = pt(intersections[i + 1].param_on_target);
    segments.push_back({TrimSegment::LINE_SEGMENT,
                        intersections[i].param_on_target,
                        intersections[i + 1].param_on_target, s, sY, e, eY});
  }

  auto [lsx, lsy] = pt(intersections.back().param_on_target);
  auto [lex, ley] = pt(1.0);
  segments.push_back({TrimSegment::LINE_SEGMENT,
                      intersections.back().param_on_target, 1.0, lsx, lsy, lex, ley});

  return segments;
}

// ================================================================
// split_circle_at_intersections
// ================================================================

std::vector<TrimSegment> split_circle_at_intersections(
    const SketchCircle& circle,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  const double cx = circle.center_x, cy = circle.center_y, r = circle.radius;

  auto pt_at_angle = [&](double a) -> std::pair<double, double> {
    return {cx + r * std::cos(a), cy + r * std::sin(a)};
  };

  std::vector<TrimSegment> segments;
  const double k2Pi = 2.0 * M_PI;

  for (size_t i = 0; i < intersections.size(); ++i) {
    double a_start = intersections[i].param_on_target;
    double a_end   = intersections[(i + 1) % intersections.size()].param_on_target;

    // Going CCW from a_start to a_end.
    if (a_end <= a_start) a_end += k2Pi;

    auto [sx, sy] = pt_at_angle(a_start);
    auto [ex, ey] = pt_at_angle(a_end >= k2Pi ? wrap_angle(a_end) : a_end);

    segments.push_back(TrimSegment{
        TrimSegment::ARC_SEGMENT,
        a_start,
        a_end < k2Pi ? a_end : a_end - k2Pi,
        sx, sy, ex, ey,
        cx, cy, r,
        true /* ccw */
    });
  }

  return segments;
}

// ================================================================
// select_clicked_segment (line)
// ================================================================

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchLine& original_line,
    double click_x,
    double click_y) {
  if (segments.empty()) return -1;

  const double abx = original_line.end_x - original_line.start_x;
  const double aby = original_line.end_y - original_line.start_y;
  const double ab_len_sq = abx * abx + aby * aby;

  double click_t = -1.0;
  if (ab_len_sq > kTrimCoincidentTolerance * kTrimCoincidentTolerance) {
    click_t = ((click_x - original_line.start_x) * abx +
               (click_y - original_line.start_y) * aby) / ab_len_sq;
    click_t = std::clamp(click_t, 0.0, 1.0);
  }

  constexpr double kParamTol = 1e-10;
  for (size_t i = 0; i < segments.size(); ++i) {
    if (click_t >= segments[i].param_start - kParamTol &&
        click_t <= segments[i].param_end + kParamTol) {
      return static_cast<int>(i);
    }
  }

  // Distance fallback.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double dist_sq = point_segment_distance_sq(
        click_x, click_y,
        segments[i].start_x, segments[i].start_y,
        segments[i].end_x, segments[i].end_y);
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_index = static_cast<int>(i);
    }
  }
  if (best_dist_sq <= kSegmentSelectTolerance * kSegmentSelectTolerance) {
    return best_index;
  }
  return -1;
}

// ================================================================
// select_clicked_segment (circle)
// ================================================================

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchCircle& original_circle,
    double click_x,
    double click_y) {
  if (segments.empty()) return -1;

  const double cx = original_circle.center_x;
  const double cy = original_circle.center_y;

  // Angle of click point relative to circle center.
  double click_angle = std::atan2(click_y - cy, click_x - cx);
  click_angle = wrap_angle(click_angle);

  const double k2Pi = 2.0 * M_PI;
  constexpr double kAngTol = 1e-10;

  for (size_t i = 0; i < segments.size(); ++i) {
    double a_start = segments[i].param_start;
    double a_end   = segments[i].param_end;

    // Arc might wrap around 0.
    bool wrapped = (a_end <= a_start);
    if (wrapped) a_end += k2Pi;

    double test_angle = click_angle;
    if (wrapped && test_angle < a_start) test_angle += k2Pi;

    if (test_angle >= a_start - kAngTol && test_angle <= a_end + kAngTol) {
      return static_cast<int>(i);
    }
  }

  // Distance fallback: project click onto each arc chord.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double dist_sq = point_segment_distance_sq(
        click_x, click_y,
        segments[i].start_x, segments[i].start_y,
        segments[i].end_x, segments[i].end_y);
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_index = static_cast<int>(i);
    }
  }
  if (best_dist_sq <= kSegmentSelectTolerance * kSegmentSelectTolerance) {
    return best_index;
  }
  return -1;
}

// ================================================================
// Arc helpers
// ================================================================

namespace {

std::pair<double, double> arc_angles(const SketchArc& arc) {
  double a_start = wrap_angle(std::atan2(arc.start_y - arc.center_y,
                                          arc.start_x - arc.center_x));
  double a_end   = wrap_angle(std::atan2(arc.end_y - arc.center_y,
                                          arc.end_x - arc.center_x));
  return {a_start, a_end};
}

bool angle_in_arc_sweep(double a, double a_start, double a_end, bool ccw) {
  a = wrap_angle(a);
  a_start = wrap_angle(a_start);
  a_end   = wrap_angle(a_end);
  if (ccw) {
    if (a_end <= a_start) a_end += 2.0 * M_PI;
    if (a < a_start) a += 2.0 * M_PI;
    return a >= a_start - 1e-12 && a <= a_end + 1e-12;
  } else {
    // CW sweep: from a_start down to a_end.
    if (a_start <= a_end) a_start += 2.0 * M_PI;
    if (a > a_start) a -= 2.0 * M_PI;
    return a <= a_start + 1e-12 && a >= a_end - 1e-12;
  }
}

}  // namespace

// ================================================================
// intersect_arc_line
// ================================================================

std::vector<TrimIntersection> intersect_arc_line(
    const SketchArc& target,
    const SketchLine& other) {
  auto isects = intersect_line_circle(other, SketchCircle{
      .center_x = target.center_x,
      .center_y = target.center_y,
      .radius   = target.radius,
  });

  auto [a_start, a_end] = arc_angles(target);

  std::vector<TrimIntersection> result;
  for (auto& is : isects) {
    // is.param_on_other is the angle on the circle.
    if (angle_in_arc_sweep(is.param_on_other, a_start, a_end, target.ccw)) {
      // Remap: param_on_target = angle on arc, param_on_other = t on line.
      result.push_back(TrimIntersection{
          .x = is.x,
          .y = is.y,
          .param_on_target = is.param_on_other,
          .param_on_other  = is.param_on_target,
          .other_entity_id = other.id,
      });
    }
  }
  return result;
}

// ================================================================
// intersect_arc_circle
// ================================================================

std::vector<TrimIntersection> intersect_arc_circle(
    const SketchArc& target,
    const SketchCircle& other) {
  auto isects = intersect_circle_circle(
      SketchCircle{.center_x = target.center_x, .center_y = target.center_y, .radius = target.radius},
      other);

  auto [a_start, a_end] = arc_angles(target);

  std::vector<TrimIntersection> result;
  for (auto& is : isects) {
    if (angle_in_arc_sweep(is.param_on_target, a_start, a_end, target.ccw)) {
      is.other_entity_id = other.id;
      result.push_back(std::move(is));
    }
  }
  return result;
}

// ================================================================
// intersect_arc_arc
// ================================================================

std::vector<TrimIntersection> intersect_arc_arc(
    const SketchArc& target,
    const SketchArc& other) {
  auto isects = intersect_circle_circle(
      SketchCircle{.center_x = target.center_x, .center_y = target.center_y, .radius = target.radius},
      SketchCircle{.center_x = other.center_x, .center_y = other.center_y, .radius = other.radius});

  auto [ta_start, ta_end] = arc_angles(target);
  auto [oa_start, oa_end] = arc_angles(other);

  std::vector<TrimIntersection> result;
  for (auto& is : isects) {
    if (angle_in_arc_sweep(is.param_on_target, ta_start, ta_end, target.ccw) &&
        angle_in_arc_sweep(is.param_on_other, oa_start, oa_end, other.ccw)) {
      is.other_entity_id = other.id;
      result.push_back(std::move(is));
    }
  }
  return result;
}

// ================================================================
// find_all_intersections (arc target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchArc& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  int total_lines = 0, total_circles = 0, total_arcs = 0, found = 0;

  for (const auto& line : params.lines) {
    ++total_lines;
    if (line.is_construction) continue;
    auto isects = intersect_arc_line(target, line);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& circle : params.circles) {
    ++total_circles;
    if (circle.is_construction) continue;
    auto isects = intersect_arc_circle(target, circle);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    if (arc.id == target.id) continue;
    ++total_arcs;
    if (arc.is_construction) continue;
    auto isects = intersect_arc_arc(target, arc);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  // Filter out intersections at the arc's own endpoints — they
  // produce zero-length segments and prevent meaningful trimming.
  {
    auto [as, ae] = arc_angles(target);
    const double kEpTol = 1e-6;
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [&](const TrimIntersection& is) {
                         double a = wrap_angle(is.param_on_target);
                         return (std::abs(std::sin(a - as)) < kEpTol &&
                                 std::abs(std::cos(a - as) - 1.0) < kEpTol) ||
                                (std::abs(std::sin(a - ae)) < kEpTol &&
                                 std::abs(std::cos(a - ae) - 1.0) < kEpTol);
                       }),
        results.end());
  }

  if (results.empty()) return results;

  std::sort(results.begin(), results.end(),
            [](const TrimIntersection& a, const TrimIntersection& b) {
              return a.param_on_target < b.param_on_target;
            });

  deduplicate(results);
  return results;
}

// ================================================================
// split_arc_at_intersections
// ================================================================

std::vector<TrimSegment> split_arc_at_intersections(
    const SketchArc& arc,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  const double cx = arc.center_x, cy = arc.center_y, r = arc.radius;

  auto pt_at_angle = [&](double a) -> std::pair<double, double> {
    return {cx + r * std::cos(a), cy + r * std::sin(a)};
  };

  auto [a_start, a_end] = arc_angles(arc);
  const bool ccw = arc.ccw;

  // Build sorted list of angles within the arc sweep.
  std::vector<double> angles;
  for (const auto& is : intersections) {
    double a = wrap_angle(is.param_on_target);
    if (ccw) {
      if (a < a_start) a += 2.0 * M_PI;
    } else {
      if (a > a_start) a -= 2.0 * M_PI;
    }
    angles.push_back(a);
  }

  // Stably sort in sweep direction.
  std::sort(angles.begin(), angles.end(),
            ccw ? [](double a, double b) { return a < b; }
                : [](double a, double b) { return a > b; });

  std::vector<TrimSegment> segments;
  const double k2Pi = 2.0 * M_PI;

  // First segment: from arc start to first intersection.
  {
    double s = a_start, e = angles[0];
    if (ccw) {
      if (e <= s) e += k2Pi;
    } else {
      if (e >= s) e -= k2Pi;
    }
    auto [sx, sy] = pt_at_angle(s);
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back({TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  }

  // Middle segments.
  for (size_t i = 0; i + 1 < angles.size(); ++i) {
    double s = angles[i], e = angles[i + 1];
    auto [sx, sy] = pt_at_angle(wrap_angle(s));
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back({TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  }

  // Last segment: from last intersection to arc end.
  {
    double s = angles.back(), e = a_end;
    if (ccw) {
      if (e <= s) e += k2Pi;
    } else {
      if (e >= s) e -= k2Pi;
    }
    auto [sx, sy] = pt_at_angle(wrap_angle(s));
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back({TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  }

  return segments;
}

// ================================================================
// select_clicked_segment (arc)
// ================================================================

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchArc& original_arc,
    double click_x,
    double click_y) {
  if (segments.empty()) return -1;

  const double cx = original_arc.center_x;
  const double cy = original_arc.center_y;

  double click_angle = std::atan2(click_y - cy, click_x - cx);
  click_angle = wrap_angle(click_angle);

  auto [a_start, a_end] = arc_angles(original_arc);
  const bool ccw = original_arc.ccw;
  const double k2Pi = 2.0 * M_PI;
  constexpr double kAngTol = 1e-10;

  // Adjust click angle into the arc sweep.
  double test_angle = click_angle;
  if (ccw) {
    if (test_angle < a_start) test_angle += k2Pi;
  } else {
    if (test_angle > a_start) test_angle -= k2Pi;
  }

  for (size_t i = 0; i < segments.size(); ++i) {
    double s = segments[i].param_start;
    double e = segments[i].param_end;
    if (ccw) {
      if (e <= s) e += k2Pi;
    } else {
      if (e >= s) e -= k2Pi;
    }
    double ta = test_angle;
    if (ccw && ta < s) ta += k2Pi;
    if (!ccw && ta > s) ta -= k2Pi;
    if (ta >= s - kAngTol && ta <= e + kAngTol) return static_cast<int>(i);
  }

  // Distance fallback.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double dist_sq = point_segment_distance_sq(
        click_x, click_y,
        segments[i].start_x, segments[i].start_y,
        segments[i].end_x, segments[i].end_y);
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_index = static_cast<int>(i);
    }
  }
  if (best_dist_sq <= kSegmentSelectTolerance * kSegmentSelectTolerance) {
    return best_index;
  }
  return -1;
}

}  // namespace polysmith::core
