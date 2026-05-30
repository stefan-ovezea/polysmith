#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/feature.h"

namespace polysmith::core {

// A single snap candidate resolved from the sketch geometry against the
// current cursor position. Returned by resolve_snap().
struct SnapCandidate {
  std::string kind;
  std::string entity_id;
  std::string point_id;
  double local_x;
  double local_y;
  double distance;
  std::string label;
  // Parametric position along the host entity, when meaningful.
  // For "nearest" (line-body): t in [0,1] along the line segment.
  // For "midpoint": 0.5.
  // For other kinds: -1.0 (undefined).
  double param_t = -1.0;
};

// Resolve the best snap candidate given the cursor position, active
// sketch geometry, current selection filter, and optional line start
// point for polar-snap angle computation.
std::optional<SnapCandidate> resolve_snap(
    double cursor_x,
    double cursor_y,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    double tolerance,
    std::optional<double> start_x = std::nullopt,
    std::optional<double> start_y = std::nullopt,
    const std::vector<std::string>& snap_priority = {});

const inline std::vector<std::string> kDefaultSnapPriority = {
    "endpoint",
    "center",
    "midpoint",
    "axis_lock",
    "intersection",
    "quadrant",
    "perpendicular",
    "perpendicular_direction",
    "tangent",
    "parallel",
    "polar",
    "grid",
    "grid_line",
    "nearest",
};

} // namespace polysmith::core