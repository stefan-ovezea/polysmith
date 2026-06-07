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

struct FaceOutlineCircle {
  FaceOutlinePoint center;
  FaceOutlinePoint axis;
  double radius;
};

struct FaceOutline {
  // "rectangle", "circle", or "polygon". Polygon outlines come from
  // body-derived faces (numeric face ids) where we walked the outer
  // wire and collected the line-segment endpoints in order.
  std::string kind;

  // Populated when kind == "rectangle". Four world-space corners in
  // outline order (closed loop).
  std::vector<FaceOutlinePoint> rectangle_corners;

  // Populated when kind == "polygon". World-space corners in outline
  // order (open list — the projector closes the loop by drawing back
  // to corner[0]).
  std::vector<FaceOutlinePoint> polygon_corners;
  std::vector<std::vector<FaceOutlinePoint>> inner_loops;

  // Populated when kind == "circle".
  FaceOutlinePoint circle_center;
  FaceOutlinePoint circle_axis;  // unit normal to the disc
  double circle_radius;
  std::vector<FaceOutlineCircle> inner_circles;
};

// Resolve a face id of the form "{owner_feature_id}:face:{suffix}" against
// the current document and produce a world-space outline. Returns nullopt
// when the face is not supported by the projection helper (see Project
// roadmap).
std::optional<FaceOutline> compute_face_outline(const DocumentState& document,
                                                const std::string& face_id);

struct PlanarFaceProfile {
  PlaneFrame plane_frame;
  std::vector<SketchProfilePoint> outer_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
};

std::optional<PlanarFaceProfile> compute_planar_face_profile(
    const DocumentState& document,
    const std::string& face_id);

}  // namespace polysmith::core
