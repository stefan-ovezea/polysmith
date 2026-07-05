void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius,
                       bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  feature.sketch_parameters->circles.push_back(SketchCircle{
      .id = "circle-" + std::to_string(circle_index),
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
      .is_construction = is_construction,
  });
  const auto& circle = feature.sketch_parameters->circles.back();
  if (!is_construction) {
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-circle-" + circle.id,
        .kind = "circle_radius",
        .entity_id = circle.id,
        .value = circle.radius,
        .is_auto = true,
    });
  }
  if (!is_construction) {
    run_inference_on_new_circle(*feature.sketch_parameters,
                                feature.sketch_parameters->circles.back());
  }
  refresh_sketch_with_append_focus(feature, {circle.id});
}

void add_sketch_arc(FeatureEntry& feature,
                    int arc_index,
                    int start_point_index,
                    int end_point_index,
                    double start_x,
                    double start_y,
                    double end_x,
                    double end_y,
                    double center_x,
                    double center_y,
                    double radius,
                    bool ccw,
                    bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch arcs");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch arcs must have non-zero radius");
  }

  // Reuse an existing endpoint id when this arc's start / end land on
  // a point that another entity (line or arc) already owns. Mirrors
  // `add_sketch_line`'s behaviour so arcs and lines share endpoint
  // points whenever they meet at the same coordinates — the profile
  // loop detector relies on shared point ids (or the coord fallback)
  // to chain edges into a closed loop.
  const auto shared_start_point = find_coincident_endpoint(
      *feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point = find_coincident_endpoint(
      *feature.sketch_parameters, "", end_x, end_y);

  const std::string start_point_id =
      shared_start_point.has_value()
          ? std::get<0>(shared_start_point.value())
          : "point-" + std::to_string(start_point_index);
  const std::string end_point_id =
      shared_end_point.has_value()
          ? std::get<0>(shared_end_point.value())
          : "point-" + std::to_string(end_point_index);

  feature.sketch_parameters->arcs.push_back(SketchArc{
      .id = "arc-" + std::to_string(arc_index),
      .start_point_id = start_point_id,
      .end_point_id = end_point_id,
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .ccw = ccw,
      .is_construction = is_construction,
  });

  refresh_sketch_with_append_focus(
      feature, {"arc-" + std::to_string(arc_index)});

  // Arc endpoints start unfixed. The arc's shape is cached at creation;
  // the user can toggle fix on individual endpoints via the constraint
  // tool if they want to lock them.

}

std::optional<std::tuple<double, double>> find_point_position(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id)
      return std::tuple<double, double>{line.start_x, line.start_y};
    if (line.end_point_id == point_id)
      return std::tuple<double, double>{line.end_x, line.end_y};
  }
  const auto point_it = std::find_if(
      parameters.points.begin(), parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  if (point_it != parameters.points.end())
    return std::tuple<double, double>{point_it->x, point_it->y};
  return std::nullopt;
}
