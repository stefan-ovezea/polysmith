#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/document/plane_frame.h"
#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct SweepFeatureParameters {
  struct PathSegment {
    std::string entity_id;
    std::string kind;  // "line" | "arc"
    double start_x = 0.0;
    double start_y = 0.0;
    double start_z = 0.0;
    double end_x = 0.0;
    double end_y = 0.0;
    double end_z = 0.0;
    double center_x = 0.0;
    double center_y = 0.0;
    double center_z = 0.0;
    double mid_x = 0.0;
    double mid_y = 0.0;
    double mid_z = 0.0;
    double radius = 0.0;
    bool ccw = true;
  };

  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::string path_sketch_feature_id;
  std::string path_entity_id;
  double path_start_x;
  double path_start_y;
  double path_start_z;
  double path_end_x;
  double path_end_y;
  double path_end_z;
  std::vector<PathSegment> path_segments;
};

}  // namespace polysmith::core
