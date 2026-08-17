#pragma once

#include <string>
#include <vector>

#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct FeatureEntry;

struct PolygonSketchProfile {
  std::string id;
  std::string plane_id;
  std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
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

std::vector<SketchProfileRegion> build_sketch_profile_regions(
    const SketchFeatureParameters& parameters);
DetectedSketchProfiles detect_sketch_profiles(const FeatureEntry& feature);

// Recompute a sketch feature's profile list from its current geometry
// (the exact-curve arrangement).  Used by the document loader: saved
// files can carry stale profile lists (saved before an arrangement
// fix, or saved while a region was invisible to the old pipeline) and
// must not be trusted verbatim.
void refresh_sketch_profiles(FeatureEntry& feature);

}  // namespace polysmith::core
