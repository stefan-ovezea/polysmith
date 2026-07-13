void add_sketch_distance_dimension(FeatureEntry& feature,
                                   const std::string& first_entity_id,
                                   const std::string& second_entity_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold distance dimensions");
  }
  if (first_entity_id == second_entity_id) {
    throw std::runtime_error("Distance dimension requires two distinct entities");
  }

  auto& parameters = *feature.sketch_parameters;
  const bool first_is_circle = first_entity_id.rfind("circle-", 0) == 0;
  const bool second_is_circle = second_entity_id.rfind("circle-", 0) == 0;
  const bool first_is_arc = first_entity_id.rfind("arc-", 0) == 0;
  const bool second_is_arc = second_entity_id.rfind("arc-", 0) == 0;
  const bool first_is_curve = first_is_circle || first_is_arc;
  const bool second_is_curve = second_is_circle || second_is_arc;
  const bool first_is_line = first_entity_id.rfind("line-", 0) == 0;
  const bool second_is_line = second_entity_id.rfind("line-", 0) == 0;

  std::string kind;
  std::string driven_entity_id;
  std::string reference_entity_id;
  double value = 0.0;
  if (first_is_curve && second_is_curve) {
    // Curve-to-curve: use center-to-center distance.  Arc centers are
    // stored as cached doubles; circle centers are the circle's
    // center_x/y fields.  signed_circle_line_distance works on the
    // center for arcs since the distance-from-line formula is the same.
    double cx1, cy1, cx2, cy2;
    if (first_is_circle) {
      const auto& c = require_circle(parameters, first_entity_id);
      cx1 = c.center_x; cy1 = c.center_y;
    } else {
      const auto& a = require_arc(parameters, first_entity_id);
      cx1 = a.center_x; cy1 = a.center_y;
    }
    if (second_is_circle) {
      const auto& c = require_circle(parameters, second_entity_id);
      cx2 = c.center_x; cy2 = c.center_y;
    } else {
      const auto& a = require_arc(parameters, second_entity_id);
      cx2 = a.center_x; cy2 = a.center_y;
    }
    kind = "circle_center_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    const double dx = cx2 - cx1, dy = cy2 - cy1;
    value = std::sqrt(dx * dx + dy * dy);
  } else if ((first_is_curve && second_is_line) ||
             (first_is_line && second_is_curve)) {
    const std::string curve_id = first_is_curve ? first_entity_id : second_entity_id;
    const std::string line_id = first_is_line ? first_entity_id : second_entity_id;
    const auto& line = require_line(parameters, line_id);
    double cx, cy;
    if (curve_id.rfind("circle-", 0) == 0) {
      const auto& c = require_circle(parameters, curve_id);
      cx = c.center_x; cy = c.center_y;
    } else {
      const auto& a = require_arc(parameters, curve_id);
      cx = a.center_x; cy = a.center_y;
    }
    kind = "circle_line_distance";
    driven_entity_id = curve_id;
    reference_entity_id = line_id;
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double len = std::sqrt(dx * dx + dy * dy);
    value = std::abs(-dy * (cx - line.start_x) + dx * (cy - line.start_y)) / len;
  } else if (first_is_line && second_is_line) {
    const auto& first_line = require_line(parameters, first_entity_id);
    const auto& second_line = require_line(parameters, second_entity_id);
    kind = "line_line_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = std::abs(signed_line_line_distance(second_line, first_line));
  } else {
    throw std::runtime_error(
        "Distance dimensions currently support line-line, circle-circle, or circle-line picks");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Distance dimension must be greater than zero");
  }

  const auto duplicate_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        return dim.kind == kind && dim.entity_id == driven_entity_id &&
               dim.secondary_entity_id == reference_entity_id;
      });
  if (duplicate_it != parameters.dimensions.end()) {
    return;
  }

  const std::string dim_id =
      "dim-" + kind + "-" + driven_entity_id + "-" + reference_entity_id;
  parameters.dimensions.push_back(SketchDimension{
      .id = dim_id,
      .kind = kind,
      .entity_id = driven_entity_id,
      .secondary_entity_id = reference_entity_id,
      .value = value,
  });
  // Point-pair fallback for types that constrain identifiable point pairs.
  std::optional<std::string> pair_a, pair_b;
  if (kind == "line_line_distance") {
    const auto& first_line = require_line(parameters, first_entity_id);
    const auto& second_line = require_line(parameters, second_entity_id);
    pair_a = first_line.start_vertex_id;
    pair_b = second_line.start_vertex_id;
  } else if (kind == "circle_center_distance") {
    pair_a = circle_center_vertex_id_for(parameters, first_entity_id);
    pair_b = circle_center_vertex_id_for(parameters, second_entity_id);
  }
  finalize_new_dimension(feature, parameters, dim_id, pair_a, pair_b);
}
