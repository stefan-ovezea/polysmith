#include "core/sketch/inference_engine.h"

#include <cmath>
#include <string>

namespace polysmith::core {
namespace {

constexpr double kCoincidentTolerance = 0.01; // mm

struct NearestPoint {
  std::string vertex_id;
  double x;
  double y;
};

// Find the nearest sketch point (by id) not owned by the given point id.
std::optional<NearestPoint> find_nearest_existing_point(
    const SketchFeatureParameters& params,
    double x,
    double y,
    const std::string& own_point_id) {
  std::optional<NearestPoint> result;
  double best_dist = kCoincidentTolerance;
  for (const auto& pt : params.vertices) {
    if (pt.id == own_point_id) continue;
    const double dx = pt.x - x;
    const double dy = pt.y - y;
    const double d = std::sqrt(dx * dx + dy * dy);
    if (d <= best_dist) {
      best_dist = d;
      result = NearestPoint{pt.id, pt.x, pt.y};
    }
  }
  return result;
}

bool constraint_already_exists(
    const SketchFeatureParameters& params,
    const std::string& kind,
    const std::vector<std::string>& target_ids) {
  for (const auto& c : params.constraints) {
    if (c.kind != kind) continue;
    if (c.target_ids.size() != target_ids.size()) continue;
    bool match = true;
    for (size_t i = 0; i < target_ids.size(); ++i) {
      if (c.target_ids[i] != target_ids[i]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

} // namespace

int run_inference_on_new_line(SketchFeatureParameters& params,
                              SketchLine& line) {
  int count = 0;

  // Only the *endpoint* of a newly drawn line is subject to inference.
  // The start point is a deliberate user placement and must not be
  // auto‑merged with nearby geometry — merging it can pull the entire
  // line onto an existing entity, making it invisible and creating a
  // phantom point sink that collects future constraints.

  auto end_near = find_nearest_existing_point(
      params, line.end_x, line.end_y, line.end_vertex_id);
  if (end_near.has_value() &&
      !constraint_already_exists(params, "coincident",
          {line.end_vertex_id, end_near->vertex_id})) {
    params.constraints.push_back(SketchConstraint{
        .constraint_id = "constraint-" +
            std::to_string(params.constraints.size() + 1),
        .kind = "coincident",
        .target_ids = {line.end_vertex_id, end_near->vertex_id},
    });
    ++count;
  }

  return count;
}

int run_inference_on_new_circle(SketchFeatureParameters& params,
                                SketchCircle& circle) {
  int count = 0;

  for (const auto& other : params.circles) {
    if (&other == &circle) continue;
    const double dx = other.center_x - circle.center_x;
    const double dy = other.center_y - circle.center_y;
    const double d = std::sqrt(dx * dx + dy * dy);
    if (d <= kCoincidentTolerance &&
        !constraint_already_exists(params, "concentric",
            {circle.id, other.id})) {
      params.constraints.push_back(SketchConstraint{
          .constraint_id = "constraint-" +
              std::to_string(params.constraints.size() + 1),
          .kind = "concentric",
          .target_ids = {circle.id, other.id},
      });
      ++count;
    }
  }

  return count;
}

} // namespace polysmith::core
