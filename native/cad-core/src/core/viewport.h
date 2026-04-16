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
  std::string plane_id;
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
  bool is_selected;
  std::optional<std::string> constraint_hint;
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
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  double scene_width;
  double scene_height;
  double scene_depth;
  ViewportSceneBounds scene_bounds;
};

ViewportState build_viewport_state(const std::optional<DocumentState>& document);

}  // namespace polysmith::core
