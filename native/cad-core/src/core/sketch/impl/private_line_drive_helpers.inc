void drive_line_length(SketchLine& line, double value) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double current_length = std::sqrt(dx * dx + dy * dy);

  if (current_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Cannot drive a zero-length sketch line");
  }

  double direction_x = dx / current_length;
  double direction_y = dy / current_length;

  if (line.constraint.has_value()) {
    if (line.constraint.value() == "horizontal") {
      direction_x = dx >= 0.0 ? 1.0 : -1.0;
      direction_y = 0.0;
    } else if (line.constraint.value() == "vertical") {
      direction_x = 0.0;
      direction_y = dy >= 0.0 ? 1.0 : -1.0;
    }
  }

  line.end_x = line.start_x + direction_x * value;
  line.end_y = line.start_y + direction_y * value;
  apply_line_constraint(line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
}

void drive_line_length_from_fixed_end(SketchLine& line, double value) {
  const double dx = line.start_x - line.end_x;
  const double dy = line.start_y - line.end_y;
  const double current_length = std::sqrt(dx * dx + dy * dy);

  if (current_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Cannot drive a zero-length sketch line");
  }

  double direction_x = dx / current_length;
  double direction_y = dy / current_length;

  if (line.constraint.has_value()) {
    if (line.constraint.value() == "horizontal") {
      direction_x = dx >= 0.0 ? 1.0 : -1.0;
      direction_y = 0.0;
    } else if (line.constraint.value() == "vertical") {
      direction_x = 0.0;
      direction_y = dy >= 0.0 ? 1.0 : -1.0;
    }
  }

  line.start_x = line.end_x + direction_x * value;
  line.start_y = line.end_y + direction_y * value;
  if (line.constraint == "horizontal") {
    line.start_y = line.end_y;
  } else if (line.constraint == "vertical") {
    line.start_x = line.end_x;
  }
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
}

void drive_line_length_respecting_fixed_points(
    SketchFeatureParameters& parameters,
    SketchLine& line,
    double value) {
  const bool start_fixed = vertex_is_fixed(parameters, line.start_vertex_id);
  const bool end_fixed = vertex_is_fixed(parameters, line.end_vertex_id);

  if (start_fixed && end_fixed) {
    if (nearly_equal(measure_line_length(line), value)) {
      return;
    }
    throw std::runtime_error(
        "Cannot drive a line length when both endpoints are fixed");
  }

  if (end_fixed) {
    drive_line_length_from_fixed_end(line, value);
    return;
  }

  drive_line_length(line, value);
}
