void add_sketch_point_distance_dimension(FeatureEntry& feature,
                                         const std::string& point_a_id,
                                         const std::string& point_b_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;

  const auto point_a_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& p) { return p.id == point_a_id; });
  const auto point_b_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& p) { return p.id == point_b_id; });

  if (point_a_it == parameters.points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_a_id);
  }
  if (point_b_it == parameters.points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_b_id);
  }

  const double dx = point_b_it->x - point_a_it->x;
  const double dy = point_b_it->y - point_a_it->y;
  const double distance = std::sqrt(dx * dx + dy * dy);

  if (distance <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Point distance dimension must be greater than zero");
  }

  const std::string dimension_id =
      "dim-point-distance-" + point_a_id + "-" + point_b_id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Already exists, just update the value for the current geometry.
    dimension_it->value = distance;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "point_distance",
      .entity_id = point_a_id,
      .secondary_entity_id = point_b_id,
      .value = distance,
      // point_distance is always driven (reference-only) for now.
      // The value is re-computed from the two point coordinates during
      // sync_driven_dimensions. Making it driving requires complex
      // point-movement propagation and is a follow-up.
      .driven = true,
  });

  refresh_sketch_derived_state(feature);
}

void add_sketch_line_length_dimension(FeatureEntry& feature,
                                      const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& line = require_line(parameters, line_id);

  if (line.is_construction) {
    throw std::runtime_error(
        "Cannot create a driving dimension on a construction line");
  }

  const std::string dimension_id = "dim-line-" + line.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    throw std::runtime_error(
        "Line length dimension already exists: " + dimension_id);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "line_length",
      .entity_id = line.id,
      .value = measure_line_length(line),
  });

  refresh_sketch_derived_state(feature);

  // If the solver reports conflicts, over-constrained → make driven.
  if (feature.sketch_parameters->solver_conflicting_count > 0 ||
      feature.sketch_parameters->solver_dofs < 0) {
    auto it = std::find_if(
        parameters.dimensions.begin(), parameters.dimensions.end(),
        [&](SketchDimension& d) { return d.id == dimension_id; });
    if (it != parameters.dimensions.end()) {
      it->driven = true;
      refresh_sketch_derived_state(feature);
    }
  }
}

void add_sketch_circle_radius_dimension(FeatureEntry& feature,
                                        const std::string& circle_id,
                                        std::optional<std::string> display_as) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& circle = require_circle(parameters, circle_id);

  if (circle.is_construction) {
    throw std::runtime_error(
        "Cannot create a driving dimension on a construction circle");
  }

  const std::string dimension_id = "dim-circle-" + circle.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    throw std::runtime_error(
        "Circle radius dimension already exists: " + dimension_id);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .value = circle.radius,
      .display_as = display_as.value_or(""),
  });

  refresh_sketch_derived_state(feature);

  // If the solver reports conflicts, over-constrained → make driven.
  if (feature.sketch_parameters->solver_conflicting_count > 0 ||
      feature.sketch_parameters->solver_dofs < 0) {
    auto it = std::find_if(
        parameters.dimensions.begin(), parameters.dimensions.end(),
        [&](SketchDimension& d) { return d.id == dimension_id; });
    if (it != parameters.dimensions.end()) {
      it->driven = true;
      refresh_sketch_derived_state(feature);
    }
  }
}

void add_sketch_polygon_radius_dimension(FeatureEntry& feature,
                                         const std::string& polygon_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto polygon_it = std::find_if(
      parameters.polygons.begin(),
      parameters.polygons.end(),
      [&](const SketchPolygon& polygon) { return polygon.id == polygon_id; });

  if (polygon_it == parameters.polygons.end()) {
    throw std::runtime_error("Sketch polygon not found: " + polygon_id);
  }

  if (polygon_it->is_construction) {
    throw std::runtime_error(
        "Cannot create a driving dimension on a construction polygon");
  }

  const std::string dimension_id = "dim-polygon-" + polygon_it->id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    throw std::runtime_error(
        "Polygon radius dimension already exists: " + dimension_id);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "polygon_radius",
      .entity_id = polygon_it->id,
      .value = polygon_it->radius,
  });

  refresh_sketch_derived_state(feature);

  // If the solver reports conflicts, over-constrained → make driven.
  if (feature.sketch_parameters->solver_conflicting_count > 0 ||
      feature.sketch_parameters->solver_dofs < 0) {
    auto it = std::find_if(
        parameters.dimensions.begin(), parameters.dimensions.end(),
        [&](SketchDimension& d) { return d.id == dimension_id; });
    if (it != parameters.dimensions.end()) {
      it->driven = true;
      refresh_sketch_derived_state(feature);
    }
  }
}
