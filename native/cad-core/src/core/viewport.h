#pragma once

#include <optional>
#include <vector>

#include "core/document.h"

namespace polysmith::core {

struct ViewportBoxPrimitive {
  std::string id;
  std::string label;
  double width;
  double height;
  double depth;
  double x_offset;
  double center_x;
  double center_y;
  double center_z;
  bool is_selected;
};

struct ViewportCylinderPrimitive {
  std::string id;
  std::string label;
  double radius;
  double height;
  double x_offset;
  double center_x;
  double center_y;
  double center_z;
  bool is_selected;
};

struct ViewportSketchPlaneFrame {
  double origin_x;
  double origin_y;
  double origin_z;
  double x_axis_x;
  double x_axis_y;
  double x_axis_z;
  double y_axis_x;
  double y_axis_y;
  double y_axis_z;
  double normal_x;
  double normal_y;
  double normal_z;
};

struct ViewportPolygonExtrudePrimitive {
  std::string id;
  std::string label;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> profile_points;
  double depth;
  bool is_selected;
};

struct ViewportSolidFace {
  struct PlaneFrame {
    double origin_x;
    double origin_y;
    double origin_z;
    double x_axis_x;
    double x_axis_y;
    double x_axis_z;
    double y_axis_x;
    double y_axis_y;
    double y_axis_z;
    double normal_x;
    double normal_y;
    double normal_z;
  };

  std::string face_id;
  std::string owner_id;
  std::string owner_kind;
  std::string label;
  std::string sketchability;
  double center_x;
  double center_y;
  double center_z;
  double normal_x;
  double normal_y;
  double normal_z;
  PlaneFrame plane_frame;
  double width;
  double height;
  double radius;
  bool is_selected;
};

struct ViewportReferencePlane {
  std::string id;
  std::string label;
  std::string orientation;
  double center_x;
  double center_y;
  double center_z;
  double width;
  double height;
  bool is_selected;
  bool is_active_sketch_plane;
};

struct ViewportReferenceAxis {
  std::string id;
  std::string label;
  std::string axis;
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
};

struct ViewportSketchLinePrimitive {
  std::string line_id;
  std::string start_point_id;
  std::string end_point_id;
  std::string plane_id;
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
  bool is_selected;
  std::optional<std::string> constraint;
};

struct ViewportSketchCirclePrimitive {
  std::string circle_id;
  std::string plane_id;
  double center_x;
  double center_y;
  double center_z;
  double radius;
  bool is_selected;
};

struct ViewportSketchDimensionPrimitive {
  std::string dimension_id;
  std::string plane_id;
  std::string kind;
  std::string entity_id;
  std::string label;
  bool is_selected;
  double anchor_start_x;
  double anchor_start_y;
  double anchor_start_z;
  double anchor_end_x;
  double anchor_end_y;
  double anchor_end_z;
  double dimension_start_x;
  double dimension_start_y;
  double dimension_start_z;
  double dimension_end_x;
  double dimension_end_y;
  double dimension_end_z;
  double label_x;
  double label_y;
  double label_z;
};

struct ViewportSketchConstraintPrimitive {
  std::string constraint_id;
  std::string plane_id;
  std::string kind;
  std::string entity_id;
  std::optional<std::string> related_entity_id;
  std::string label;
  bool is_selected;
  double position_x;
  double position_y;
  double position_z;
};

struct ViewportSketchProfilePrimitive {
  std::string profile_id;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  std::string profile_kind;
  std::vector<SketchProfilePoint> profile_points;
  double start_x;
  double start_y;
  double width;
  double height;
  double radius;
  bool is_selected;
};

struct ViewportSceneBounds {
  double center_x;
  double center_y;
  double center_z;
  double width;
  double height;
  double depth;
  double max_dimension;
};

struct ViewportState {
  bool has_active_document;
  std::vector<ViewportBoxPrimitive> boxes;
  std::vector<ViewportCylinderPrimitive> cylinders;
  std::vector<ViewportPolygonExtrudePrimitive> polygon_extrudes;
  std::vector<ViewportSolidFace> solid_faces;
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  std::vector<ViewportSketchDimensionPrimitive> sketch_dimensions;
  std::vector<ViewportSketchConstraintPrimitive> sketch_constraints;
  std::vector<ViewportSketchProfilePrimitive> sketch_profiles;
  double scene_width;
  double scene_height;
  double scene_depth;
  ViewportSceneBounds scene_bounds;
};

ViewportState build_viewport_state(const std::optional<DocumentState>& document);

}  // namespace polysmith::core
