#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/document/plane_frame.h"
#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct RevolveFeatureParameters {
  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::string axis_sketch_feature_id;
  std::string axis_entity_id;
  double axis_start_x;
  double axis_start_y;
  double axis_start_z;
  double axis_end_x;
  double axis_end_y;
  double axis_end_z;
  double angle_degrees = 360.0;
};

}  // namespace polysmith::core
