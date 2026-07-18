#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/document/plane_frame.h"
#include "core/sketch/sketch_geometry_types.h"
#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct ExtrudeFeatureParameters {
  struct SideParameters {
    // "distance", "through_all", "to_object", or "to_next".
    std::string extent_type = "distance";
    double distance = 10.0;
    double start_offset = 0.0;
    double taper_angle_degrees = 0.0;
    std::optional<std::string> target_reference_id;
  };

  struct ThinParameters {
    bool enabled = false;
    double thickness = 1.0;
    // "center", "inside", or "outside".
    std::string placement = "center";
  };

  std::string sketch_feature_id;
  std::string profile_id;
  std::vector<std::string> profile_ids;
  std::vector<std::string> open_entity_ids;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  double start_x;
  double start_y;
  double width;
  double height;
  double radius;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::vector<std::vector<SketchProfilePoint>> additional_profile_points;
  std::vector<std::vector<std::vector<SketchProfilePoint>>> additional_inner_loops;
  // Sketch wire data — when non-empty, build_extrude_shape uses exact OCCT
  // curves (GC_MakeSegment / GC_MakeArcOfCircle / gp_Circ) instead of
  // polygon-approximating the boundary with BRepBuilderAPI_MakePolygon.
  std::vector<std::string> sketch_edge_ids;  // in boundary walk order
  std::vector<SketchLine> sketch_lines;
  std::vector<SketchArc> sketch_arcs;
  std::vector<SketchCircle> sketch_circles;
  double depth;
  // "one_side", "symmetric", or "two_sides".
  std::string extent_mode = "one_side";
  SideParameters side1{};
  std::optional<SideParameters> side2;
  ThinParameters thin{};
  std::string mode = "new_body";
  std::string operation = "new_body";
  std::string intersect_result = "replace_target";
  std::optional<std::string> target_body_id;
};

}  // namespace polysmith::core
