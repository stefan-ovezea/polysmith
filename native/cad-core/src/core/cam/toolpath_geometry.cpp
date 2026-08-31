#include "core/cam/toolpath_geometry.h"

#include <algorithm>
#include <cmath>

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
// Assumed rapid traverse speed for the time estimate (mm/min).  The
// real number is machine-dependent; this is a planning estimate only.
constexpr double kRapidFeedMmPerMin = 3000.0;

// Sweep angle of an arc move from `from` to `arc`, signed so that
// CW moves (kind == FeedArcCW) are negative and CCW positive.
double arc_sweep(const ToolpathMove& from, const ToolpathMove& arc) {
  const double cx = from.x + arc.i;
  const double cy = from.y + arc.j;
  const double radius = std::hypot(arc.i, arc.j);
  if (radius < 1e-9) {
    return 0.0;
  }
  const double startAngle = std::atan2(-arc.j, -arc.i);
  const double endAngle = std::atan2(arc.y - cy, arc.x - cx);
  double sweep = endAngle - startAngle;
  const bool clockwise = arc.kind == ToolpathMoveKind::FeedArcCW;
  // Full circles: the endpoint equals the start point, so the sweep
  // comes from the move kind alone.
  if (std::hypot(arc.x - from.x, arc.y - from.y) < 1e-9) {
    return clockwise ? -kTwoPi : kTwoPi;
  }
  // Normalize into the direction the move kind demands.
  if (clockwise && sweep > 0.0) {
    sweep -= kTwoPi;
  } else if (!clockwise && sweep < 0.0) {
    sweep += kTwoPi;
  }
  return sweep;
}

double arc_length(const ToolpathMove& from, const ToolpathMove& arc) {
  return std::hypot(arc.i, arc.j) * std::abs(arc_sweep(from, arc));
}

}  // namespace

double move_length(const ToolpathMove& from, const ToolpathMove& to) {
  if (to.kind == ToolpathMoveKind::FeedArcCW ||
      to.kind == ToolpathMoveKind::FeedArcCCW) {
    return arc_length(from, to);
  }
  return std::hypot(to.x - from.x, to.y - from.y,
                    to.z - from.z);
}

void linearize_arc_move(const ToolpathMove& from, const ToolpathMove& arc,
                        double chord_tolerance_mm,
                        std::vector<std::array<double, 3>>& out) {
  const double radius = std::hypot(arc.i, arc.j);
  const double sweep = arc_sweep(from, arc);
  if (radius < 1e-9 || std::abs(sweep) < 1e-9) {
    out.push_back({arc.x, arc.y, arc.z});
    return;
  }
  // Sagitta of a chord spanning angle θ: s = r (1 - cos(θ/2)).
  // Solve for the maximum angle meeting the tolerance.
  const double toleranceRatio =
      std::max(0.0, std::min(1.0, chord_tolerance_mm / radius));
  const double maxAngle = 2.0 * std::acos(1.0 - toleranceRatio);
  const int steps = std::max(
      1, static_cast<int>(std::ceil(std::abs(sweep) / maxAngle)));

  const double cx = from.x + arc.i;
  const double cy = from.y + arc.j;
  const double startAngle = std::atan2(-arc.j, -arc.i);
  for (int step = 1; step <= steps; ++step) {
    const double t = static_cast<double>(step) / steps;
    const double angle = startAngle + sweep * t;
    out.push_back({cx + radius * std::cos(angle),
                   cy + radius * std::sin(angle), arc.z});
  }
}

void finalize_toolpath(Toolpath& toolpath) {
  toolpath.bounds = Bounds3D{};
  bool havePoint = false;
  toolpath.total_length_mm = 0.0;
  toolpath.estimated_time_seconds = 0.0;
  toolpath.num_rapids = 0;
  toolpath.num_feeds = 0;
  toolpath.num_arcs = 0;
  toolpath.num_points = 0;

  ToolpathMove previous;
  bool havePrevious = false;
  for (const auto& move : toolpath.moves) {
    toolpath.num_points += 1;
    if (havePrevious) {
      toolpath.total_length_mm += move_length(previous, move);
    }
    if (move.kind == ToolpathMoveKind::Rapid) {
      toolpath.num_rapids += 1;
      if (havePrevious) {
        toolpath.estimated_time_seconds +=
            move_length(previous, move) / kRapidFeedMmPerMin * 60.0;
      }
    } else {
      toolpath.num_feeds += 1;
      if (move.kind == ToolpathMoveKind::FeedArcCW ||
          move.kind == ToolpathMoveKind::FeedArcCCW) {
        toolpath.num_arcs += 1;
      }
      const double feed = move.feedrate_mm_per_min > 0.0
                              ? move.feedrate_mm_per_min
                              : kRapidFeedMmPerMin;
      if (havePrevious) {
        toolpath.estimated_time_seconds +=
            move_length(previous, move) / feed * 60.0;
      }
    }

    if (!havePoint) {
      toolpath.bounds.min_x = toolpath.bounds.max_x = move.x;
      toolpath.bounds.min_y = toolpath.bounds.max_y = move.y;
      toolpath.bounds.min_z = toolpath.bounds.max_z = move.z;
      havePoint = true;
    } else {
      toolpath.bounds.min_x = std::min(toolpath.bounds.min_x, move.x);
      toolpath.bounds.max_x = std::max(toolpath.bounds.max_x, move.x);
      toolpath.bounds.min_y = std::min(toolpath.bounds.min_y, move.y);
      toolpath.bounds.max_y = std::max(toolpath.bounds.max_y, move.y);
      toolpath.bounds.min_z = std::min(toolpath.bounds.min_z, move.z);
      toolpath.bounds.max_z = std::max(toolpath.bounds.max_z, move.z);
    }

    previous = move;
    havePrevious = true;
  }
  if (toolpath.estimated_time_seconds > 0.0) {
    // Round to a whole second so the UI shows stable estimates.
    toolpath.estimated_time_seconds =
        std::ceil(toolpath.estimated_time_seconds);
  }
}

}  // namespace polysmith::core
