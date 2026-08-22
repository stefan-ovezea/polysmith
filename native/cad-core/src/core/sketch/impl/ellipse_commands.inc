// Ellipse entity (feature/sketch).  v1: center + major-axis point +
// minor-axis point creation.  The two axis points are flagged fixed at
// creation so the cached shape cannot drift — the solver has no
// ellipse registration yet (FreeCAD's ellipse solving is historically
// fragile; parametric ellipses are a follow-up).

void add_sketch_ellipse(FeatureEntry& feature,
                        int ellipse_index,
                        double center_x,
                        double center_y,
                        double axis_a_x,
                        double axis_a_y,
                        double axis_b_x,
                        double axis_b_y,
                        bool is_construction,
                        bool refresh_immediately) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch ellipses");
  }

  auto& params = *feature.sketch_parameters;

  const double a = std::hypot(axis_a_x - center_x, axis_a_y - center_y);
  if (a <= 0.001) {
    throw std::runtime_error("Sketch ellipses must have a non-zero major radius");
  }
  const double rotation = std::atan2(axis_a_y - center_y, axis_a_x - center_x);
  // Minor radius = perpendicular distance from the minor-axis click to
  // the major axis (standard center+axes creation).
  const double b = std::abs(-std::sin(rotation) * (axis_b_x - center_x) +
                            std::cos(rotation) * (axis_b_y - center_y));
  if (b <= 0.001) {
    throw std::runtime_error("Sketch ellipses must have a non-zero minor radius");
  }

  // Vertex unification, mirroring circle centers / line endpoints.
  const auto shared_center = find_coincident_endpoint(
      params, "", center_x, center_y);
  const std::string center_vertex_id =
      shared_center.has_value()
          ? std::get<0>(shared_center.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);
  const double snapped_center_x =
      shared_center.has_value() ? std::get<1>(shared_center.value()) : center_x;
  const double snapped_center_y =
      shared_center.has_value() ? std::get<2>(shared_center.value()) : center_y;

  const double a_x = snapped_center_x + a * std::cos(rotation);
  const double a_y = snapped_center_y + a * std::sin(rotation);
  const double b_x = snapped_center_x + b * std::cos(rotation + 1.5707963267948966);
  const double b_y = snapped_center_y + b * std::sin(rotation + 1.5707963267948966);

  const auto shared_a = find_coincident_endpoint(params, "", a_x, a_y);
  const std::string axis_a_vertex_id =
      shared_a.has_value()
          ? std::get<0>(shared_a.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);
  const auto shared_b = find_coincident_endpoint(params, "", b_x, b_y);
  const std::string axis_b_vertex_id =
      shared_b.has_value()
          ? std::get<0>(shared_b.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);

  params.ellipses.push_back(SketchEllipse{
      .id = "ellipse-" + std::to_string(ellipse_index),
      .center_vertex_id = center_vertex_id,
      .axis_a_vertex_id = axis_a_vertex_id,
      .axis_b_vertex_id = axis_b_vertex_id,
      .center_x = snapped_center_x,
      .center_y = snapped_center_y,
      .a = a,
      .b = b,
      .rotation = rotation,
      .is_construction = is_construction,
  });

  // Axis points are fixed at creation — the cached shape is frozen
  // until the solver learns ellipses.
  for (auto& vertex : params.vertices) {
    if (vertex.id == axis_a_vertex_id || vertex.id == axis_b_vertex_id) {
      vertex.is_fixed = true;
    }
  }

  if (refresh_immediately) {
    refresh_sketch_with_append_focus(
        feature, {"ellipse-" + std::to_string(ellipse_index)});
  }
}

// Translates an ellipse rigidly (move tool path): shifts the cached
// shape and all three vertex ids by (dx, dy).  Axis points stay fixed
// (their absolute positions move with the entity).
void translate_sketch_ellipse(SketchEllipse& ellipse, double dx, double dy) {
  ellipse.center_x += dx;
  ellipse.center_y += dy;
}

void update_sketch_ellipse(FeatureEntry& feature,
                           const std::string& ellipse_id,
                           double center_x,
                           double center_y,
                           double a,
                           double b,
                           double rotation) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch ellipses");
  }
  auto& params = *feature.sketch_parameters;
  const auto it = std::find_if(
      params.ellipses.begin(), params.ellipses.end(),
      [&](const SketchEllipse& e) { return e.id == ellipse_id; });
  if (it == params.ellipses.end()) {
    throw std::runtime_error("Sketch ellipse not found: " + ellipse_id);
  }
  if (a <= 0.001 || b <= 0.001) {
    throw std::runtime_error("Sketch ellipses must have non-zero radii");
  }
  it->center_x = center_x;
  it->center_y = center_y;
  it->a = a;
  it->b = b;
  it->rotation = rotation;
  // Axis points follow the new shape (still fixed).
  for (auto& vertex : params.vertices) {
    if (vertex.id == it->axis_a_vertex_id) {
      vertex.x = center_x + a * std::cos(rotation);
      vertex.y = center_y + a * std::sin(rotation);
    }
    if (vertex.id == it->axis_b_vertex_id) {
      vertex.x = center_x + b * std::cos(rotation + 1.5707963267948966);
      vertex.y = center_y + b * std::sin(rotation + 1.5707963267948966);
    }
    if (vertex.id == it->center_vertex_id) {
      vertex.x = center_x;
      vertex.y = center_y;
    }
  }
  refresh_sketch_derived_state(feature);
}
