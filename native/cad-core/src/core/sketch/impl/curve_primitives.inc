void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius,
                       bool is_construction,
                       bool auto_dimensions,
                       bool refresh_immediately) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  // Reuse an existing point id when the circle center lands on a
  // projected vertex or another entity's endpoint.  Mirrors
  // add_sketch_line / add_sketch_arc behaviour so that circles
  // centered on projected geometry stay attached on reload.
  const auto shared_center = find_coincident_endpoint(
      *feature.sketch_parameters, "", center_x, center_y);
  auto& params = *feature.sketch_parameters;
  const std::string center_vertex_id =
      shared_center.has_value()
          ? std::get<0>(shared_center.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);
  // Use the matched point's exact coordinates for the center so the
  // circle doesn't drift from the snap target.
  const double snapped_center_x =
      shared_center.has_value() ? std::get<1>(shared_center.value()) : center_x;
  const double snapped_center_y =
      shared_center.has_value() ? std::get<2>(shared_center.value()) : center_y;

  feature.sketch_parameters->circles.push_back(SketchCircle{
      .id = "circle-" + std::to_string(circle_index),
      .center_vertex_id = center_vertex_id,
      .center_x = snapped_center_x,
      .center_y = snapped_center_y,
      .radius = radius,
      .is_construction = is_construction,
  });
  const auto& circle = feature.sketch_parameters->circles.back();
  // Projected circles (auto_dimensions = false) skip the automatic
  // radius dimension — same rationale as add_sketch_line.
  if (!is_construction && auto_dimensions) {
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-circle-" + circle.id,
        .kind = "circle_radius",
        .entity_id = circle.id,
        .value = circle.radius,
        .is_auto = true,
    });
  }
  if (!is_construction && refresh_immediately) {
    run_inference_on_new_circle(*feature.sketch_parameters,
                                feature.sketch_parameters->circles.back());
  }
  if (refresh_immediately) {
    refresh_sketch_with_append_focus(feature, {circle.id});
  }
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
                    bool is_construction,
                    bool auto_dimensions,
                    bool refresh_immediately) {
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

  auto& params = *feature.sketch_parameters;
  const std::string start_vertex_id =
      shared_start_point.has_value()
          ? std::get<0>(shared_start_point.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);
  const std::string end_vertex_id =
      shared_end_point.has_value()
          ? std::get<0>(shared_end_point.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);

  // Use the matched point's exact coordinates for start/end so the
  // arc's cached position doesn't drift from the snap target.
  const double snapped_arc_start_x =
      shared_start_point.has_value() ? std::get<1>(shared_start_point.value()) : start_x;
  const double snapped_arc_start_y =
      shared_start_point.has_value() ? std::get<2>(shared_start_point.value()) : start_y;
  const double snapped_arc_end_x =
      shared_end_point.has_value() ? std::get<1>(shared_end_point.value()) : end_x;
  const double snapped_arc_end_y =
      shared_end_point.has_value() ? std::get<2>(shared_end_point.value()) : end_y;

  // Reuse an existing point id when the arc center lands on a
  // projected vertex or another entity's endpoint — same rationale
  // as circle centers and line endpoints.
  const auto shared_arc_center = find_coincident_endpoint(
      params, "", center_x, center_y);
  const std::string arc_center_vertex_id =
      shared_arc_center.has_value()
          ? std::get<0>(shared_arc_center.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);
  // Use the matched point's exact coordinates for the center.
  const double snapped_arc_center_x =
      shared_arc_center.has_value() ? std::get<1>(shared_arc_center.value()) : center_x;
  const double snapped_arc_center_y =
      shared_arc_center.has_value() ? std::get<2>(shared_arc_center.value()) : center_y;

  feature.sketch_parameters->arcs.push_back(SketchArc{
      .id = "arc-" + std::to_string(arc_index),
      .start_vertex_id = start_vertex_id,
      .end_vertex_id = end_vertex_id,
      .center_vertex_id = arc_center_vertex_id,
      .center_x = snapped_arc_center_x,
      .center_y = snapped_arc_center_y,
      .radius = radius,
      .start_x = snapped_arc_start_x,
      .start_y = snapped_arc_start_y,
      .end_x = snapped_arc_end_x,
      .end_y = snapped_arc_end_y,
      .ccw = ccw,
      .is_construction = is_construction,
  });

  // Projected arcs (auto_dimensions = false) skip the automatic radius
  // dimension — same rationale as add_sketch_line.
  if (!is_construction && auto_dimensions) {
    const auto& arc = feature.sketch_parameters->arcs.back();
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-arc-" + arc.id,
        .kind = "arc_radius",
        .entity_id = arc.id,
        .value = arc.radius,
        .is_auto = true,
    });
  }

  if (refresh_immediately) {
    refresh_sketch_with_append_focus(
        feature, {"arc-" + std::to_string(arc_index)});
  }

  // Arc endpoints start unfixed. The arc's shape is cached at creation;
  // the user can toggle fix on individual endpoints via the constraint
  // tool if they want to lock them.

}

std::optional<std::tuple<double, double>> find_point_position(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  for (const auto& line : parameters.lines) {
    if (line.start_vertex_id == point_id)
      return std::tuple<double, double>{line.start_x, line.start_y};
    if (line.end_vertex_id == point_id)
      return std::tuple<double, double>{line.end_x, line.end_y};
  }
  const auto point_it = std::find_if(
      parameters.vertices.begin(), parameters.vertices.end(),
      [&](const SketchVertex& point) { return point.id == point_id; });
  if (point_it != parameters.vertices.end())
    return std::tuple<double, double>{point_it->x, point_it->y};
  return std::nullopt;
}
