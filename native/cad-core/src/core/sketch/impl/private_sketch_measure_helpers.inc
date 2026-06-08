double measure_line_length(const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  return std::sqrt(dx * dx + dy * dy);
}

bool nearly_equal(double left, double right) {
  return std::abs(left - right) <= kCoincidentTolerance;
}

bool points_match(double left_x,
                  double left_y,
                  double right_x,
                  double right_y) {
  return nearly_equal(left_x, right_x) && nearly_equal(left_y, right_y);
}

double distance_between_circles(const SketchCircle& first,
                                const SketchCircle& second) {
  const double dx = second.center_x - first.center_x;
  const double dy = second.center_y - first.center_y;
  return std::sqrt(dx * dx + dy * dy);
}

double signed_circle_line_distance(const SketchCircle& circle,
                                   const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  if (length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Line distance dimension requires a non-zero line");
  }
  const double normal_x = -dy / length;
  const double normal_y = dx / length;
  return (circle.center_x - line.start_x) * normal_x +
         (circle.center_y - line.start_y) * normal_y;
}

double signed_line_line_distance(const SketchLine& driven_line,
                                 const SketchLine& reference_line) {
  const double ref_dx = reference_line.end_x - reference_line.start_x;
  const double ref_dy = reference_line.end_y - reference_line.start_y;
  const double ref_length = std::sqrt(ref_dx * ref_dx + ref_dy * ref_dy);
  if (ref_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Line distance dimension requires a non-zero line");
  }
  const double driven_dx = driven_line.end_x - driven_line.start_x;
  const double driven_dy = driven_line.end_y - driven_line.start_y;
  const double driven_length =
      std::sqrt(driven_dx * driven_dx + driven_dy * driven_dy);
  if (driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Line distance dimension requires a non-zero line");
  }
  const double cross =
      (ref_dx / ref_length) * (driven_dy / driven_length) -
      (ref_dy / ref_length) * (driven_dx / driven_length);
  if (std::abs(cross) > 1e-3) {
    throw std::runtime_error(
        "Line distance dimension requires parallel sketch lines");
  }
  const double normal_x = -ref_dy / ref_length;
  const double normal_y = ref_dx / ref_length;
  const double midpoint_x = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double midpoint_y = (driven_line.start_y + driven_line.end_y) / 2.0;
  return (midpoint_x - reference_line.start_x) * normal_x +
         (midpoint_y - reference_line.start_y) * normal_y;
}
