#include "core/geometry/extrude_helpers.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <array>
#include <limits>

#include "core/geometry/face_geometry.h"
#include "core/geometry/feature_shape.h"

namespace polysmith::core {

gp_Vec extrude_normal_vector(const ExtrudeFeatureParameters& params,
                             double side_sign) {
  if (params.plane_frame.has_value()) {
    return gp_Vec(params.plane_frame->normal_x * side_sign,
                  params.plane_frame->normal_y * side_sign,
                  params.plane_frame->normal_z * side_sign);
  }
  if (params.plane_id == "ref-plane-xy") {
    return gp_Vec(0.0, 0.0, side_sign);
  }
  if (params.plane_id == "ref-plane-yz") {
    return gp_Vec(side_sign, 0.0, 0.0);
  }
  return gp_Vec(0.0, side_sign, 0.0);
}

gp_Pnt extrude_plane_origin(const ExtrudeFeatureParameters& params) {
  if (params.plane_frame.has_value()) {
    return gp_Pnt(params.plane_frame->origin_x,
                  params.plane_frame->origin_y,
                  params.plane_frame->origin_z);
  }
  return gp_Pnt(0.0, 0.0, 0.0);
}

std::optional<std::pair<double, double>> body_projection_range(
    const TopoDS_Shape& shape, const gp_Pnt& origin, const gp_Vec& direction) {
  if (shape.IsNull()) {
    return std::nullopt;
  }
  Bnd_Box box;
  BRepBndLib::Add(shape, box);
  if (box.IsVoid()) {
    return std::nullopt;
  }
  double xmin = 0.0;
  double ymin = 0.0;
  double zmin = 0.0;
  double xmax = 0.0;
  double ymax = 0.0;
  double zmax = 0.0;
  box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  const std::array<gp_Pnt, 8> corners = {
      gp_Pnt(xmin, ymin, zmin),
      gp_Pnt(xmin, ymin, zmax),
      gp_Pnt(xmin, ymax, zmin),
      gp_Pnt(xmin, ymax, zmax),
      gp_Pnt(xmax, ymin, zmin),
      gp_Pnt(xmax, ymin, zmax),
      gp_Pnt(xmax, ymax, zmin),
      gp_Pnt(xmax, ymax, zmax),
  };
  double min_projection = std::numeric_limits<double>::infinity();
  double max_projection = -std::numeric_limits<double>::infinity();
  for (const auto& corner : corners) {
    const gp_Vec delta(origin, corner);
    const double projection = delta.Dot(direction);
    min_projection = std::min(min_projection, projection);
    max_projection = std::max(max_projection, projection);
  }
  return std::make_pair(min_projection, max_projection);
}

std::optional<double> distance_to_body_boundary(
    const CompiledBodies& compiled, const std::optional<std::string>& body_id,
    const gp_Pnt& origin, const gp_Vec& direction, double start_offset,
    bool through_all) {
  std::optional<double> best;
  for (const auto& body : compiled.bodies) {
    if (body_id.has_value() && body.id != body_id.value()) {
      continue;
    }
    const auto range = body_projection_range(body.shape, origin, direction);
    if (!range.has_value()) {
      continue;
    }
    const double target =
        through_all ? range->second + std::max(1.0, range->second - range->first) * 0.05
                    : (range->first > start_offset ? range->first : range->second);
    const double distance = target - start_offset;
    if (distance <= 1.0e-6) {
      continue;
    }
    if (!best.has_value() || distance < best.value()) {
      best = distance;
    }
  }
  return best;
}

std::optional<double> distance_to_face_plane(const DocumentState& document,
                                             const std::string& face_id,
                                             const gp_Pnt& origin,
                                             const gp_Vec& direction,
                                             double start_offset) {
  const auto profile = compute_planar_face_profile(document, face_id);
  if (!profile.has_value()) {
    return std::nullopt;
  }
  const gp_Pnt target_origin(profile->plane_frame.origin_x,
                             profile->plane_frame.origin_y,
                             profile->plane_frame.origin_z);
  const gp_Vec target_normal(profile->plane_frame.normal_x,
                             profile->plane_frame.normal_y,
                             profile->plane_frame.normal_z);
  const double denom = direction.Dot(target_normal);
  if (std::abs(denom) <= 1.0e-8) {
    return std::nullopt;
  }
  const double t = gp_Vec(origin, target_origin).Dot(target_normal) / denom;
  const double distance = t - start_offset;
  return distance > 1.0e-6 ? std::optional<double>(distance) : std::nullopt;
}

void resolve_extrude_side_extent(
    const DocumentState& document, ExtrudeFeatureParameters& params,
    ExtrudeFeatureParameters::SideParameters& side, double side_sign) {
  if (side.extent_type == "distance") {
    return;
  }

  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  const gp_Pnt origin = extrude_plane_origin(params);
  const gp_Vec direction = extrude_normal_vector(params, side_sign);
  std::optional<double> distance;

  if (side.extent_type == "to_object" && side.target_reference_id.has_value() &&
      side.target_reference_id->find(":face:") != std::string::npos) {
    distance = distance_to_face_plane(document,
                                      side.target_reference_id.value(),
                                      origin,
                                      direction,
                                      side.start_offset);
  } else if (side.extent_type == "through_all" ||
             side.extent_type == "to_object") {
    const std::optional<std::string> body_id =
        side.target_reference_id.has_value()
            ? side.target_reference_id
            : params.target_body_id;
    if (side.extent_type == "through_all" && !body_id.has_value()) {
      throw std::runtime_error("Through All extrude requires a target body");
    }
    distance = distance_to_body_boundary(compiled,
                                         body_id,
                                         origin,
                                         direction,
                                         side.start_offset,
                                         side.extent_type == "through_all");
  } else if (side.extent_type == "to_next") {
    distance = distance_to_body_boundary(compiled,
                                         std::nullopt,
                                         origin,
                                         direction,
                                         side.start_offset,
                                         false);
  }

  if (!distance.has_value()) {
    throw std::runtime_error("Unable to resolve extrude extent: " +
                             side.extent_type);
  }
  side.distance = distance.value();
}

void normalize_extrude_parameters(const DocumentState& document,
                                  ExtrudeFeatureParameters& params) {
  const double abs_depth = std::abs(params.depth);
  const bool default_side =
      params.side1.extent_type == "distance" &&
      params.side1.distance == 10.0 &&
      params.side1.start_offset == 0.0 &&
      params.side1.taper_angle_degrees == 0.0 &&
      !params.side1.target_reference_id.has_value();
  if (default_side) {
    params.side1.distance = abs_depth > 0.0 ? abs_depth : 10.0;
  }
  if (params.operation.empty()) {
    params.operation = params.mode.empty() ? "new_body" : params.mode;
  }
  if (params.operation == "join" && !params.target_body_id.has_value()) {
    params.mode = "new_body";
  } else {
    params.mode = params.operation;
  }

  if (params.extent_mode != "one_side" && params.extent_mode != "symmetric" &&
      params.extent_mode != "two_sides") {
    params.extent_mode = "one_side";
  }
  if (params.extent_mode == "symmetric" && !params.side2.has_value()) {
    params.side2 = params.side1;
  }
  if (params.extent_mode == "two_sides" && !params.side2.has_value()) {
    params.side2 = params.side1;
  }

  resolve_extrude_side_extent(document, params, params.side1, params.depth < 0 ? -1.0 : 1.0);
  if (params.side2.has_value()) {
    resolve_extrude_side_extent(document, params, params.side2.value(), params.depth < 0 ? 1.0 : -1.0);
  }

  if (params.extent_mode == "symmetric") {
    params.depth = (params.depth < 0.0 ? -1.0 : 1.0) * params.side1.distance;
  } else {
    params.depth = (params.depth < 0.0 ? -1.0 : 1.0) * params.side1.distance;
  }
}

bool shapes_intersect_with_volume(const TopoDS_Shape& a,
                                  const TopoDS_Shape& b) {
  if (a.IsNull() || b.IsNull()) {
    return false;
  }
  try {
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) {
      return false;
    }
    const TopoDS_Shape result = common.Shape();
    if (result.IsNull()) {
      return false;
    }
    if (TopExp_Explorer(result, TopAbs_SOLID).More()) {
      return true;
    }
  } catch (const std::exception&) {
  }
  Bnd_Box box_a;
  BRepBndLib::Add(a, box_a);
  Bnd_Box box_b;
  BRepBndLib::Add(b, box_b);
  const double overlap_x =
      std::min(box_a.CornerMax().X(), box_b.CornerMax().X()) -
      std::max(box_a.CornerMin().X(), box_b.CornerMin().X());
  const double overlap_y =
      std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y()) -
      std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y());
  const double overlap_z =
      std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z()) -
      std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z());
  if (overlap_x <= 0.0 || overlap_y <= 0.0 || overlap_z <= 0.0) {
    return false;
  }
  return overlap_x * overlap_y * overlap_z > 1e-3;
}

