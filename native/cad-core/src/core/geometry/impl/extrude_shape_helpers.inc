TopoDS_Shape make_polygon_prism_shape(const ExtrudeFeatureParameters& parameters) {
  TopoDS_Compound compound;
  BRep_Builder builder;
  builder.MakeCompound(compound);

  if (parameters.profile_kind == "open_chain") {
    const std::vector<SketchProfilePoint> wall = make_open_thin_wall(parameters);
    builder.Add(compound, make_polygon_prism_shape(parameters, wall, {}));
    return compound;
  }

  std::vector<SketchProfilePoint> primary_points;
  std::vector<std::vector<SketchProfilePoint>> primary_inner_loops =
      parameters.inner_loops;
  apply_closed_thin_profile(parameters,
                            parameters.profile_points,
                            primary_points,
                            primary_inner_loops);
  builder.Add(compound,
              make_polygon_prism_shape(parameters, primary_points, primary_inner_loops));
  for (size_t index = 0; index < parameters.additional_profile_points.size();
       ++index) {
    std::vector<SketchProfilePoint> profile_points;
    std::vector<std::vector<SketchProfilePoint>> inner_loops =
        index < parameters.additional_inner_loops.size()
            ? parameters.additional_inner_loops[index]
            : std::vector<std::vector<SketchProfilePoint>>{};
    apply_closed_thin_profile(parameters,
                              parameters.additional_profile_points[index],
                              profile_points,
                              inner_loops);
    builder.Add(compound,
                make_polygon_prism_shape(parameters, profile_points, inner_loops));
  }

  return compound;
}

std::vector<SketchProfilePoint> sample_circle_points(double center_x,
                                                     double center_y,
                                                     double radius) {
  constexpr int kCircleSegments = 96;
  std::vector<SketchProfilePoint> points;
  points.reserve(kCircleSegments);
  for (int index = 0; index < kCircleSegments; ++index) {
    const double angle =
        (static_cast<double>(index) / static_cast<double>(kCircleSegments)) *
        2.0 * kPi;
    points.push_back(SketchProfilePoint{
        .x = center_x + radius * std::cos(angle),
        .y = center_y + radius * std::sin(angle),
    });
  }
  return points;
}

TopoDS_Shape make_circle_extrude_shape(
    const ExtrudeFeatureParameters& parameters,
    double taper_angle_degrees = 0.0) {
  const double signed_depth = parameters.depth;
  const double abs_depth = std::abs(signed_depth);
  const double end_radius =
      parameters.radius - abs_depth * std::tan(taper_angle_degrees * kPi / 180.0);
  if (end_radius < -1.0e-7) {
    throw std::runtime_error(
        "Extrude taper is too steep for this circular profile. Reduce the taper "
        "angle, increase the circle radius, or reduce the distance.");
  }
  const double clamped_end_radius = std::max(0.0, end_radius);
  const bool has_tapered_radius =
      std::abs(clamped_end_radius - parameters.radius) > 1.0e-7;

  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const gp_Pnt center =
        to_world_point(frame, parameters.start_x, parameters.start_y);
    const double sign = signed_depth >= 0.0 ? 1.0 : -1.0;
    const gp_Dir axis_direction(frame.normal_x * sign,
                                frame.normal_y * sign,
                                frame.normal_z * sign);
    const gp_Ax2 axis(center, axis_direction);
    const TopoDS_Shape shape =
        !has_tapered_radius
            ? BRepPrimAPI_MakeCylinder(axis, parameters.radius, abs_depth).Shape()
            : BRepPrimAPI_MakeCone(
                  axis, parameters.radius, clamped_end_radius, abs_depth)
                  .Shape();
    if (shape.IsNull()) {
      throw std::runtime_error("Failed to build circle extrude on plane frame");
    }
    return shape;
  }

  const gp_Pnt center = to_world_point(parameters.plane_id,
                                       parameters.start_x,
                                       parameters.start_y);
  const double sign = signed_depth >= 0.0 ? 1.0 : -1.0;
  const gp_Dir axis_direction =
      parameters.plane_id == "ref-plane-xy"
          ? gp_Dir(0.0, sign, 0.0)
          : parameters.plane_id == "ref-plane-yz"
                ? gp_Dir(sign, 0.0, 0.0)
                : gp_Dir(0.0, 0.0, sign);
  const gp_Ax2 axis(center, axis_direction);
  const TopoDS_Shape shape =
      !has_tapered_radius
          ? BRepPrimAPI_MakeCylinder(axis, parameters.radius, abs_depth).Shape()
          : BRepPrimAPI_MakeCone(
                axis, parameters.radius, clamped_end_radius, abs_depth)
                .Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build circle extrude");
  }
  return shape;
}

ExtrudeFeatureParameters make_polygon_source(
    const ExtrudeFeatureParameters& parameters) {
  ExtrudeFeatureParameters source = parameters;
  if (source.profile_kind == "rectangle") {
    source.profile_kind = "polygon";
    source.profile_points = {
        {parameters.start_x, parameters.start_y},
        {parameters.start_x + parameters.width, parameters.start_y},
        {parameters.start_x + parameters.width,
         parameters.start_y + parameters.height},
        {parameters.start_x, parameters.start_y + parameters.height},
    };
  } else if (source.profile_kind == "circle" && source.thin.enabled) {
    source.profile_kind = "polygon";
    source.profile_points =
        sample_circle_points(parameters.start_x, parameters.start_y, parameters.radius);
  }
  return source;
}

TopoDS_Shape build_single_depth_extrude(
    const ExtrudeFeatureParameters& parameters) {
  if (parameters.profile_kind == "rectangle") {
    return make_polygon_prism_shape(make_polygon_source(parameters));
  }
  if (parameters.profile_kind == "circle") {
    if (parameters.thin.enabled) {
      return make_polygon_prism_shape(make_polygon_source(parameters));
    }
    return make_circle_extrude_shape(parameters,
                                     parameters.side1.taper_angle_degrees);
  }
  if (parameters.profile_kind == "polygon" ||
      parameters.profile_kind == "open_chain") {
    return make_polygon_prism_shape(parameters);
  }
  throw std::runtime_error("Unsupported extrude profile kind: " +
                           parameters.profile_kind);
}

double resolved_side_distance(
    const ExtrudeFeatureParameters& parameters,
    const ExtrudeFeatureParameters::SideParameters& side,
    bool symmetric) {
  double distance = side.distance;
  if (distance <= 0.0) {
    distance = std::abs(parameters.depth);
  }
  if (symmetric) {
    distance *= 0.5;
  }
  return distance;
}

TopoDS_Shape build_extrude_side(
    const ExtrudeFeatureParameters& parameters,
    const ExtrudeFeatureParameters::SideParameters& side,
    double side_sign,
    bool symmetric) {
  ExtrudeFeatureParameters side_parameters = make_polygon_source(parameters);
  side_parameters.depth =
      side_sign * resolved_side_distance(parameters, side, symmetric);
  side_parameters.extent_mode = "one_side";
  side_parameters.side1 = side;
  side_parameters.side2 = std::nullopt;

  TopoDS_Shape shape = build_single_depth_extrude(side_parameters);
  shape = translate_shape(shape, extrusion_vector(parameters,
                                                  side_sign * side.start_offset));
  shape = apply_draft_angle(shape,
                            parameters,
                            side_sign,
                            side.taper_angle_degrees,
                            side.start_offset);
  return shape;
}
