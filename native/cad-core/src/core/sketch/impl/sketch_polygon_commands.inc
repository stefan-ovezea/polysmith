void add_sketch_polygon(FeatureEntry& feature,
                        int polygon_index,
                        int sides,
                        const std::string& mode,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y,
                        bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch polygons");
  }
  if (sides < 3) {
    throw std::runtime_error("Polygon must have at least 3 sides");
  }

  double center_x = start_x;
  double center_y = start_y;
  double radius = std::hypot(end_x - start_x, end_y - start_y);

  if (mode == "edge") {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double edge_len = std::hypot(dx, dy);
    if (edge_len <= kMinimumSketchDimensionValue) {
      throw std::runtime_error("Polygon edge must have non-zero length");
    }
    double mid_x = (start_x + end_x) / 2.0;
    double mid_y = (start_y + end_y) / 2.0;
    double nx = -dy / edge_len;
    double ny = dx / edge_len;
    radius = edge_len / (2.0 * std::sin(M_PI / sides));
    center_x = mid_x + nx * radius * std::cos(M_PI / sides);
    center_y = mid_y + ny * radius * std::cos(M_PI / sides);
  }

  // Decompose into N individual lines so the polygon participates in
  // snap, constraints, drag, and profile detection — same approach as
  // add_sketch_rectangle.
  if (!feature.sketch_parameters.has_value()) return;
  auto& parameters = *feature.sketch_parameters;

  // Compute vertices in CCW order starting from angle 0 (right).
  std::vector<std::pair<double, double>> vertices(sides);
  const double angle_step = 2.0 * M_PI / sides;
  for (int i = 0; i < sides; ++i) {
    const double a = angle_step * i - M_PI / 2.0;  // start from top
    vertices[i] = {center_x + radius * std::cos(a),
                   center_y + radius * std::sin(a)};
  }

  // Create lines. Pass false for snap_start so polygon vertices aren't
  // merged with nearby geometry (same as rectangle).
  std::vector<std::string> line_ids(sides);
  for (int i = 0; i < sides; ++i) {
    const int next = (i + 1) % sides;
    const int line_idx = polygon_index * 100 + i;  // unique index
    add_sketch_line(feature, line_idx,
                    vertices[i].first, vertices[i].second,
                    vertices[next].first, vertices[next].second,
                    is_construction, /*snap_start=*/false);
    line_ids[i] = "line-" + std::to_string(line_idx);
  }

  // Remove the auto line-length and line-angle dimensions that
  // add_sketch_line created — polygon lines use the polygon radius
  // dimension instead. Also clear any H/V constraints so the
  // polygon can be freely rotated.
  for (const auto& lid : line_ids) {
    auto& line = require_line(parameters, lid);
    line.constraint = std::nullopt;
  }
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(), parameters.dimensions.end(),
                     [&](const SketchDimension& d) {
                       return (d.kind == "line_length" || d.kind == "line_angle") &&
                              std::find(line_ids.begin(), line_ids.end(), d.entity_id) != line_ids.end();
                     }),
      parameters.dimensions.end());

  // Add equal-length constraints so all sides stay the same length.
  for (int i = 1; i < sides; ++i) {
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-equal-length-poly-" + std::to_string(polygon_index) +
              "-" + std::to_string(i),
        .kind = "equal_length",
        .first_line_id = line_ids[0],
        .second_line_id = line_ids[i],
    });
  }

  // Store the polygon entry so the UI can still select/identify it.
  parameters.polygons.push_back(SketchPolygon{
      .id = "polygon-" + std::to_string(polygon_index),
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
      .sides = sides,
      .mode = mode,
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .is_construction = is_construction,
  });
  if (!is_construction) {
    parameters.dimensions.push_back(SketchDimension{
        .id = "dim-polygon-" + std::to_string(polygon_index),
        .kind = "polygon_radius",
        .entity_id = "polygon-" + std::to_string(polygon_index),
        .value = radius,
        .is_auto = true,
    });
  }

  refresh_sketch_derived_state(feature);
}
