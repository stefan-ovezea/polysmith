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

void reify_dimension_expressions(
    FeatureEntry& feature,
    const std::vector<struct ParameterEntry>& parameters) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  struct PendingDimensionUpdate {
    std::string id;
    std::string expression;
    double value;
  };
  std::vector<PendingDimensionUpdate> pending_updates;

  for (const auto& dim : feature.sketch_parameters->dimensions) {
    if (dim.expression.empty()) {
      continue;
    }

    const bool dim_is_angle =
        (dim.kind == "angle" || dim.kind == "line_angle");

    // Build a resolver against the current parameter table
    auto resolver = [&parameters,
                      dim_is_angle](const std::string& name) -> double {
      for (const auto& p : parameters) {
        if (p.name == name) {
          if (p.has_error) {
            throw std::runtime_error("Parameter '" + name +
                                     "' has an unresolved expression");
          }
          // Angle-type parameter referenced in a non-angle (length)
          // dimension — the numeric value would be misinterpreted.
          if (p.kind == "angle" && !dim_is_angle) {
            throw std::runtime_error("Angle parameter '" + name +
                "' cannot be used in a length dimension");
          }
          return p.resolved_value;
        }
      }
      throw std::runtime_error("Unknown parameter: '" + name + "'");
    };

    try {
      double resolved = evaluate_formula(dim.expression, resolver);
      if (resolved <= 0.0) {
        // Keep last good value for invalid dimension values
        continue;
      }
      // Angle dimensions store radians, but expressions are authored
      // in degrees (both literal numbers and angle-kind parameter
      // references). Convert here so the dimension's internal value
      // stays in radians while the expression stays human-readable.
      if (dim.kind == "angle" || dim.kind == "line_angle") {
        resolved = resolved * (M_PI / 180.0);
      } else if (dim.kind == "circle_radius") {
        resolved = resolved / 2.0;
      }
      // Preserve the orientation quadrant for line_angle dimensions so
      // a parameter re-eval doesn't flip the line's direction.
      if (dim.kind == "line_angle" && dim.value < 0.0) {
        resolved = -resolved;
      }
      pending_updates.push_back(PendingDimensionUpdate{
          .id = dim.id,
          .expression = dim.expression,
          .value = resolved,
      });
    } catch (const std::exception&) {
      // Silently keep last good value — expression resolution
      // failures shouldn't nuke the dimension's working value.
    }
  }

  for (const auto& update : pending_updates) {
    try {
      update_sketch_dimension(feature,
                              update.id,
                              update.value,
                              update.expression);
    } catch (const std::exception&) {
      // Keep the last valid geometry when a stored expression can no
      // longer be driven, e.g. because a referenced parameter was
      // deleted or the target entity was removed.
    }
  }
}

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
  refresh_sketch_derived_state(feature);
}

void add_sketch_distance_dimension(FeatureEntry& feature,
                                   const std::string& first_entity_id,
                                   const std::string& second_entity_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold distance dimensions");
  }
  if (first_entity_id == second_entity_id) {
    throw std::runtime_error("Distance dimension requires two distinct entities");
  }

  auto& parameters = *feature.sketch_parameters;
  const bool first_is_circle = first_entity_id.rfind("circle-", 0) == 0;
  const bool second_is_circle = second_entity_id.rfind("circle-", 0) == 0;
  const bool first_is_line = first_entity_id.rfind("line-", 0) == 0;
  const bool second_is_line = second_entity_id.rfind("line-", 0) == 0;

  std::string kind;
  std::string driven_entity_id;
  std::string reference_entity_id;
  double value = 0.0;
  if (first_is_circle && second_is_circle) {
    const auto& first_circle = require_circle(parameters, first_entity_id);
    const auto& second_circle = require_circle(parameters, second_entity_id);
    kind = "circle_center_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = distance_between_circles(first_circle, second_circle);
  } else if ((first_is_circle && second_is_line) ||
             (first_is_line && second_is_circle)) {
    const std::string circle_id = first_is_circle ? first_entity_id : second_entity_id;
    const std::string line_id = first_is_line ? first_entity_id : second_entity_id;
    const auto& circle = require_circle(parameters, circle_id);
    const auto& line = require_line(parameters, line_id);
    kind = "circle_line_distance";
    driven_entity_id = circle_id;
    reference_entity_id = line_id;
    value = std::abs(signed_circle_line_distance(circle, line));
  } else if (first_is_line && second_is_line) {
    const auto& first_line = require_line(parameters, first_entity_id);
    const auto& second_line = require_line(parameters, second_entity_id);
    kind = "line_line_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = std::abs(signed_line_line_distance(second_line, first_line));
  } else {
    throw std::runtime_error(
        "Distance dimensions currently support line-line, circle-circle, or circle-line picks");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Distance dimension must be greater than zero");
  }

  const auto duplicate_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        return dim.kind == kind && dim.entity_id == driven_entity_id &&
               dim.secondary_entity_id == reference_entity_id;
      });
  if (duplicate_it != parameters.dimensions.end()) {
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = "dim-" + kind + "-" + driven_entity_id + "-" + reference_entity_id,
      .kind = kind,
      .entity_id = driven_entity_id,
      .secondary_entity_id = reference_entity_id,
      .value = value,
  });
  refresh_sketch_derived_state(feature);
}
