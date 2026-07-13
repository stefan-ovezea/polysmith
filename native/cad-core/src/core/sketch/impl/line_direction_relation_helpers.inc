void drive_line_perpendicular_to_reference(SketchLine& driven_line,
                                           const SketchLine& reference_line,
                                           const SketchFeatureParameters& parameters) {
  // Skip silently when the driven line still carries an H/V
  // constraint — its angle is already fully determined by the axis
  // constraint, so there is nothing for the relation to drive.
  if (driven_line.constraint.has_value()) {
    return;
  }

  const double reference_dx = reference_line.end_x - reference_line.start_x;
  const double reference_dy = reference_line.end_y - reference_line.start_y;
  const double reference_length =
      std::sqrt(reference_dx * reference_dx + reference_dy * reference_dy);
  const double driven_length = measure_line_length(driven_line);

  if (reference_length <= kMinimumSketchDimensionValue ||
      driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Perpendicular relations require non-zero length lines");
  }

  const double current_dx = driven_line.end_x - driven_line.start_x;
  const double current_dy = driven_line.end_y - driven_line.start_y;
  const double candidate_a_x = -reference_dy / reference_length;
  const double candidate_a_y = reference_dx / reference_length;
  const double candidate_b_x = -candidate_a_x;
  const double candidate_b_y = -candidate_a_y;
  const double dot_a = current_dx * candidate_a_x + current_dy * candidate_a_y;
  const double dot_b = current_dx * candidate_b_x + current_dy * candidate_b_y;
  const double direction_x = dot_a >= dot_b ? candidate_a_x : candidate_b_x;
  const double direction_y = dot_a >= dot_b ? candidate_a_y : candidate_b_y;

  const bool start_fixed = vertex_is_fixed(parameters, driven_line.start_vertex_id);
  const bool end_fixed = vertex_is_fixed(parameters, driven_line.end_vertex_id);
  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a perpendicular relation when both endpoints are fixed");
  }

  if (end_fixed) {
    driven_line.start_x = driven_line.end_x - direction_x * driven_length;
    driven_line.start_y = driven_line.end_y - direction_y * driven_length;
  } else {
    driven_line.end_x = driven_line.start_x + direction_x * driven_length;
    driven_line.end_y = driven_line.start_y + direction_y * driven_length;
  }
  validate_line(
      driven_line.start_x, driven_line.start_y, driven_line.end_x, driven_line.end_y);
}

void drive_line_parallel_to_reference(SketchLine& driven_line,
                                      const SketchLine& reference_line,
                                      const SketchFeatureParameters& parameters) {
  // Skip silently when the driven line still carries an H/V
  // constraint — its angle is already fully determined by the axis
  // constraint, so there is nothing for the relation to drive.
  if (driven_line.constraint.has_value()) {
    return;
  }

  const double reference_dx = reference_line.end_x - reference_line.start_x;
  const double reference_dy = reference_line.end_y - reference_line.start_y;
  const double reference_length =
      std::sqrt(reference_dx * reference_dx + reference_dy * reference_dy);
  const double driven_length = measure_line_length(driven_line);

  if (reference_length <= kMinimumSketchDimensionValue ||
      driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Parallel relations require non-zero length lines");
  }

  const double current_dx = driven_line.end_x - driven_line.start_x;
  const double current_dy = driven_line.end_y - driven_line.start_y;
  const double candidate_a_x = reference_dx / reference_length;
  const double candidate_a_y = reference_dy / reference_length;
  const double candidate_b_x = -candidate_a_x;
  const double candidate_b_y = -candidate_a_y;
  const double dot_a = current_dx * candidate_a_x + current_dy * candidate_a_y;
  const double dot_b = current_dx * candidate_b_x + current_dy * candidate_b_y;
  const double direction_x = dot_a >= dot_b ? candidate_a_x : candidate_b_x;
  const double direction_y = dot_a >= dot_b ? candidate_a_y : candidate_b_y;

  const bool start_fixed = vertex_is_fixed(parameters, driven_line.start_vertex_id);
  const bool end_fixed = vertex_is_fixed(parameters, driven_line.end_vertex_id);
  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a parallel relation when both endpoints are fixed");
  }

  if (end_fixed) {
    driven_line.start_x = driven_line.end_x - direction_x * driven_length;
    driven_line.start_y = driven_line.end_y - direction_y * driven_length;
  } else {
    driven_line.end_x = driven_line.start_x + direction_x * driven_length;
    driven_line.end_y = driven_line.start_y + direction_y * driven_length;
  }
  validate_line(
      driven_line.start_x, driven_line.start_y, driven_line.end_x, driven_line.end_y);
}
