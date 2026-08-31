#include "core/cam/laser/laser_tabs.h"

#include <algorithm>
#include <cmath>

namespace polysmith::core::laser {

namespace {

using cam2d::OffsetSegment;
using cam2d::XY;
using cam2d::kPi;
using cam2d::xy_length;

// Arc length of one segment.
double segment_length(const OffsetSegment& segment) {
  if (!segment.is_arc) {
    return xy_length(segment.end.x - segment.start.x,
                     segment.end.y - segment.start.y);
  }
  const double startAngle = std::atan2(segment.start.y - segment.center.y,
                                       segment.start.x - segment.center.x);
  const double endAngle = std::atan2(segment.end.y - segment.center.y,
                                     segment.end.x - segment.center.x);
  double sweep = endAngle - startAngle;
  if (xy_length(segment.end.x - segment.start.x,
                segment.end.y - segment.start.y) < 1e-9) {
    sweep = segment.cw ? -2.0 * kPi : 2.0 * kPi;
  } else if (segment.cw && sweep > 0) {
    sweep -= 2.0 * kPi;
  } else if (!segment.cw && sweep < 0) {
    sweep += 2.0 * kPi;
  }
  return segment.radius * std::abs(sweep);
}

// Point on a segment at arc-length fraction t (0..1 from start).
XY point_at(const OffsetSegment& segment, double t) {
  if (!segment.is_arc) {
    return XY{segment.start.x + (segment.end.x - segment.start.x) * t,
              segment.start.y + (segment.end.y - segment.start.y) * t};
  }
  const double startAngle = std::atan2(segment.start.y - segment.center.y,
                                       segment.start.x - segment.center.x);
  const double endAngle = std::atan2(segment.end.y - segment.center.y,
                                     segment.end.x - segment.center.x);
  double sweep = endAngle - startAngle;
  if (xy_length(segment.end.x - segment.start.x,
                segment.end.y - segment.start.y) < 1e-9) {
    sweep = segment.cw ? -2.0 * kPi : 2.0 * kPi;
  } else if (segment.cw && sweep > 0) {
    sweep -= 2.0 * kPi;
  } else if (!segment.cw && sweep < 0) {
    sweep += 2.0 * kPi;
  }
  const double angle = startAngle + sweep * t;
  return XY{segment.center.x + segment.radius * std::cos(angle),
            segment.center.y + segment.radius * std::sin(angle)};
}

// A straight piece of a segment between arc-length fractions t0..t1.
// Arc pieces keep their arc shape; the tab piece itself is a chord
// (tabs are small — the chord is indistinguishable at laser-off).
OffsetSegment slice(const OffsetSegment& segment, double t0, double t1,
                    bool chord) {
  OffsetSegment out;
  if (chord) {
    out.is_arc = false;
    out.start = point_at(segment, t0);
    out.end = point_at(segment, t1);
    return out;
  }
  if (!segment.is_arc) {
    out.is_arc = false;
    out.start = point_at(segment, t0);
    out.end = point_at(segment, t1);
    return out;
  }
  // Partial arc: same center/radius/sweep direction, trimmed span.
  out.is_arc = true;
  out.center = segment.center;
  out.radius = segment.radius;
  out.cw = segment.cw;
  out.start = point_at(segment, t0);
  out.end = point_at(segment, t1);
  return out;
}

}  // namespace

std::vector<TabbedSegment> apply_loop_tabs(
    const PlannedLoop& loop, const LaserCutParameters& params,
    std::vector<std::string>& warnings) {
  std::vector<TabbedSegment> result;
  if (!params.tabs_enabled || params.mode == "engrave") {
    return result;
  }
  if (loop.is_hole && !params.tabs_on_holes) {
    return result;  // standard: tabs on outer contours only
  }
  const double tabWidth = std::max(params.tab_width_mm, 0.05);
  if (loop.length < 3.0 * tabWidth) {
    warnings.push_back(
        "A contour is too short to carry tabs and was cut without them.");
    return result;
  }

  // Tab count and even placement along the loop's arc length.
  const double spacing = std::max(params.tab_spacing_mm, tabWidth * 2.0);
  const int tabCount = std::max(
      1, static_cast<int>(std::ceil(loop.length / spacing)));
  // Tab i spans [i·L/N − w/2, i·L/N + w/2]; the first span starts at
  // L/N − w/2 ≥ 0 as long as L/N ≥ w/2 (guaranteed by the 3·w guard
  // only when N ≤ 3 — clamp the span start to 0 for safety).
  const double span = loop.length / tabCount;
  double cursor = 0.0;
  for (const auto& segment : loop.segments) {
    const double length = segment_length(segment);
    const double segStart = cursor;
    const double segEnd = cursor + length;
    // Walk tab boundaries inside this segment.
    double pieceStart = segStart;
    for (int i = 0; i < tabCount; ++i) {
      double tabStart = span * i + (span - tabWidth) / 2.0;
      double tabEnd = tabStart + tabWidth;
      if (tabEnd <= segStart || tabStart >= segEnd) {
        continue;
      }
      tabStart = std::max(tabStart, segStart);
      tabEnd = std::min(tabEnd, segEnd);
      if (pieceStart < tabStart) {
        TabbedSegment piece;
        piece.segment =
            slice(segment, (pieceStart - segStart) / length,
                  (tabStart - segStart) / length, /*chord=*/false);
        result.push_back(std::move(piece));
      }
      TabbedSegment tab;
      tab.segment = slice(segment, (tabStart - segStart) / length,
                          (tabEnd - segStart) / length, /*chord=*/true);
      tab.in_tab = true;
      result.push_back(std::move(tab));
      pieceStart = tabEnd;
    }
    if (pieceStart < segEnd) {
      TabbedSegment piece;
      piece.segment =
          slice(segment, (pieceStart - segStart) / length,
                (segEnd - segStart) / length, /*chord=*/false);
      result.push_back(std::move(piece));
    }
    cursor = segEnd;
  }
  return result;
}

}  // namespace polysmith::core::laser
