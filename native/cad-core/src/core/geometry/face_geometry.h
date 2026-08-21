#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/document/feature.h"

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

// A partial-circular edge of a wire outline. `start`/`end` coincide with
// the adjacent polygon corners (the segment the arc spans). `axis` is the
// unit normal of the arc's plane — used only to detect non-parallel
// projections; the sweep direction is computed at projection time from
// the sketch-local start/end angles (wire edges can be reversed relative
// to their underlying curves, so the axis alone doesn't give the winding).
struct FaceOutlineArc {
  FaceOutlinePoint start;
  FaceOutlinePoint end;
  FaceOutlinePoint center;
  FaceOutlinePoint axis;
  double radius;
  // A point ON the arc between start and end (the curve midpoint).
  // Wire edges can be reversed relative to their underlying curves, so
  // the sweep direction is derived from this point at projection time
  // instead of from the axis.
  FaceOutlinePoint mid;
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

  // kind == "polygon": parallel to polygon_corners; entry i describes the
  // segment from polygon_corners[i] to polygon_corners[(i+1) % n].
  // nullopt = straight line; set = circular arc. Arc junction corners are
  // never removed by the merge/simplify passes.
  std::vector<std::optional<FaceOutlineArc>> polygon_segment_arcs;
  std::vector<std::vector<std::optional<FaceOutlineArc>>> inner_segment_arcs;

  // Populated when kind == "circle" (annular faces) AND when kind ==
  // "polygon" with circular through-holes (previously those holes were
  // chord-sampled into 64-segment polylines in inner_loops).
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

// Douglas-Peucker simplification of a closed outline loop (open point
// list — the caller closes it). Keeps vertices so the simplified loop
// deviates at most `tolerance` (mm) from the original. Used to decimate
// chord-sampled outlines from converted meshes before projection.
// `must_keep` (optional, same size as points) forces vertices to
// survive — arc junction corners must never be simplified away or the
// arc segment would be orphaned.
std::vector<FaceOutlinePoint> simplify_outline_polyline(
    const std::vector<FaceOutlinePoint>& points,
    double tolerance,
    const std::vector<bool>* must_keep = nullptr,
    std::vector<size_t>* kept_indices = nullptr);

struct PlanarFaceProfile {
  PlaneFrame plane_frame;
  std::vector<SketchProfilePoint> outer_points;
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
};

std::optional<PlanarFaceProfile> compute_planar_face_profile(
    const DocumentState& document,
    const std::string& face_id);

}  // namespace polysmith::core
