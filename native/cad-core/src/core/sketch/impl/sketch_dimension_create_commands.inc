// finalize_new_dimension and count_driving_on_point_pair are now in
// dimensions.inc (included earlier in sketch_feature.cpp).

void add_sketch_vertex_distance_dimension(FeatureEntry& feature,
                                         const std::string& point_a_id,
                                         const std::string& point_b_id,
                                         std::optional<std::string> axis) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;

  // Look up by id first, then vertex_id (IPC sends vertex_id which maps to
  // SketchVertex::vertex_id; p.id can differ for legacy / center vertices).
  auto find_vertex = [&](const std::string& vid) {
    auto it = std::find_if(parameters.vertices.begin(), parameters.vertices.end(),
        [&](const SketchVertex& p) { return p.id == vid || p.vertex_id == vid; });
    return it;
  };
  const auto point_a_it = find_vertex(point_a_id);
  const auto point_b_it = find_vertex(point_b_id);

  if (point_a_it == parameters.vertices.end()) {
    throw std::runtime_error("Sketch point not found: " + point_a_id);
  }
  if (point_b_it == parameters.vertices.end()) {
    throw std::runtime_error("Sketch point not found: " + point_b_id);
  }

  double distance;
  if (axis.has_value() && *axis == "x") {
    distance = std::abs(point_b_it->x - point_a_it->x);
  } else if (axis.has_value() && *axis == "y") {
    distance = std::abs(point_b_it->y - point_a_it->y);
  } else {
    const double dx = point_b_it->x - point_a_it->x;
    const double dy = point_b_it->y - point_a_it->y;
    distance = std::sqrt(dx * dx + dy * dy);
  }

  if (distance <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Point distance dimension must be greater than zero");
  }

  const std::string base_dimension_id =
      "dim-point-distance-" + point_a_id + "-" + point_b_id;
  const std::string axis_suffix =
      axis.has_value() && !axis->empty() ? ("-" + *axis) : "";
  const std::string dimension_id = base_dimension_id + axis_suffix;

  // Check for an existing dimension with the SAME axis.  A horizontal
  // and vertical dimension on the same point pair must be independent.
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Already exists with the same axis — just update the value.
    dimension_it->value = distance;
    return;
  }

  // When this is a Euclidean (no axis) distance on two points that are
  // the endpoints of the same line, and a line_length dimension already
  // exists on that line, silently update the existing dimension instead
  // of creating a redundant point_distance.  If no line_length exists,
  // a point_distance is created (which supports drag-to-place in the UI).
  if (!axis.has_value() || axis->empty()) {
    const auto matching_line = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& l) {
          return (l.start_vertex_id == point_a_id && l.end_vertex_id == point_b_id) ||
                 (l.start_vertex_id == point_b_id && l.end_vertex_id == point_a_id);
        });
    if (matching_line != parameters.lines.end()) {
      const auto existing_ll = std::find_if(
          parameters.dimensions.begin(),
          parameters.dimensions.end(),
          [&](const SketchDimension& dim) {
            return dim.kind == "line_length" && dim.entity_id == matching_line->id;
          });
      if (existing_ll != parameters.dimensions.end()) {
        existing_ll->value = distance;
        return;
      }
    }
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "point_distance",
      .entity_id = point_a_id,
      .secondary_entity_id = point_b_id,
      .value = distance,
      .display_as = axis.value_or(""),
  });

  finalize_new_dimension(feature, parameters, dimension_id,
                         point_a_id, point_b_id);
}

void add_sketch_line_length_dimension(FeatureEntry& feature,
                                      const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  require_user_line(parameters, line_id);
  auto& line = require_line(parameters, line_id);

  const std::string dimension_id = "dim-line-" + line.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Convert auto dimension to manual driving dimension (mirrors
    // add_sketch_line_angle_dimension's auto→manual conversion).
    if (dimension_it->is_auto) {
      dimension_it->is_auto = false;
      dimension_it->value = measure_line_length(line);
      finalize_new_dimension(feature, parameters, dimension_id,
                             line.start_vertex_id, line.end_vertex_id);
      return;
    }
    // Already a manual dimension — update value to match current geometry.
    dimension_it->value = measure_line_length(line);
    return;
  }

  // A point_distance (Euclidean, no axis) on the same two endpoints
  // is semantically identical to a line_length.  If one exists, remove
  // it so we don't end up with two dimensions constraining the same
  // point pair.
  const auto dup_pt = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        if (dim.kind != "point_distance") return false;
        if (!dim.display_as.empty()) return false;
        const std::string& pa = dim.entity_id;
        const std::string& pb = dim.secondary_entity_id;
        return (pa == line.start_vertex_id && pb == line.end_vertex_id) ||
               (pa == line.end_vertex_id && pb == line.start_vertex_id);
      });
  if (dup_pt != parameters.dimensions.end()) {
    parameters.dimensions.erase(dup_pt);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "line_length",
      .entity_id = line.id,
      .value = measure_line_length(line),
  });

  finalize_new_dimension(feature, parameters, dimension_id,
                         line.start_vertex_id, line.end_vertex_id);
}

