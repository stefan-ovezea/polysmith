#pragma once

#include <string>
#include <vector>

#include "core/feature.h"

namespace polysmith::core {

struct PolygonSketchProfile {
  std::string id;
  std::string plane_id;
  std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> points;
};

struct CircleSketchProfile {
  std::string id;
  std::string plane_id;
  std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame;
  double center_x;
  double center_y;
  double radius;
};

struct DetectedSketchProfiles {
  std::vector<PolygonSketchProfile> polygons;
  std::vector<CircleSketchProfile> circles;
};

DetectedSketchProfiles detect_sketch_profiles(const FeatureEntry& feature);

}  // namespace polysmith::core
