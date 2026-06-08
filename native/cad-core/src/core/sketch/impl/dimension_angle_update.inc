void update_angle_dimension(FeatureEntry& feature,
                            SketchFeatureParameters& parameters,
                            SketchDimension& dimension,
                            double value) {
  // Angle dimensions take a target angle in radians. We rotate the
  // SECOND line (`secondary_entity_id`) about the endpoint shared
  // with the FIRST line (`entity_id`) until the angle between
  // their outgoing direction vectors matches the target. Requires
  // the two lines to share an endpoint within
  // `kCoincidentTolerance`.
  auto& line_a = require_line(parameters, dimension.entity_id);
  auto& line_b = require_line(parameters, dimension.secondary_entity_id);

  // Locate the shared endpoint or intersection point.  First try
  // endpoint–endpoint, then endpoint-on-segment.
  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  auto pt_on_seg = [](double px, double py, double ax, double ay,
                      double bx, double by, double tol) -> bool {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12)
      return std::sqrt((px-ax)*(px-ax) + (py-ay)*(py-ay)) <= tol;
    double t = ((px-ax)*dx + (py-ay)*dy) / len2;
    if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
    double proj_x = ax + t*dx, proj_y = ay + t*dy;
    return std::sqrt((px-proj_x)*(px-proj_x) + (py-proj_y)*(py-proj_y)) <= tol;
  };

  int a_pivot_index = -1;
  int b_pivot_index = -1;
  for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (std::abs(a_ends[i].first - b_ends[j].first) <=
              kCoincidentTolerance &&
          std::abs(a_ends[i].second - b_ends[j].second) <=
              kCoincidentTolerance) {
        a_pivot_index = i; b_pivot_index = j; break;
      }
    }
  }
  if (a_pivot_index < 0) {
    for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
      if (pt_on_seg(a_ends[i].first, a_ends[i].second,
                    b_ends[0].first, b_ends[0].second,
                    b_ends[1].first, b_ends[1].second,
                    kCoincidentTolerance)) {
        a_pivot_index = i; b_pivot_index = -1; break;
      }
    }
  }
  if (a_pivot_index < 0) {
    for (int j = 0; j < 2 && a_pivot_index < 0; ++j) {
      if (pt_on_seg(b_ends[j].first, b_ends[j].second,
                    a_ends[0].first, a_ends[0].second,
                    a_ends[1].first, a_ends[1].second,
                    kCoincidentTolerance)) {
        a_pivot_index = -1; b_pivot_index = j; break;
      }
    }
  }
  if (a_pivot_index < 0 && b_pivot_index < 0) {
    throw std::runtime_error(
        "Angle dimension requires the two lines to share an endpoint "
        "or have one endpoint on the other line");
  }

  double pivot_x, pivot_y;
  double a_dx, a_dy, b_dx, b_dy;
  bool drive_a = false;  // true → rotate line A; false → rotate line B
  int driven_pivot_index = 0;

  if (b_pivot_index >= 0) {
    // Pivot is an endpoint of line B.  Line B rotates.
    pivot_x = b_ends[b_pivot_index].first;
    pivot_y = b_ends[b_pivot_index].second;
    a_dx = a_ends[0].first - pivot_x; a_dy = a_ends[0].second - pivot_y;
    double a_len0 = std::sqrt(a_dx*a_dx + a_dy*a_dy);
    double a_dx1 = a_ends[1].first - pivot_x, a_dy1 = a_ends[1].second - pivot_y;
    double a_len1 = std::sqrt(a_dx1*a_dx1 + a_dy1*a_dy1);
    if (a_len1 > a_len0) { a_dx = a_dx1; a_dy = a_dy1; }
    int b_other = 1 - b_pivot_index;
    b_dx = b_ends[b_other].first - pivot_x;
    b_dy = b_ends[b_other].second - pivot_y;
    driven_pivot_index = b_pivot_index;
  } else {
    // Pivot is an endpoint of line A.  Line A rotates.
    drive_a = true;
    pivot_x = a_ends[a_pivot_index].first;
    pivot_y = a_ends[a_pivot_index].second;
    int a_other = 1 - a_pivot_index;
    a_dx = a_ends[a_other].first - pivot_x;
    a_dy = a_ends[a_other].second - pivot_y;
    b_dx = b_ends[0].first - pivot_x; b_dy = b_ends[0].second - pivot_y;
    double b_len0 = std::sqrt(b_dx*b_dx + b_dy*b_dy);
    double b_dx1 = b_ends[1].first - pivot_x, b_dy1 = b_ends[1].second - pivot_y;
    double b_len1 = std::sqrt(b_dx1*b_dx1 + b_dy1*b_dy1);
    if (b_len1 > b_len0) { b_dx = b_dx1; b_dy = b_dy1; }
    driven_pivot_index = a_pivot_index;
  }

  const double a_length = std::sqrt(a_dx * a_dx + a_dy * a_dy);
  const double b_length = std::sqrt(b_dx * b_dx + b_dy * b_dy);
  if (a_length <= kMinimumSketchDimensionValue ||
      b_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Angle dimension requires both lines to have non-zero length");
  }

  // Current signed angle from A's direction to B's direction.
  const double current_signed = std::atan2(
      a_dx * b_dy - a_dy * b_dx, a_dx * b_dx + a_dy * b_dy);
  const double target_signed = current_signed >= 0.0 ? value : -value;
  const double delta = target_signed - current_signed;
  const double cos_delta = std::cos(delta);
  const double sin_delta = std::sin(delta);

  double new_other_x, new_other_y;
  if (drive_a) {
    // Rotate line A about the pivot.  Rotating A by +delta changes
    // the relative angle (B − A) by −delta, so we negate.
    const double rot_cos = std::cos(-delta);
    const double rot_sin = std::sin(-delta);
    const double new_a_dx = a_dx * rot_cos - a_dy * rot_sin;
    const double new_a_dy = a_dx * rot_sin + a_dy * rot_cos;
    new_other_x = pivot_x + new_a_dx;
    new_other_y = pivot_y + new_a_dy;
  } else {
    const double new_b_dx = b_dx * cos_delta - b_dy * sin_delta;
    const double new_b_dy = b_dx * sin_delta + b_dy * cos_delta;
    new_other_x = pivot_x + new_b_dx;
    new_other_y = pivot_y + new_b_dy;
  }

  // Mutate the driven line in-place.
  auto& driven_line = drive_a ? line_a : line_b;
  if (driven_pivot_index == 0) {
    driven_line.end_x = new_other_x;
    driven_line.end_y = new_other_y;
  } else {
    driven_line.start_x = new_other_x;
    driven_line.start_y = new_other_y;
  }

  snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
  validate_line(driven_line.start_x, driven_line.start_y,
                driven_line.end_x, driven_line.end_y);

  const std::string moved_point_id =
      driven_pivot_index == 0 ? driven_line.end_point_id
                              : driven_line.start_point_id;
  propagate_connected_point_move(
      parameters, moved_point_id, new_other_x, new_other_y);

  dimension.value = value;
  refresh_sketch_derived_state(feature);
  return;
}
