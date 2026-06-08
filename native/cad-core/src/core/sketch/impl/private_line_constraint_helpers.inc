void apply_line_constraint(SketchLine& line) {
  if (!line.constraint.has_value()) {
    return;
  }

  if (line.constraint.value() == "horizontal") {
    line.end_y = line.start_y;
    return;
  }

  if (line.constraint.value() == "vertical") {
    line.end_x = line.start_x;
  }
}

void apply_line_constraint_respecting_fixed_points(
    const SketchFeatureParameters& parameters,
    SketchLine& line) {
  if (!line.constraint.has_value()) {
    return;
  }

  const bool start_fixed = point_is_fixed(parameters, line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, line.end_point_id);

  if (line.constraint.value() == "horizontal") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_y, line.end_y)) {
      throw std::runtime_error(
          "Cannot make a line horizontal when both endpoints are fixed");
    }
    // Changing an already-vertical line to horizontal collapses it to
    // zero length (X is already equal, now Y becomes equal too).  Clear
    // the constraint instead — same safeguard as enforce_line_HV_constraints.
    if (nearly_equal(line.start_x, line.end_x)) {
      fprintf(stderr, "WARN cleared horizontal constraint on %s "
              "(would create zero-length line)\n", line.id.c_str());
      line.constraint = std::nullopt;
      return;
    }
    if (end_fixed) {
      line.start_y = line.end_y;
    } else {
      line.end_y = line.start_y;
    }
    return;
  }

  if (line.constraint.value() == "vertical") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_x, line.end_x)) {
      throw std::runtime_error(
          "Cannot make a line vertical when both endpoints are fixed");
    }
    if (nearly_equal(line.start_y, line.end_y)) {
      fprintf(stderr, "WARN cleared vertical constraint on %s "
              "(would create zero-length line)\n", line.id.c_str());
      line.constraint = std::nullopt;
      return;
    }
    if (end_fixed) {
      line.start_x = line.end_x;
    } else {
      line.end_x = line.start_x;
    }
  }
}