bool shapes_touch_without_volume(const TopoDS_Shape& a,
                                 const TopoDS_Shape& b) {
  if (a.IsNull() || b.IsNull()) {
    return false;
  }
  try {
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) {
      return false;
    }
    const TopoDS_Shape result = common.Shape();
    if (result.IsNull()) {
      return false;
    }
    const bool has_solid = TopExp_Explorer(result, TopAbs_SOLID).More();
    const bool has_face = TopExp_Explorer(result, TopAbs_FACE).More();
    const bool has_edge = TopExp_Explorer(result, TopAbs_EDGE).More();
    const bool has_vertex = TopExp_Explorer(result, TopAbs_VERTEX).More();
    if (has_solid) {
      Bnd_Box box_a;
      BRepBndLib::Add(a, box_a);
      Bnd_Box box_b;
      BRepBndLib::Add(b, box_b);
      const double overlap_x =
          std::min(box_a.CornerMax().X(), box_b.CornerMax().X()) -
          std::max(box_a.CornerMin().X(), box_b.CornerMin().X());
      const double overlap_y =
          std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y()) -
          std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y());
      const double overlap_z =
          std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z()) -
          std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z());
      const double overlap_volume =
          overlap_x > 0.0 && overlap_y > 0.0 && overlap_z > 0.0
              ? overlap_x * overlap_y * overlap_z
              : 0.0;
      const bool genuine_overlap = overlap_volume > 1e-3;
      return !genuine_overlap;
    }
    if (has_face || has_edge || has_vertex) {
      return true;
    }
  } catch (const std::exception&) {
  }
  Bnd_Box box_a;
  BRepBndLib::Add(a, box_a);
  Bnd_Box box_b;
  BRepBndLib::Add(b, box_b);
  const double overlap_x =
      std::min(box_a.CornerMax().X(), box_b.CornerMax().X()) -
      std::max(box_a.CornerMin().X(), box_b.CornerMin().X());
  const double overlap_y =
      std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y()) -
      std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y());
  const double overlap_z =
      std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z()) -
      std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z());
  const double overlap_volume =
      overlap_x > 0.0 && overlap_y > 0.0 && overlap_z > 0.0
          ? overlap_x * overlap_y * overlap_z
          : 0.0;
  const double gap_x =
      std::max(box_a.CornerMin().X(), box_b.CornerMin().X()) -
      std::min(box_a.CornerMax().X(), box_b.CornerMax().X());
  const double gap_y =
      std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y()) -
      std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y());
  const double gap_z =
      std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z()) -
      std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z());
  const double gap = std::max(gap_x, std::max(gap_y, gap_z));
  constexpr double kTouchTolerance = 1e-4;
  return overlap_volume <= 1e-3 && gap <= kTouchTolerance;
}

std::optional<std::string> find_intersecting_body_for_extrude(
    const DocumentState& document, const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) {
      continue;
    }
    if (shapes_intersect_with_volume(body.shape, candidate)) {
      return body.id;
    }
  }
  return std::nullopt;
}

std::optional<std::string> find_touching_body_for_extrude(
    const DocumentState& document, const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) {
      continue;
    }
    if (shapes_touch_without_volume(body.shape, candidate)) {
      return body.id;
    }
  }
  return std::nullopt;
}

}  // namespace polysmith::core
