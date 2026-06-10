void add_sketch_angle_dimension(FeatureEntry& feature,
                                const std::string& first_line_id,
                                const std::string& second_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold angle dimensions");
  }
  if (first_line_id == second_line_id) {
    throw std::runtime_error(
        "Angle dimension requires two distinct lines");
  }
  auto& parameters = *feature.sketch_parameters;
  const auto& line_a = require_line(parameters, first_line_id);
  const auto& line_b = require_line(parameters, second_line_id);

  // Compute the current angle (unsigned) between outgoing directions
  // from the shared endpoint or intersection point.  First try a
  // direct endpoint match; if that fails, check whether one line's
  // endpoint lies on the other line's segment (the lines don't need
  // to share an endpoint — an endpoint-on-segment is enough).
  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  // Helper: closest point on segment AB to point P, and the distance.
  auto point_on_segment = [](double px, double py,
                             double ax, double ay,
                             double bx, double by,
                             double tolerance) -> bool {
    double dx = bx - ax;
    double dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) {
      return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay)) <=
             tolerance;
    }
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double proj_x = ax + t * dx;
    double proj_y = ay + t * dy;
    return std::sqrt((px - proj_x) * (px - proj_x) +
                     (py - proj_y) * (py - proj_y)) <= tolerance;
  };

  // Helper: direction away from pivot along line (to the farther endpoint).
  auto dir_away = [](double px, double py,
                     double e0x, double e0y,
                     double e1x, double e1y) -> std::pair<double, double> {
    double d0 = std::sqrt((e0x - px) * (e0x - px) + (e0y - py) * (e0y - py));
    double d1 = std::sqrt((e1x - px) * (e1x - px) + (e1y - py) * (e1y - py));
    double tx = d0 > d1 ? e0x : e1x;
    double ty = d0 > d1 ? e0y : e1y;
    double dx = tx - px;
    double dy = ty - py;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return {1.0, 0.0};
    return {dx / len, dy / len};
  };

  int a_pivot_index = -1;
  int b_pivot_index = -1;
  // Pass 1: direct endpoint–endpoint match (existing behaviour).
  for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (std::abs(a_ends[i].first - b_ends[j].first) <=
              kCoincidentTolerance &&
          std::abs(a_ends[i].second - b_ends[j].second) <=
              kCoincidentTolerance) {
        a_pivot_index = i;
        b_pivot_index = j;
        break;
      }
    }
  }
  // Pass 2: one line's endpoint lies on the other line's segment.
  if (a_pivot_index < 0) {
    for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
      if (point_on_segment(a_ends[i].first, a_ends[i].second,
                           b_ends[0].first, b_ends[0].second,
                           b_ends[1].first, b_ends[1].second,
                           kCoincidentTolerance)) {
        a_pivot_index = i;
        b_pivot_index = -1; // pivot is an A-endpoint, on B's segment
        break;
      }
    }
  }
  if (a_pivot_index < 0) {
    for (int j = 0; j < 2 && a_pivot_index < 0; ++j) {
      if (point_on_segment(b_ends[j].first, b_ends[j].second,
                           a_ends[0].first, a_ends[0].second,
                           a_ends[1].first, a_ends[1].second,
                           kCoincidentTolerance)) {
        a_pivot_index = -1; // pivot is a B-endpoint, on A's segment
        b_pivot_index = j;
        break;
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

  if (a_pivot_index >= 0 && b_pivot_index >= 0) {
    // Shared endpoint case (existing behaviour).
    pivot_x = a_ends[a_pivot_index].first;
    pivot_y = a_ends[a_pivot_index].second;
    a_dx = a_ends[1 - a_pivot_index].first - pivot_x;
    a_dy = a_ends[1 - a_pivot_index].second - pivot_y;
    b_dx = b_ends[1 - b_pivot_index].first - pivot_x;
    b_dy = b_ends[1 - b_pivot_index].second - pivot_y;
  } else if (a_pivot_index >= 0) {
    // A's endpoint lies on B's segment.
    pivot_x = a_ends[a_pivot_index].first;
    pivot_y = a_ends[a_pivot_index].second;
    auto a_dir = dir_away(pivot_x, pivot_y,
                          a_ends[1 - a_pivot_index].first,
                          a_ends[1 - a_pivot_index].second,
                          a_ends[a_pivot_index].first,
                          a_ends[a_pivot_index].second);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
    auto b_dir = dir_away(pivot_x, pivot_y,
                          b_ends[0].first, b_ends[0].second,
                          b_ends[1].first, b_ends[1].second);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
  } else {
    // B's endpoint lies on A's segment.
    pivot_x = b_ends[b_pivot_index].first;
    pivot_y = b_ends[b_pivot_index].second;
    auto a_dir = dir_away(pivot_x, pivot_y,
                          a_ends[0].first, a_ends[0].second,
                          a_ends[1].first, a_ends[1].second);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
    auto b_dir = dir_away(pivot_x, pivot_y,
                          b_ends[1 - b_pivot_index].first,
                          b_ends[1 - b_pivot_index].second,
                          b_ends[b_pivot_index].first,
                          b_ends[b_pivot_index].second);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
  }
  const double signed_angle = std::atan2(
      a_dx * b_dy - a_dy * b_dx, a_dx * b_dx + a_dy * b_dy);
  const double current_angle = std::abs(signed_angle);

  // Refuse to create a duplicate angle dimension on the same pair so
  // the dimension list doesn't accumulate stale entries when the
  // user clicks the same two lines repeatedly.
  const auto duplicate_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        return dim.kind == "angle" &&
               ((dim.entity_id == first_line_id &&
                 dim.secondary_entity_id == second_line_id) ||
                (dim.entity_id == second_line_id &&
                 dim.secondary_entity_id == first_line_id));
      });
  if (duplicate_it != parameters.dimensions.end()) {
    return;
  }

  // Stable id derived from the line ids (sorted) so the same pair
  // round-trips through serialization without churning, and the
  // duplicate-detection above doesn't depend on insertion order.
  const std::string& first = first_line_id < second_line_id
                                 ? first_line_id
                                 : second_line_id;
  const std::string& second = first_line_id < second_line_id
                                  ? second_line_id
                                  : first_line_id;
  parameters.dimensions.push_back(SketchDimension{
      .id = "dim-angle-" + first + "-" + second,
      .kind = "angle",
      .entity_id = first_line_id,
      .secondary_entity_id = second_line_id,
      .value = current_angle,
  });

  // When creating an angle dimension, both lines need driving length
  // constraints so the solver preserves their current lengths and the
  // angle solve doesn't deform the shape. The TS dimension-tool flow
  // may have already deleted the first line's auto-dimension, so we
  // either convert an existing auto-dimension or create one.
  for (const auto& line_id : {first_line_id, second_line_id}) {
    const auto& line = line_id == first_line_id ? line_a : line_b;
    const std::string len_dim_id = "dim-line-" + line_id;
    auto len_it = std::find_if(
        parameters.dimensions.begin(),
        parameters.dimensions.end(),
        [&](SketchDimension& dim) {
          return dim.id == len_dim_id && dim.kind == "line_length";
        });
    if (len_it != parameters.dimensions.end()) {
      // Existing auto-dimension: make it driving.
      len_it->is_auto = false;
    } else {
      // Dimension was deleted (e.g. by the TS auto-cleanup flow).
      // Re-create it as a manual (driving) dimension locked to the
      // current measured length.
      parameters.dimensions.push_back(SketchDimension{
          .id = len_dim_id,
          .kind = "line_length",
          .entity_id = line_id,
          .value = measure_line_length(line),
          .is_auto = false,
      });
    }
  }

  refresh_sketch_derived_state(feature);
}
