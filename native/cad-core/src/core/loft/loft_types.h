#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/document/plane_frame.h"
#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct LoftSectionParameters {
  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> profile_points;
};

struct LoftFeatureParameters {
  std::vector<LoftSectionParameters> sections;
  bool ruled = false;
};

}  // namespace polysmith::core
