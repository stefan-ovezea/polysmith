// Test whether two solid shapes intersect with non-zero volume. Used by
// the auto-cut detector at extrude-creation time so a new_body extrude
// that overlaps an existing body is silently promoted to a cut.
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
    // Any solid in the result -> the inputs share volume. We don't
    // dilute to TopAbs_FACE because two adjacent (touching but not
    // overlapping) solids share faces without sharing volume, and
    // promoting that to a cut would surprise the user.
    TopExp_Explorer explorer(result, TopAbs_SOLID);
    return explorer.More();
  } catch (const std::exception&) {
    return false;
  }
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
    if (TopExp_Explorer(result, TopAbs_SOLID).More()) {
      return false;
    }
    if (TopExp_Explorer(result, TopAbs_FACE).More() ||
        TopExp_Explorer(result, TopAbs_EDGE).More() ||
        TopExp_Explorer(result, TopAbs_VERTEX).More()) {
      return true;
    }
  } catch (const std::exception&) {
  }
  try {
    BRepExtrema_DistShapeShape distance(a, b);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= 1.0e-6;
  } catch (const std::exception&) {
    return false;
  }
}

// Check whether a candidate extrude (built from `parameters`) would
// overlap any existing body in `document`. Returns the body id of the
// first such body, or nullopt when no intersection is found. Bodies
// derived from features after the candidate (in feature_history order)
// are excluded — we only want to detect overlap with bodies that exist
// "now" from the user's point of view.
std::optional<std::string> find_intersecting_body_for_extrude(
    const DocumentState& document,
    const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
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
    const DocumentState& document,
    const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
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

std::string face_owner_id(const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return "";
  }

  return face_id.substr(0, separator);
}

bool is_supported_sketch_tool(const std::string& tool) {
  return tool == "select" || tool == "line" || tool == "rectangle" ||
         tool == "circle" || tool == "polygon" || tool == "arc" || tool == "fillet" ||
         tool == "trim" || tool == "project" || tool == "dimension";
}

std::string plane_id_from_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xy";
  }

  return "ref-plane-xz";
}

std::string plane_id_from_frame(const PlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xy";
  }

  return "ref-plane-xz";
}

PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  return PlaneFrame{
      .origin_x = plane_frame.origin_x,
      .origin_y = plane_frame.origin_y,
      .origin_z = plane_frame.origin_z,
      .x_axis_x = plane_frame.x_axis_x,
      .x_axis_y = plane_frame.x_axis_y,
      .x_axis_z = plane_frame.x_axis_z,
      .y_axis_x = plane_frame.y_axis_x,
      .y_axis_y = plane_frame.y_axis_y,
      .y_axis_z = plane_frame.y_axis_z,
      .normal_x = plane_frame.normal_x,
      .normal_y = plane_frame.normal_y,
      .normal_z = plane_frame.normal_z,
  };
}

gp_Vec extrude_normal_vector(const ExtrudeFeatureParameters& params,
                             double side_sign) {
  if (params.plane_frame.has_value()) {
    return gp_Vec(params.plane_frame->normal_x * side_sign,
                  params.plane_frame->normal_y * side_sign,
                  params.plane_frame->normal_z * side_sign);
  }
  if (params.plane_id == "ref-plane-xy") {
    return gp_Vec(0.0, side_sign, 0.0);
  }
  if (params.plane_id == "ref-plane-yz") {
    return gp_Vec(side_sign, 0.0, 0.0);
  }
  return gp_Vec(0.0, 0.0, side_sign);
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
    const TopoDS_Shape& shape,
    const gp_Pnt& origin,
    const gp_Vec& direction) {
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
    const CompiledBodies& compiled,
    const std::optional<std::string>& body_id,
    const gp_Pnt& origin,
    const gp_Vec& direction,
    double start_offset,
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

std::optional<double> distance_to_face_plane(
    const DocumentState& document,
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
    const DocumentState& document,
    ExtrudeFeatureParameters& params,
    ExtrudeFeatureParameters::SideParameters& side,
    double side_sign) {
  if (side.extent_type == "distance") {
    return;
  }

  const CompiledBodies compiled = compile_bodies(document);
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