void add_sketch_line_angle_dimension(FeatureEntry& feature,
                                     const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  require_user_line(parameters, line_id);
  auto& line = require_line(parameters, line_id);

  const std::string dimension_id = "dim-line-angle-" + line.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Every non-construction line has an auto line_angle created during
    // line construction (is_auto = true, expression empty).  When the
    // user explicitly requests a line_angle via the dimension tool,
    // convert the existing auto entry to a driving dimension instead
    // of failing.
    if (dimension_it->is_auto) {
      dimension_it->is_auto = false;
      const double dx = line.end_x - line.start_x;
      const double dy = line.end_y - line.start_y;
      dimension_it->value = std::atan2(dy, dx);
      refresh_sketch_derived_state(feature);
      // If over-constrained, fall back to driven.
      if (feature.sketch_parameters->solver_conflicting_count > 0 ||
          feature.sketch_parameters->solver_dofs < 0) {
        dimension_it->driven = true;
        refresh_sketch_derived_state(feature);
      }
      return;
    }
    throw std::runtime_error(
        "Line angle dimension already exists: " + dimension_id);
  }

  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double angle = std::atan2(dy, dx);

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "line_angle",
      .entity_id = line.id,
      .value = angle,
  });

  finalize_new_dimension(feature, parameters, dimension_id);
}

void add_sketch_circle_radius_dimension(FeatureEntry& feature,
                                        const std::string& circle_id,
                                        std::optional<std::string> display_as) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& circle = require_circle(parameters, circle_id);

  const std::string dimension_id = "dim-circle-" + circle.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Already exists — update value to match current geometry (mirrors
    // line_length's idempotent behaviour).
    dimension_it->value = circle.radius;
    dimension_it->is_auto = false;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .value = circle.radius,
      .display_as = display_as.value_or(""),
  });

  finalize_new_dimension(feature, parameters, dimension_id);
}

void add_sketch_arc_radius_dimension(FeatureEntry& feature,
                                     const std::string& arc_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& arc = require_arc(parameters, arc_id);

  const std::string dimension_id = "dim-arc-" + arc.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = arc.radius;
    dimension_it->is_auto = false;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "arc_radius",
      .entity_id = arc.id,
      .value = arc.radius,
  });

  finalize_new_dimension(feature, parameters, dimension_id);
}

void add_sketch_arc_angle_dimension(FeatureEntry& feature,
                                    const std::string& arc_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& arc = require_arc(parameters, arc_id);

  // Absolute sweep from the cached endpoints (sign-independent of ccw).
  const double raw_sweep = std::atan2(arc.end_y - arc.center_y,
                                      arc.end_x - arc.center_x) -
                           std::atan2(arc.start_y - arc.center_y,
                                      arc.start_x - arc.center_x);
  double sweep = raw_sweep;
  if (sweep < 0.0) sweep += 2.0 * 3.14159265358979323846;

  const std::string dimension_id = "dim-arc-angle-" + arc.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = sweep;
    dimension_it->is_auto = false;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "arc_angle",
      .entity_id = arc.id,
      .value = sweep,
  });

  finalize_new_dimension(feature, parameters, dimension_id);
}

// Arc-length dimension: drives the arc's length (radius * sweep)
// via planegcs addConstraintArcLength. The sweep derives from the
// endpoints on every solve.
void add_sketch_arc_length_dimension(FeatureEntry& feature,
                                     const std::string& arc_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& arc = require_arc(parameters, arc_id);

  const double raw_sweep = std::atan2(arc.end_y - arc.center_y,
                                      arc.end_x - arc.center_x) -
                           std::atan2(arc.start_y - arc.center_y,
                                      arc.start_x - arc.center_x);
  double sweep = raw_sweep;
  if (sweep < 0.0) sweep += 2.0 * 3.14159265358979323846;
  const double length = arc.radius * sweep;

  const std::string dimension_id = "dim-arc-length-" + arc.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = length;
    dimension_it->is_auto = false;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "arc_length",
      .entity_id = arc.id,
      .value = length,
  });

  finalize_new_dimension(feature, parameters, dimension_id);
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

  const std::string dimension_id = "dim-polygon-" + polygon_it->id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Already exists — update value to match current geometry (mirrors
    // circle_radius / line_length idempotent behaviour).
    dimension_it->value = polygon_it->radius;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "polygon_radius",
      .entity_id = polygon_it->id,
      .value = polygon_it->radius,
  });

  finalize_new_dimension(feature, parameters, dimension_id);
}
