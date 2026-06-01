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

// ── Category-specific snap resolvers ──────────────────────────
// Each collects candidates from a single category and returns the
// closest by the category's native distance metric. Categories have
// absolute priority: Discrete > Direction > Continuous. Callers
// should try them in that order and stop at the first match.

// Discrete: fixed geometric points — endpoint, midpoint, center,
//           intersection, quadrant.
// Distance = Euclidean mm from cursor to the fixed point.
// exclude_point_id: when set, any candidate whose point_id matches
//   is dropped. Use this during endpoint drag to prevent the dragged
//   point from snapping to its own previous position.
std::optional<SnapCandidate> resolve_discrete_snaps(
    double cursor_x,
    double cursor_y,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    double tolerance,
    std::optional<std::string> exclude_point_id = std::nullopt);

// Direction: constraint rays projected from a start point —
//            axis_lock (H/V), parallel, perpendicular_direction, polar.
// Distance = perpendicular offset from the constraint ray through start.
// Requires start_x / start_y; returns nullopt without them.
// exclude_entity_ids: entity ids to drop from parallel / perpendicular-
//   direction candidates. Use this to prevent the dragged line from
//   matching against itself.
std::optional<SnapCandidate> resolve_direction_snaps(
    double cursor_x,
    double cursor_y,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    double tolerance,
    std::optional<double> start_x,
    std::optional<double> start_y,
    const std::vector<std::string>& exclude_entity_ids = {});

// Continuous: projection onto geometry — nearest (line/circle body),
//             perpendicular foot, tangent, grid, grid_line.
// Distance = projection offset from cursor to the geometry.
// start_x / start_y are optional (unused by current collectors but
// accepted for caller convenience).
// exclude_entity_ids: entity ids to drop from perpendicular foot
//   candidates — prevents the dragged line from snapping to its own
//   perpendicular foot (a meaningless constraint).
std::optional<SnapCandidate> resolve_continuous_snaps(
    double cursor_x,
    double cursor_y,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    double tolerance,
    std::optional<double> start_x = std::nullopt,
    std::optional<double> start_y = std::nullopt,
    const std::vector<std::string>& exclude_entity_ids = {});

// ── Legacy unified resolver ───────────────────────────────────
// Convenience wrapper that calls the three category resolvers with
// priority Discrete > Direction > Continuous. Prefer the category-
// specific functions in new code so tolerance can differ per pass.
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