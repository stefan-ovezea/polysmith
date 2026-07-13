void add_sketch_angle_dimension(FeatureEntry& feature,
                                const std::string& first_line_id,
                                const std::string& second_line_id,
                                std::optional<double> value) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold angle dimensions");
  }
  if (first_line_id == second_line_id) {
    throw std::runtime_error(
        "Angle dimension requires two distinct lines");
  }
  auto& parameters = *feature.sketch_parameters;
  auto& line_a = require_line(parameters, first_line_id);
  auto& line_b = require_line(parameters, second_line_id);

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
  auto dir_toward_smaller_angle =
      [](double px, double py,
         double host0x, double host0y,
         double host1x, double host1y,
         double other_dx, double other_dy) -> std::pair<double, double> {
    auto unit = [&](double x, double y) {
      double dx = x - px;
      double dy = y - py;
      double len = std::sqrt(dx * dx + dy * dy);
      if (len < 1e-12) return std::pair<double, double>{1.0, 0.0};
      return std::pair<double, double>{dx / len, dy / len};
    };
    // Skip a candidate that sits at the pivot — the fallback (1,0)
    // can win the scoring and turn the inter-line angle into a
    // single-line angle from horizontal.
    double len0 = std::sqrt((host0x - px)*(host0x - px) + (host0y - py)*(host0y - py));
    double len1 = std::sqrt((host1x - px)*(host1x - px) + (host1y - py)*(host1y - py));
    if (len0 < 1e-9) return unit(host1x, host1y);
    if (len1 < 1e-9) return unit(host0x, host0y);
    auto d0 = unit(host0x, host0y);
    auto d1 = unit(host1x, host1y);
    auto angle_to_other = [&](const std::pair<double, double>& dir) {
      return std::abs(std::atan2(
          dir.first * other_dy - dir.second * other_dx,
          dir.first * other_dx + dir.second * other_dy));
    };
    return angle_to_other(d0) <= angle_to_other(d1) ? d0 : d1;
  };

  // --- Parallelism gate: parallel lines that do NOT share a point
  //     ID get a line-line distance dimension, not an angle.  This
  //     matches Fusion 360 behaviour and runs *before* the coordinate-
  //     proximity pivot search so that two unrelated parallel lines
  //     whose endpoints happen to be near each other aren't mistaken
  //     for a shared vertex.
  bool share_point_id = false;
  for (const auto& id_a : {line_a.start_vertex_id, line_a.end_vertex_id}) {
    for (const auto& id_b : {line_b.start_vertex_id, line_b.end_vertex_id}) {
      if (id_a == id_b) { share_point_id = true; break; }
    }
    if (share_point_id) break;
  }
  if (!share_point_id) {
    double adx = line_a.end_x - line_a.start_x;
    double ady = line_a.end_y - line_a.start_y;
    double alen = std::sqrt(adx * adx + ady * ady);
    double bdx = line_b.end_x - line_b.start_x;
    double bdy = line_b.end_y - line_b.start_y;
    double blen = std::sqrt(bdx * bdx + bdy * bdy);
    if (alen > 1e-12 && blen > 1e-12) {
      double cross = (adx / alen) * (bdy / blen) - (ady / alen) * (bdx / blen);
      if (std::abs(cross) < 1e-3) {
        // Parallel, no shared point → line-line distance.
        add_sketch_distance_dimension(feature, first_line_id, second_line_id);
        return;
      }
    }
  }

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
  // 0.10 tolerance — generous enough for real-world snap precision.
  if (a_pivot_index < 0) {
    for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
      if (point_on_segment(a_ends[i].first, a_ends[i].second,
                           b_ends[0].first, b_ends[0].second,
                           b_ends[1].first, b_ends[1].second,
                           0.10)) {
        a_pivot_index = i;
        b_pivot_index = -1;
        break;
      }
    }
  }
  if (a_pivot_index < 0) {
    for (int j = 0; j < 2 && a_pivot_index < 0; ++j) {
      if (point_on_segment(b_ends[j].first, b_ends[j].second,
                           a_ends[0].first, a_ends[0].second,
                           a_ends[1].first, a_ends[1].second,
                           0.10)) {
        a_pivot_index = -1;
        b_pivot_index = j;
        break;
      }
    }
  }
  // Pass 3: infinite-line intersection (virtual pivot).
  // Lines that don't share an endpoint or have an endpoint on the other
  // still form an angle — their extended lines intersect at a virtual
  // pivot used for display and angle computation.  The solver enforces
  // only the direction angle (L2LAngle), so a physical shared point is
  // not required.
  double virtual_pivot_x = 0.0, virtual_pivot_y = 0.0;
  bool use_virtual_pivot = false;
  if (a_pivot_index < 0 && b_pivot_index < 0) {
    double a_dx_inf = a_ends[1].first - a_ends[0].first;
    double a_dy_inf = a_ends[1].second - a_ends[0].second;
    double b_dx_inf = b_ends[1].first - b_ends[0].first;
    double b_dy_inf = b_ends[1].second - b_ends[0].second;
    double det = a_dx_inf * b_dy_inf - a_dy_inf * b_dx_inf;
    if (std::abs(det) < 1e-12) {
      throw std::runtime_error(
          "Dimension requires the two lines to share an endpoint, "
          "have one endpoint on the other line, or be parallel");
    }
    double t = ((b_ends[0].first - a_ends[0].first) * b_dy_inf -
                (b_ends[0].second - a_ends[0].second) * b_dx_inf) / det;
    virtual_pivot_x = a_ends[0].first + t * a_dx_inf;
    virtual_pivot_y = a_ends[0].second + t * a_dy_inf;
    use_virtual_pivot = true;
    a_pivot_index = -2;  // sentinel: virtual pivot from infinite-line intersection
    b_pivot_index = -2;
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
    auto b_dir = dir_toward_smaller_angle(
        pivot_x, pivot_y,
        b_ends[0].first, b_ends[0].second,
        b_ends[1].first, b_ends[1].second,
        a_dx, a_dy);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
  } else if (b_pivot_index >= 0) {
    // B's endpoint lies on A's segment.
    pivot_x = b_ends[b_pivot_index].first;
    pivot_y = b_ends[b_pivot_index].second;
    auto b_dir = dir_away(pivot_x, pivot_y,
                          b_ends[1 - b_pivot_index].first,
                          b_ends[1 - b_pivot_index].second,
                          b_ends[b_pivot_index].first,
                          b_ends[b_pivot_index].second);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
    auto a_dir = dir_toward_smaller_angle(
        pivot_x, pivot_y,
        a_ends[0].first, a_ends[0].second,
        a_ends[1].first, a_ends[1].second,
        b_dx, b_dy);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
  } else {
    // Virtual pivot: direction vectors point away from the infinite-line
    // intersection along each line (to the farther endpoint).
    pivot_x = virtual_pivot_x;
    pivot_y = virtual_pivot_y;
    auto a_dir = dir_away(pivot_x, pivot_y,
                          a_ends[0].first, a_ends[0].second,
                          a_ends[1].first, a_ends[1].second);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
    auto b_dir = dir_away(pivot_x, pivot_y,
                          b_ends[0].first, b_ends[0].second,
                          b_ends[1].first, b_ends[1].second);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
  }
  // Compute the geometric interior angle using the pivot-aware
  // direction vectors (a_dx/a_dy, b_dx/b_dy) already computed above.
  // The planegcs solver will enforce this same value because
  // constraint_solver_dimension_constraints creates properly-oriented
  // temporary lines for the L2LAngle constraint.
  const double signed_angle = std::atan2(
      a_dx * b_dy - a_dy * b_dx, a_dx * b_dx + a_dy * b_dy);
  // Store the SIGNED angle so the planegcs L2LAngle constraint
  // (which enforces a directed angle, not an interior angle)
  // receives the correct target value.  The UI displays the
  // absolute value for human readability.
  const double current_angle = signed_angle;

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
    throw std::runtime_error(
        "Angle dimension already exists for these two lines");
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
  std::optional<double> stored_pivot_x;
  std::optional<double> stored_pivot_y;
  if (use_virtual_pivot) {
    stored_pivot_x = virtual_pivot_x;
    stored_pivot_y = virtual_pivot_y;
  }
  parameters.dimensions.push_back(SketchDimension{
      .id = "dim-angle-" + first + "-" + second,
      .kind = "angle",
      .entity_id = first_line_id,
      .secondary_entity_id = second_line_id,
      .value = value.value_or(current_angle),
      .is_auto = true,
      .pivot_x = stored_pivot_x,
      .pivot_y = stored_pivot_y,
  });

  feature.parameters_summary = make_parameters_summary(parameters);
}
