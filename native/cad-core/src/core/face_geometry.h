#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/feature.h"

namespace polysmith::core {

struct DocumentState;

struct FaceOutlinePoint {
  double x;
  double y;
  double z;
};

struct FaceOutline {
  // "rectangle" or "circle". Polygon top/bottom and polygon-side faces
  // are not yet supported by the projection helper and produce nullopt.
  std::string kind;

  // Populated when kind == "rectangle". Four world-space corners in
  // outline order (closed loop).
  std::vector<FaceOutlinePoint> rectangle_corners;

  // Populated when kind == "circle".
  FaceOutlinePoint circle_center;
  FaceOutlinePoint circle_axis;  // unit normal to the disc
  double circle_radius;
};

// Resolve a face id of the form "{owner_feature_id}:face:{suffix}" against
// the current document and produce a world-space outline. Returns nullopt
// when the face is not supported by the projection helper (see Project
// roadmap).
std::optional<FaceOutline> compute_face_outline(const DocumentState& document,
                                                const std::string& face_id);

}  // namespace polysmith::core
