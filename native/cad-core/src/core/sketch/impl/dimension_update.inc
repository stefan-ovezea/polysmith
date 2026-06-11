#include "core/sketch/impl/dimension_angle_update.inc"

void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value,
                             std::optional<std::string> expression) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch dimensions");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch dimensions must be greater than zero");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& dimension = require_dimension(parameters, dimension_id);

  // Driven (reference-only) dimensions show the measured value but do
  // not drive geometry. Silently ignore edit attempts.
  if (dimension.driven) {
    return;
  }

  // Store expression if provided — cleared when expression is nullopt
  // (plain numeric update).
  dimension.expression = expression.value_or("");

  if (dimension.kind == "line_length") {
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == dimension.entity_id; });

    if (line_it == parameters.lines.end()) {
      throw std::runtime_error("Sketch line not found for dimension: " + dimension_id);
    }

    const double previous_start_x = line_it->start_x;
    const double previous_start_y = line_it->start_y;
    const double previous_end_x = line_it->end_x;
    const double previous_end_y = line_it->end_y;

    drive_line_length_respecting_fixed_points(parameters, *line_it, value);
    snap_line_endpoints_to_coincident_geometry(parameters, *line_it);
    if (!points_match(previous_start_x,
                      previous_start_y,
                      line_it->start_x,
                      line_it->start_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->start_point_id,
                                     line_it->start_x,
                                     line_it->start_y);
    }
    if (!points_match(previous_end_x,
                      previous_end_y,
                      line_it->end_x,
                      line_it->end_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->end_point_id,
                                     line_it->end_x,
                                     line_it->end_y);
    }
    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_it->id);
    enforce_perpendicular_relations(parameters, line_it->id);
    enforce_parallel_relations(parameters, line_it->id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_radius") {
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == dimension.entity_id;
        });

    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for dimension: " + dimension_id);
    }

    circle_it->radius = value;
    sync_circle_dimension(parameters, *circle_it);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "angle") {
    fprintf(stderr, "ANGLE-UPDATE: id=%s old=%.4f rad (%.1f°) new=%.4f rad (%.1f°)\n",
            dimension_id.c_str(),
            dimension.value, dimension.value * 180.0 / M_PI,
            value, value * 180.0 / M_PI);
    update_angle_dimension(feature, parameters, dimension, value);
    return;
  }

  if (dimension.kind == "line_angle") {
    // Line-angle dimension: absolute angle from the positive X axis
    // (horizontal). Rotate the line about its START point, preserving
    // the current length.
    auto& line = require_line(parameters, dimension.entity_id);

    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double current_length = std::sqrt(dx * dx + dy * dy);
    if (current_length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Line-angle dimension requires a line with non-zero length");
    }
    // Preserve the sign quadrant: the user enters a magnitude but the
    // line keeps its current orientation.  Mirrors the angle-between-lines
    // variant just above.
    const double current_angle = std::atan2(dy, dx);
    const double target = (current_angle < 0.0) ? -value : value;

    const double new_end_x = line.start_x + std::cos(target) * current_length;
    const double new_end_y = line.start_y + std::sin(target) * current_length;

    const double previous_end_x = line.end_x;
    const double previous_end_y = line.end_y;

    line.end_x = new_end_x;
    line.end_y = new_end_y;

    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    // Propagate the moved endpoint through any coincident points.
    if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    dimension.value = value;
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_center_distance") {
    auto& driven_circle = require_circle(parameters, dimension.entity_id);
    const auto& reference_circle =
        require_circle(parameters, dimension.secondary_entity_id);
    const double dx = driven_circle.center_x - reference_circle.center_x;
    const double dy = driven_circle.center_y - reference_circle.center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Circle-center distance requires distinct circle centers");
    }
    driven_circle.center_x = reference_circle.center_x + (dx / length) * value;
    driven_circle.center_y = reference_circle.center_y + (dy / length) * value;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_line_distance") {
    auto& circle = require_circle(parameters, dimension.entity_id);
    const auto& line = require_line(parameters, dimension.secondary_entity_id);
    const double current_signed = signed_circle_line_distance(circle, line);
    const double direction = current_signed < 0.0 ? -1.0 : 1.0;
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double normal_x = -dy / length;
    const double normal_y = dx / length;
    const double delta = direction * value - current_signed;
    circle.center_x += normal_x * delta;
    circle.center_y += normal_y * delta;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "line_line_distance") {
    auto& driven_line = require_line(parameters, dimension.entity_id);
    const auto& reference_line =
        require_line(parameters, dimension.secondary_entity_id);
    const double current_signed =
        signed_line_line_distance(driven_line, reference_line);
    const double direction = current_signed < 0.0 ? -1.0 : 1.0;
    const double dx = reference_line.end_x - reference_line.start_x;
    const double dy = reference_line.end_y - reference_line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double normal_x = -dy / length;
    const double normal_y = dx / length;
    const double delta = direction * value - current_signed;
    driven_line.start_x += normal_x * delta;
    driven_line.start_y += normal_y * delta;
    driven_line.end_x += normal_x * delta;
    driven_line.end_y += normal_y * delta;
    sync_all_line_dimensions(parameters);
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "polygon_radius") {
    const auto polygon_it = std::find_if(
        parameters.polygons.begin(),
        parameters.polygons.end(),
        [&](const SketchPolygon& polygon) {
          return polygon.id == dimension.entity_id;
        });

    if (polygon_it == parameters.polygons.end()) {
      throw std::runtime_error("Sketch polygon not found for dimension: " + dimension_id);
    }

    polygon_it->radius = value;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  throw std::runtime_error("Unsupported sketch dimension kind: " + dimension.kind);
}
