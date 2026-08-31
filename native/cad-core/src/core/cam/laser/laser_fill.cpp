#include "core/cam/laser/laser_fill.h"

#include <algorithm>
#include <cmath>

namespace polysmith::core::laser {

namespace {

using cam2d::XY;
using cam2d::kPi;

XY rotate_by(const XY& p, double radians) {
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  return XY{p.x * c - p.y * s, p.x * s + p.y * c};
}

// Crossings of a horizontal scan line (at y) with one polygon.
void line_crossings(const std::vector<XY>& polygon, double y,
                    std::vector<double>& xs) {
  for (size_t i = 0; i < polygon.size(); ++i) {
    const XY& a = polygon[i];
    const XY& b = polygon[(i + 1) % polygon.size()];
    // Half-open rule: count the edge when it straddles y (avoids
    // double-counting shared vertices).
    const bool straddles =
        (a.y <= y && y < b.y) || (b.y <= y && y < a.y);
    if (!straddles) {
      continue;
    }
    xs.push_back(a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y));
  }
}

}  // namespace

std::vector<std::vector<FillLine>> hatch_region(
    const std::vector<PlannedLoop>& loops,
    const LaserCutParameters& params) {
  std::vector<std::vector<FillLine>> result;
  if (loops.empty()) {
    return result;
  }
  // The region's outer loop is its shallowest loop; everything else in
  // the group is a hole (excluded from the fill via even-odd).
  size_t outerIndex = 0;
  for (size_t i = 1; i < loops.size(); ++i) {
    if (loops[i].depth < loops[outerIndex].depth) {
      outerIndex = i;
    }
  }

  const double angle = params.fill_angle_deg * kPi / 180.0;
  const double spacing = std::max(params.line_spacing_mm, 0.05);

  // Rotate every polygon so the scan lines run horizontally.
  const auto rotated = [&](const PlannedLoop& loop) {
    std::vector<XY> points;
    points.reserve(loop.samples.size());
    for (const auto& p : loop.samples) {
      points.push_back(rotate_by(p, -angle));
    }
    return points;
  };
  std::vector<std::vector<XY>> polygons;
  polygons.reserve(loops.size());
  for (const auto& loop : loops) {
    polygons.push_back(rotated(loop));
  }

  double minY = 1e9;
  double maxY = -1e9;
  for (const auto& p : polygons[outerIndex]) {
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }
  if (maxY - minY < spacing) {
    return result;
  }

  for (double y = minY + 0.5 * spacing; y < maxY; y += spacing) {
    std::vector<double> xs;
    for (const auto& polygon : polygons) {
      line_crossings(polygon, y, xs);
    }
    std::sort(xs.begin(), xs.end());
    // Even-odd pairing: spans outside material start at even indices.
    std::vector<FillLine> spans;
    for (size_t k = 0; k + 1 < xs.size(); k += 2) {
      if (xs[k + 1] - xs[k] < 0.01) {
        continue;  // sliver span
      }
      const XY a = rotate_by(XY{xs[k], y}, angle);
      const XY b = rotate_by(XY{xs[k + 1], y}, angle);
      spans.push_back(FillLine{a, b});
    }
    if (!spans.empty()) {
      result.push_back(std::move(spans));
    }
  }
  return result;
}

}  // namespace polysmith::core::laser
