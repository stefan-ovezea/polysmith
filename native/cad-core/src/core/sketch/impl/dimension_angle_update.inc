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
  auto unit_from_pivot = [](double px, double py, double x, double y) {
    double dx = x - px;
    double dy = y - py;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return std::pair<double, double>{1.0, 0.0};
    return std::pair<double, double>{dx / len, dy / len};
  };
  auto choose_host_ray =
      [&](double px, double py,
          double h0x, double h0y,
          double h1x, double h1y,
          double other_dx, double other_dy,
          double target_small_angle) {
    auto d0 = unit_from_pivot(px, py, h0x, h0y);
    auto d1 = unit_from_pivot(px, py, h1x, h1y);
    auto score = [&](const std::pair<double, double>& dir) {
      const double measured = std::abs(std::atan2(
          dir.first * other_dy - dir.second * other_dx,
          dir.first * other_dx + dir.second * other_dy));
      return std::abs(measured - target_small_angle);
    };
    return score(d0) <= score(d1) ? d0 : d1;
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
  // Virtual pivot: stored on dimension for non-intersecting lines.
  // Use the stored pivot for angle computation, but rotate the driven
  // line around its own endpoint (not the virtual pivot).
  bool use_virtual_pivot = dimension.pivot_x.has_value() &&
                           dimension.pivot_y.has_value();
  if (a_pivot_index < 0 && b_pivot_index < 0 && !use_virtual_pivot) {
    throw std::runtime_error(
        "Angle dimension requires the two lines to share an endpoint "
        "or have one endpoint on the other line");
  }

  double pivot_x, pivot_y;
  double a_dx, a_dy, b_dx, b_dy;
  bool drive_a = false;  // true → rotate line A; false → rotate line B
  int driven_pivot_index = 0;
  double delta = 0.0;
  double current_signed_for_value = 0.0;
  constexpr double kPi = 3.14159265358979323846;
  constexpr double kTwoPi = 2.0 * kPi;
  double requested = std::fmod(std::abs(value), kTwoPi);
  if (requested < 0.0) {
    requested += kTwoPi;
  }
  const double target_small_angle =
      requested > kPi ? kTwoPi - requested : requested;

  if (use_virtual_pivot) {
    // Direction vectors from virtual pivot (for angle computation only).
    // The rotation is applied around line B's start point, so we compute
    // the delta from virtual-pivot directions and apply it separately.
    double vpx = *dimension.pivot_x;
    double vpy = *dimension.pivot_y;
    auto vdir = [](double px, double py,
                   double e0x, double e0y,
                   double e1x, double e1y) -> std::pair<double, double> {
      double d0 = std::sqrt((e0x - px)*(e0x - px) + (e0y - py)*(e0y - py));
      double d1 = std::sqrt((e1x - px)*(e1x - px) + (e1y - py)*(e1y - py));
      double tx = d0 > d1 ? e0x : e1x;
      double ty = d0 > d1 ? e0y : e1y;
      double dx = tx - px, dy = ty - py;
      double len = std::sqrt(dx*dx + dy*dy);
      if (len < 1e-12) return {1.0, 0.0};
      return {dx / len, dy / len};
    };
    auto av = vdir(vpx, vpy, a_ends[0].first, a_ends[0].second,
                                a_ends[1].first, a_ends[1].second);
    auto bv = vdir(vpx, vpy, b_ends[0].first, b_ends[0].second,
                                b_ends[1].first, b_ends[1].second);
    // Compute signed angle and delta from virtual-pivot directions.
    current_signed_for_value = std::atan2(
        av.first * bv.second - av.second * bv.first,
        av.first * bv.first + av.second * bv.second);
    auto wrap = [](double angle) {
      constexpr double kLocPi = 3.14159265358979323846;
      constexpr double kLocTwoPi = 2.0 * kLocPi;
      double w = std::fmod(angle, kLocTwoPi);
      if (w <= -kLocPi) w += kLocTwoPi;
      else if (w > kLocPi) w -= kLocTwoPi;
      return w;
    };
    const double v_positive_target = wrap(requested);
    const double v_negative_target = wrap(-requested);
    const double v_positive_delta = wrap(v_positive_target - current_signed_for_value);
    const double v_negative_delta = wrap(v_negative_target - current_signed_for_value);
    delta = std::abs(v_positive_delta) <= std::abs(v_negative_delta)
                ? v_positive_delta : v_negative_delta;

    constexpr double kLocalAngleConfirmTolerance = 1e-4;
    if (std::abs(delta) <= kLocalAngleConfirmTolerance) {
      dimension.value = requested > kPi
                            ? (current_signed_for_value < 0.0 ? -std::abs(value)
                                                       : std::abs(value))
                            : current_signed_for_value;
      return;
    }

    // Rotate line B around the endpoint CLOSER to the virtual pivot
    // so the line "swings" from its near end.  Use the FARTHER endpoint
    // for the direction vector so it points away from the pivot (same
    // sense as the virtual-pivot direction used for delta computation).
    {
      double d0 = std::sqrt((b_ends[0].first - vpx) * (b_ends[0].first - vpx) +
                            (b_ends[0].second - vpy) * (b_ends[0].second - vpy));
      double d1 = std::sqrt((b_ends[1].first - vpx) * (b_ends[1].first - vpx) +
                            (b_ends[1].second - vpy) * (b_ends[1].second - vpy));
      int near_idx = d0 <= d1 ? 0 : 1;
      int far_idx = 1 - near_idx;
      pivot_x = b_ends[near_idx].first;
      pivot_y = b_ends[near_idx].second;
      b_dx = b_ends[far_idx].first - pivot_x;
      b_dy = b_ends[far_idx].second - pivot_y;
      driven_pivot_index = near_idx;
    }
    drive_a = false;
  } else if (b_pivot_index >= 0) {
    // Pivot is an endpoint of line B.  Line B rotates.
    pivot_x = b_ends[b_pivot_index].first;
    pivot_y = b_ends[b_pivot_index].second;
    int b_other = 1 - b_pivot_index;
    b_dx = b_ends[b_other].first - pivot_x;
    b_dy = b_ends[b_other].second - pivot_y;
    const auto a_dir = choose_host_ray(
        pivot_x, pivot_y,
        a_ends[0].first, a_ends[0].second,
        a_ends[1].first, a_ends[1].second,
        b_dx, b_dy,
        target_small_angle);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
    driven_pivot_index = b_pivot_index;
  } else {
    // Pivot is an endpoint of line A.  Line A rotates.
    drive_a = true;
    pivot_x = a_ends[a_pivot_index].first;
    pivot_y = a_ends[a_pivot_index].second;
    int a_other = 1 - a_pivot_index;
    a_dx = a_ends[a_other].first - pivot_x;
    a_dy = a_ends[a_other].second - pivot_y;
    const auto b_dir = choose_host_ray(
        pivot_x, pivot_y,
        b_ends[0].first, b_ends[0].second,
        b_ends[1].first, b_ends[1].second,
        a_dx, a_dy,
        target_small_angle);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
    driven_pivot_index = a_pivot_index;
  }

  if (!use_virtual_pivot) {
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
  auto wrap_signed_angle = [](double angle) {
    constexpr double kLocalPi = 3.14159265358979323846;
    constexpr double kLocalTwoPi = 2.0 * kLocalPi;
    double wrapped = std::fmod(angle, kLocalTwoPi);
    if (wrapped <= -kLocalPi) {
      wrapped += kLocalTwoPi;
    } else if (wrapped > kLocalPi) {
      wrapped -= kLocalTwoPi;
    }
    return wrapped;
  };

  const double positive_target = wrap_signed_angle(requested);
  const double negative_target = wrap_signed_angle(-requested);
  const double positive_delta =
      wrap_signed_angle(positive_target - current_signed);
  const double negative_delta =
      wrap_signed_angle(negative_target - current_signed);
  delta = std::abs(positive_delta) <= std::abs(negative_delta)
              ? positive_delta
              : negative_delta;
  current_signed_for_value = current_signed;

  constexpr double kAngleConfirmTolerance = 1e-4;
  if (std::abs(delta) <= kAngleConfirmTolerance) {
    dimension.value = requested > kPi
                          ? (current_signed < 0.0 ? -std::abs(value)
                                                   : std::abs(value))
                          : current_signed;
    return;
  }
  }  // !use_virtual_pivot

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

  auto final_wrap = [](double angle) {
    constexpr double kFPi = 3.14159265358979323846;
    constexpr double kFTwoPi = 2.0 * kFPi;
    double w = std::fmod(angle, kFTwoPi);
    if (w <= -kFPi) w += kFTwoPi;
    else if (w > kFPi) w -= kFTwoPi;
    return w;
  };
  const double target_signed =
      final_wrap(current_signed_for_value + delta);
  dimension.value = requested > kPi
                        ? (target_signed < 0.0 ? -std::abs(value)
                                               : std::abs(value))
                        : target_signed;
  refresh_sketch_derived_state(feature);
  return;
}
