#pragma once

#include <optional>
#include <string>
#include <vector>
#include "core/sketch/sketch_profile.h"

namespace polysmith::core {

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
  std::optional<std::string> appearance_color;
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
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

struct ViewportPolygonExtrudePrimitive {
  std::string id;
  std::string label;
  std::string plane_id;
  std::optional<ViewportSketchPlaneFrame> plane_frame;
  std::vector<SketchProfilePoint> profile_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  double depth;
  bool is_selected;
  std::optional<std::string> appearance_color;
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

  // owner_id is the body id that owns this face. For body-derived faces
  // (the only kind today) face_id is "<owner_id>:face:<index>" where the
  // index comes from TopExp::MapShapes(TopAbs_FACE) on the body shape, so
  // ids are stable as long as topology is unchanged.
  std::string face_id;
  std::string owner_id;
  std::string owner_kind;
  std::string label;
  // "planar" -> the underlying surface is a plane; sketch-on-face is
  // allowed and `plane_frame` is meaningful.
  // "non-planar" -> curved surface (fillet, cylinder side, etc.);
  // sketch-on-face is rejected; `plane_frame` is a representative frame
  // (face center + a derived normal) and shouldn't drive sketching.
  std::string sketchability;
  double center_x;
  double center_y;
  double center_z;
  double normal_x;
  double normal_y;
  double normal_z;
  PlaneFrame plane_frame;
  // Legacy analytical face dimensions. Used by per-feature analytical
  // face emission for legacy box/cylinder primitives that aren't
  // body-derived. Body-derived faces leave these at 0 and rely on the
  // triangulation arrays below.
  double width;
  double height;
  double radius;
  // World-space triangulation of the actual face. The UI uses this to
  // build a real BufferGeometry per face, which is both visually
  // accurate and gives precise raycasting (the picker hits the face
  // shape, not an analytical bounding rectangle). Empty for legacy
  // analytical faces — those still build their pick mesh from
  // (width, height, radius) + plane_frame on the UI side.
  // Layout: positions are flat x0,y0,z0,x1,y1,z1,...; indices are
  // triangle vertex indices into positions.
  std::vector<double> triangle_positions;
  std::vector<int> triangle_indices;
  bool is_selected;
  std::optional<std::string> appearance_color;
};

}  // namespace polysmith::core
