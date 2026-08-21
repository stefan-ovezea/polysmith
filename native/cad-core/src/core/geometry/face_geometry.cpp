#include "core/geometry/face_geometry.h"

#include <algorithm>
#include <cmath>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"

namespace polysmith::core {
namespace {
#include "core/geometry/impl/face_id_helpers.inc"
#include "core/geometry/impl/face_outline_wire_helpers.inc"
#include "core/geometry/impl/planar_face_frame_helpers.inc"
#include "core/geometry/impl/extrude_face_outline_helpers.inc"
#include "core/geometry/impl/body_face_resolution_helpers.inc"
#include "core/geometry/impl/planar_profile_loop_helpers.inc"

}  // namespace

namespace {

double point_segment_distance(const FaceOutlinePoint& p,
                              const FaceOutlinePoint& a,
                              const FaceOutlinePoint& b) {
  const double abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
  const double ab_sq = abx * abx + aby * aby + abz * abz;
  if (ab_sq <= 1e-24) {
    const double dx = p.x - a.x, dy = p.y - a.y, dz = p.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
  }
  const double apx = p.x - a.x, apy = p.y - a.y, apz = p.z - a.z;
  const double t = std::clamp((apx * abx + apy * aby + apz * abz) / ab_sq,
                              0.0, 1.0);
  const double cx = a.x + t * abx - p.x;
  const double cy = a.y + t * aby - p.y;
  const double cz = a.z + t * abz - p.z;
  return std::sqrt(cx * cx + cy * cy + cz * cz);
}

}  // namespace

std::vector<FaceOutlinePoint> simplify_outline_polyline(
    const std::vector<FaceOutlinePoint>& points,
    double tolerance,
    const std::vector<bool>* must_keep,
    std::vector<size_t>* kept_indices) {
  if (points.size() < 4 || tolerance <= 0.0) {
    return points;
  }

  // Work on the closed loop: duplicate the first point as the closing
  // anchor. `closed` index n maps back to original index 0.
  const size_t n = points.size();
  std::vector<FaceOutlinePoint> closed = points;
  closed.push_back(points.front());

  std::vector<bool> keep(n, false);
  keep[0] = true;

  struct Range {
    size_t lo;
    size_t hi;
  };
  std::vector<Range> stack{{0, n}};
  while (!stack.empty()) {
    const Range range = stack.back();
    stack.pop_back();
    double max_distance = -1.0;
    size_t farthest = range.lo + 1;
    for (size_t i = range.lo + 1; i < range.hi; ++i) {
      const double distance =
          point_segment_distance(closed[i], closed[range.lo], closed[range.hi]);
      if (distance > max_distance) {
        max_distance = distance;
        farthest = i;
      }
    }
    if (max_distance > tolerance) {
      keep[farthest % n] = true;
      stack.push_back({range.lo, farthest});
      stack.push_back({farthest, range.hi});
    }
  }

  // Arc junction corners must survive the decimation — a fillet smaller
  // than the tolerance has sagitta below it and the DP would otherwise
  // delete the junction and orphan the arc segment.
  if (must_keep != nullptr && must_keep->size() == n) {
    for (size_t i = 0; i < n; ++i) {
      if ((*must_keep)[i]) {
        keep[i] = true;
      }
    }
  }

  std::vector<FaceOutlinePoint> result;
  result.reserve(n);
  std::vector<size_t> kept;
  kept.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    if (keep[i]) {
      result.push_back(points[i]);
      kept.push_back(i);
    }
  }
  // Never degenerate below a triangle — a null outline would make the
  // face unprojectable.
  if (result.size() < 3) {
    return points;
  }
  if (kept_indices != nullptr) {
    *kept_indices = std::move(kept);
  }
  return result;
}

std::optional<FaceOutline> compute_face_outline(const DocumentState& document,
                                                const std::string& face_id) {
  const auto parsed = parse_face_id(face_id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  // Body-derived face ids ship a numeric index suffix. Resolve those
  // through the OCCT body shape so projections work for booleaned,
  // filleted, chamfered, and plane-frame-rotated faces.
  int face_index = -1;
  if (suffix_is_numeric_index(parsed->suffix, face_index)) {
    return outline_for_body_face(document, parsed->owner_id, face_index);
  }

  // Legacy named-suffix face ids: handled per source feature.
  const FeatureEntry* feature = find_feature(document, parsed->owner_id);
  if (feature == nullptr) {
    return std::nullopt;
  }

  if (feature->kind == "extrude" && feature->extrude_parameters.has_value()) {
    return outline_for_extrude(feature->extrude_parameters.value(),
                               parsed->suffix);
  }

  // Box and cylinder source features are placed in viewport-space using a
  // running x-offset and are not supported by the projection helper yet.
  return std::nullopt;
}

std::optional<PlanarFaceProfile> compute_planar_face_profile(
    const DocumentState& document,
    const std::string& face_id) {
  const auto parsed = parse_face_id(face_id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  int face_index = -1;
  if (!suffix_is_numeric_index(parsed->suffix, face_index)) {
    return std::nullopt;
  }

  const auto face = resolve_body_face(document, parsed->owner_id, face_index);
  if (!face.has_value()) {
    return std::nullopt;
  }

  const auto plane_frame = derive_planar_frame(face.value());
  if (!plane_frame.has_value()) {
    return std::nullopt;
  }

  TopoDS_Wire outer;
  try {
    outer = BRepTools::OuterWire(face.value());
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (outer.IsNull()) {
    return std::nullopt;
  }

  const auto outer_points = sample_wire_loop(outer);
  if (outer_points.size() < 3) {
    return std::nullopt;
  }

  PlanarFaceProfile profile{};
  profile.plane_frame = plane_frame.value();
  profile.outer_points = to_local_profile_loop(profile.plane_frame, outer_points);
  for (const auto& loop : inner_wire_loops(face.value(), outer)) {
    if (loop.size() >= 3) {
      profile.inner_loops.push_back(
          to_local_profile_loop(profile.plane_frame, loop));
    }
  }
  return profile;
}

}  // namespace polysmith::core
