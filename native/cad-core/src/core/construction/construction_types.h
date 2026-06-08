#pragma once

#include <string>
#include <vector>

#include "core/document/plane_frame.h"

namespace polysmith::core {

struct ConstructionPlaneFeatureParameters {
  std::string plane_type = "offset";
  std::string source_plane_id;
  std::vector<std::string> source_plane_ids;
  std::string source_axis_id;
  double offset;
  double angle_degrees = 0.0;
  PlaneFrame plane_frame;
};

struct ConstructionAxisFeatureParameters {
  std::string source_id;
  std::string source_kind;
  double start_x = 0.0;
  double start_y = 0.0;
  double start_z = 0.0;
  double end_x = 0.0;
  double end_y = 0.0;
  double end_z = 0.0;
};

struct ConstructionPointFeatureParameters {
  std::string source_id;
  std::string source_kind;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

}  // namespace polysmith::core
