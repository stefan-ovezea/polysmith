#pragma once

#include <optional>
#include <string>
#include <vector>
#include "core/document/plane_frame.h"

namespace polysmith::core {

struct ViewportReferencePlane {
  std::string id;
  std::string label;
  // "xy" | "yz" | "xz" for the three origin planes — the renderer
  // applies a hardcoded rotation for those (legacy path). For
  // construction planes we set this to "custom" and ship a real
  // `plane_frame` instead; the renderer uses the frame to position
  // and orient the quad in world space.
  std::string orientation;
  double center_x;
  double center_y;
  double center_z;
  double width;
  double height;
  bool is_selected;
  bool is_active_sketch_plane;
  // World-space frame for construction planes. nullopt for origin
  // planes, which the renderer continues to handle via `orientation`.
  std::optional<PlaneFrame> plane_frame;
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

struct ViewportReferencePoint {
  std::string id;
  std::string label;
  double x;
  double y;
  double z;
  bool is_selected;
};

struct ViewportHelixPrimitive {
  std::string id;
  std::string label;
  std::vector<double> points;
  bool is_selected;
};

}  // namespace polysmith::core
