#include "core/sketch/trim_engine.h"

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

#include "core/sketch/impl/trim_line_circle_intersections.inc"
#include "core/sketch/impl/trim_line_circle_segments.inc"
#include "core/sketch/impl/trim_arc_operations.inc"

}  // namespace polysmith::core
