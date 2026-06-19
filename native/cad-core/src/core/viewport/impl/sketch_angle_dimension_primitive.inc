// Build the viewport primitive for an angle dimension between two
// lines that share an endpoint. We emit a chord-shaped "dimension
// line" along the imaginary arc between the two outgoing rays so the
// existing UI dimension renderer can draw something sensible without
// arc support: anchor_start / anchor_end land on each line slightly
// past the pivot, dimension_start / dimension_end form a chord at a
// larger radius, and the label sits beyond that on the bisector.
ViewportSketchDimensionPrimitive make_angle_dimension_primitive(
    const SketchLine& line_a,
    const SketchLine& line_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  // Locate the shared endpoint or intersection point.  First try a
  // direct endpoint match; if that fails, check whether one line's
  // endpoint lies on the other line's segment.
  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  auto point_on_seg = [](double px, double py,
                         double ax, double ay,
                         double bx, double by,
                         double tol) -> bool {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12)
      return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay)) <= tol;
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double proj_x = ax + t * dx, proj_y = ay + t * dy;
    return std::sqrt((px - proj_x) * (px - proj_x) +
                     (py - proj_y) * (py - proj_y)) <= tol;
  };

  auto dir_away = [](double px, double py,
                     double e0x, double e0y,
                     double e1x, double e1y) -> std::pair<double, double> {
    double d0 = std::sqrt((e0x - px) * (e0x - px) + (e0y - py) * (e0y - py));
    double d1 = std::sqrt((e1x - px) * (e1x - px) + (e1y - py) * (e1y - py));
    double tx = d0 > d1 ? e0x : e1x, ty = d0 > d1 ? e0y : e1y;
    double dx = tx - px, dy = ty - py;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return {1.0, 0.0};
    return {dx / len, dy / len};
  };
  auto dir_toward_target_angle =
      [](double px, double py,
         double host0x, double host0y,
         double host1x, double host1y,
         double other_dx, double other_dy,
         double target_angle) -> std::pair<double, double> {
    auto unit = [&](double x, double y) {
      double dx = x - px;
      double dy = y - py;
      double len = std::sqrt(dx * dx + dy * dy);
      if (len < 1e-12) return std::pair<double, double>{1.0, 0.0};
      return std::pair<double, double>{dx / len, dy / len};
    };
    auto d0 = unit(host0x, host0y);
    auto d1 = unit(host1x, host1y);
    auto angle_to_other = [&](const std::pair<double, double>& dir) {
      const double measured = std::abs(std::atan2(
          dir.first * other_dy - dir.second * other_dx,
          dir.first * other_dx + dir.second * other_dy));
      return std::abs(measured - target_angle);
    };
    return angle_to_other(d0) <= angle_to_other(d1) ? d0 : d1;
  };

  constexpr double kPi = 3.14159265358979323846;
  constexpr double kTwoPi = 2.0 * kPi;
  double requested_angle = std::fmod(std::abs(dimension.value), kTwoPi);
  if (requested_angle < 0.0) {
    requested_angle += kTwoPi;
  }
  const double target_angle =
      requested_angle > kPi ? kTwoPi - requested_angle : requested_angle;

  double pivot_x, pivot_y;
  double a_dx, a_dy, b_dx, b_dy;

  // Virtual pivot: stored on dimension for non-intersecting lines.
  if (dimension.pivot_x.has_value() && dimension.pivot_y.has_value()) {
    // Recompute virtual pivot from current line positions so the arc
    // and reference line track geometry after drags / solves.
    double vpx, vpy;
    {
      double a_dx_inf = a_ends[1].first - a_ends[0].first;
      double a_dy_inf = a_ends[1].second - a_ends[0].second;
      double b_dx_inf = b_ends[1].first - b_ends[0].first;
      double b_dy_inf = b_ends[1].second - b_ends[0].second;
      double det = a_dx_inf * b_dy_inf - a_dy_inf * b_dx_inf;
      if (std::abs(det) > 1e-12) {
        double t = ((b_ends[0].first - a_ends[0].first) * b_dy_inf -
                    (b_ends[0].second - a_ends[0].second) * b_dx_inf) / det;
        vpx = a_ends[0].first + t * a_dx_inf;
        vpy = a_ends[0].second + t * a_dy_inf;
      } else {
        vpx = *dimension.pivot_x;
        vpy = *dimension.pivot_y;
      }
    }
    // Pick the endpoint-direction pair whose absolute angle matches
    // the stored dimension value — not just the farther endpoint.
    {
      auto unit = [](double x, double y, double px, double py) {
        double dx = x - px, dy = y - py;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-12) return std::pair<double, double>{1.0, 0.0};
        return std::pair<double, double>{dx / len, dy / len};
      };
      std::pair<double, double> au[2] = {
          unit(a_ends[0].first, a_ends[0].second, vpx, vpy),
          unit(a_ends[1].first, a_ends[1].second, vpx, vpy)};
      std::pair<double, double> bu[2] = {
          unit(b_ends[0].first, b_ends[0].second, vpx, vpy),
          unit(b_ends[1].first, b_ends[1].second, vpx, vpy)};
      auto ad_def = dir_away(vpx, vpy, a_ends[0].first, a_ends[0].second,
                                          a_ends[1].first, a_ends[1].second);
      auto bd_def = dir_away(vpx, vpy, b_ends[0].first, b_ends[0].second,
                                          b_ends[1].first, b_ends[1].second);
      double best_diff = 1e9;
      for (int ai = 0; ai < 2; ++ai) {
        for (int bi = 0; bi < 2; ++bi) {
          double cross = au[ai].first * bu[bi].second - au[ai].second * bu[bi].first;
          double dot   = au[ai].first * bu[bi].first + au[ai].second * bu[bi].second;
          double measured = std::abs(std::atan2(cross, dot));
          double diff = std::abs(measured - target_angle);
          bool is_def =
              (au[ai].first == ad_def.first && au[ai].second == ad_def.second) &&
              (bu[bi].first == bd_def.first && bu[bi].second == bd_def.second);
          if (is_def) diff -= 1e-12;
          if (diff < best_diff) {
            best_diff = diff;
            a_dx = au[ai].first; a_dy = au[ai].second;
            b_dx = bu[bi].first; b_dy = bu[bi].second;
          }
        }
      }
    }
    // Arc center stays at the virtual pivot — like the UI preview.
    pivot_x = vpx;
    pivot_y = vpy;
  } else {
  constexpr double kPivotTol = 0.10;
  int a_pivot = -1, b_pivot = -1;

  // Pass 1: direct endpoint–endpoint match.
  for (int i = 0; i < 2 && a_pivot < 0; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (std::abs(a_ends[i].first - b_ends[j].first) <= kPivotTol &&
          std::abs(a_ends[i].second - b_ends[j].second) <= kPivotTol) {
        a_pivot = i; b_pivot = j; break;
      }
    }
  }
  // Pass 2: endpoint-on-segment.
  if (a_pivot < 0) {
    for (int i = 0; i < 2 && a_pivot < 0; ++i) {
      if (point_on_seg(a_ends[i].first, a_ends[i].second,
                       b_ends[0].first, b_ends[0].second,
                       b_ends[1].first, b_ends[1].second, kPivotTol)) {
        a_pivot = i; b_pivot = -1; break;
      }
    }
  }
  if (a_pivot < 0) {
    for (int j = 0; j < 2 && a_pivot < 0; ++j) {
      if (point_on_seg(b_ends[j].first, b_ends[j].second,
                       a_ends[0].first, a_ends[0].second,
                       a_ends[1].first, a_ends[1].second, kPivotTol)) {
        a_pivot = -1; b_pivot = j; break;
      }
    }
  }

  if (a_pivot >= 0 && b_pivot >= 0) {
    pivot_x = a_ends[a_pivot].first;
    pivot_y = a_ends[a_pivot].second;
    a_dx = a_ends[1 - a_pivot].first - pivot_x;
    a_dy = a_ends[1 - a_pivot].second - pivot_y;
    b_dx = b_ends[1 - b_pivot].first - pivot_x;
    b_dy = b_ends[1 - b_pivot].second - pivot_y;
  } else if (a_pivot >= 0) {
    pivot_x = a_ends[a_pivot].first;
    pivot_y = a_ends[a_pivot].second;
    auto ad = dir_away(pivot_x, pivot_y,
                       a_ends[1 - a_pivot].first, a_ends[1 - a_pivot].second,
                       a_ends[a_pivot].first, a_ends[a_pivot].second);
    a_dx = ad.first; a_dy = ad.second;
    auto bd = dir_toward_target_angle(
        pivot_x, pivot_y,
        b_ends[0].first, b_ends[0].second,
        b_ends[1].first, b_ends[1].second,
        a_dx, a_dy,
        target_angle);
    b_dx = bd.first; b_dy = bd.second;
  } else if (b_pivot >= 0) {
    pivot_x = b_ends[b_pivot].first;
    pivot_y = b_ends[b_pivot].second;
    auto bd = dir_away(pivot_x, pivot_y,
                       b_ends[1 - b_pivot].first, b_ends[1 - b_pivot].second,
                       b_ends[b_pivot].first, b_ends[b_pivot].second);
    b_dx = bd.first; b_dy = bd.second;
    auto ad = dir_toward_target_angle(
        pivot_x, pivot_y,
        a_ends[0].first, a_ends[0].second,
        a_ends[1].first, a_ends[1].second,
        b_dx, b_dy,
        target_angle);
    a_dx = ad.first; a_dy = ad.second;
  } else {
    // Should not happen — add_sketch_angle_dimension validates first.
    pivot_x = a_ends[0].first;  pivot_y = a_ends[0].second;
    a_dx = 1.0; a_dy = 0.0;  b_dx = 0.0; b_dy = 1.0;
  }
  }  // else (not virtual pivot)

  const double a_len = std::sqrt(a_dx * a_dx + a_dy * a_dy);
  const double b_len = std::sqrt(b_dx * b_dx + b_dy * b_dy);
  const double a_ux = a_len > 0.0 ? a_dx / a_len : 1.0;
  const double a_uy = a_len > 0.0 ? a_dy / a_len : 0.0;
  const double b_ux = b_len > 0.0 ? b_dx / b_len : 0.0;
  const double b_uy = b_len > 0.0 ? b_dy / b_len : 1.0;

  // Bisector direction (sum of unit vectors). Normalize defensively
  // because antiparallel rays sum to zero — fall back to A's normal.
  double bx = a_ux + b_ux;
  double by = a_uy + b_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) {
    bx = -a_uy;
    by = a_ux;
    blen = 1.0;
  }
  const double bisector_ux = bx / blen;
  const double bisector_uy = by / blen;

  // For virtual-pivot dimensions, the arc center is at the (potentially
  // distant) intersection point.  Use a radius large enough to reach from
  // the pivot to near the lines so the arc is visible where the lines are.
  double kArcRadius;
  if (dimension.pivot_x.has_value() && dimension.pivot_y.has_value()) {
    // Compute the distance from the virtual pivot to the nearest line
    // endpoint.  The arc should touch the closer of the two lines.
    double min_dist_a = std::min(
        std::sqrt((a_ends[0].first - pivot_x) * (a_ends[0].first - pivot_x) +
                  (a_ends[0].second - pivot_y) * (a_ends[0].second - pivot_y)),
        std::sqrt((a_ends[1].first - pivot_x) * (a_ends[1].first - pivot_x) +
                  (a_ends[1].second - pivot_y) * (a_ends[1].second - pivot_y)));
    double min_dist_b = std::min(
        std::sqrt((b_ends[0].first - pivot_x) * (b_ends[0].first - pivot_x) +
                  (b_ends[0].second - pivot_y) * (b_ends[0].second - pivot_y)),
        std::sqrt((b_ends[1].first - pivot_x) * (b_ends[1].first - pivot_x) +
                  (b_ends[1].second - pivot_y) * (b_ends[1].second - pivot_y)));
    double pivot_to_lines = std::min(min_dist_a, min_dist_b);
    kArcRadius = std::max(12.0, std::min(pivot_to_lines * 0.85, 500.0));
  } else {
    kArcRadius = std::max(8.0, std::min(std::min(a_len, b_len), 500.0));
  }
  constexpr double kAnchorRadius = 4.0;
  const double kLabelRadius = kArcRadius;

  const WorldPoint anchor_start = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);
  const WorldPoint anchor_end = to_world_point(
      parameters,
      pivot_x + b_ux * kAnchorRadius,
      pivot_y + b_uy * kAnchorRadius);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      pivot_x + b_ux * kArcRadius,
      pivot_y + b_uy * kArcRadius);
  const WorldPoint label = to_world_point(
      parameters,
      pivot_x + bisector_ux * kLabelRadius,
      pivot_y + bisector_uy * kLabelRadius);

  // Compute arc sweep: from line A direction to line B direction.
  const double sweep_cross = a_ux * b_uy - a_uy * b_ux;
  const bool arc_ccw = sweep_cross >= 0.0;
  const double arc_start_angle = std::atan2(a_uy, a_ux);
  const double arc_end_angle = std::atan2(b_uy, b_ux);

  // Reference line along line A direction from pivot
  const WorldPoint ref_line_start = to_world_point(parameters, pivot_x, pivot_y);
  const WorldPoint ref_line_end = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);

  // Arc center in world space (the pivot)
  const WorldPoint arc_center_world = to_world_point(parameters, pivot_x, pivot_y);

  // Render the value in degrees (CAD convention) with the same
  // formatter as length / radius so trailing-zero handling stays
  // consistent.
  const double degrees = std::abs(dimension.value) * 180.0 / 3.14159265358979323846;
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "angle",
      .entity_id = line_a.id,
      .label = format_dimension_value(degrees) + "\xc2\xb0",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,

      // Angle arc geometry
      .arc_center_x = arc_center_world.x,
      .arc_center_y = arc_center_world.y,
      .arc_center_z = arc_center_world.z,
      .arc_radius = kArcRadius,
      .arc_start_angle = arc_start_angle,
      .arc_end_angle = arc_end_angle,
      .arc_ccw = arc_ccw,

      // Reference line (along line A direction from pivot)
      .ref_line_start_x = ref_line_start.x,
      .ref_line_start_y = ref_line_start.y,
      .ref_line_start_z = ref_line_start.z,
    .ref_line_end_x = ref_line_end.x,
    .ref_line_end_y = ref_line_end.y,
    .ref_line_end_z = ref_line_end.z,
    .driven = dimension.driven,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double raw_dx = *dimension.label_x - pivot_x;
    const double raw_dy = *dimension.label_y - pivot_y;
    const double dimension_radius =
        std::max(6.0, std::min(std::sqrt(raw_dx * raw_dx + raw_dy * raw_dy), 500.0));
    const double label_radius = dimension_radius;
    const WorldPoint saved_anchor_start = to_world_point(
        parameters,
        pivot_x + a_ux * kAnchorRadius,
        pivot_y + a_uy * kAnchorRadius);
    const WorldPoint saved_anchor_end = to_world_point(
        parameters,
        pivot_x + b_ux * kAnchorRadius,
        pivot_y + b_uy * kAnchorRadius);
    const WorldPoint saved_dimension_start = to_world_point(
        parameters,
        pivot_x + a_ux * dimension_radius,
        pivot_y + a_uy * dimension_radius);
    const WorldPoint saved_dimension_end = to_world_point(
        parameters,
        pivot_x + b_ux * dimension_radius,
        pivot_y + b_uy * dimension_radius);
    const WorldPoint saved_label = to_world_point(
        parameters,
        pivot_x + bisector_ux * label_radius,
        pivot_y + bisector_uy * label_radius);
    primitive.anchor_start_x = saved_anchor_start.x;
    primitive.anchor_start_y = saved_anchor_start.y;
    primitive.anchor_start_z = saved_anchor_start.z;
    primitive.anchor_end_x = saved_anchor_end.x;
    primitive.anchor_end_y = saved_anchor_end.y;
    primitive.anchor_end_z = saved_anchor_end.z;
    primitive.dimension_start_x = saved_dimension_start.x;
    primitive.dimension_start_y = saved_dimension_start.y;
    primitive.dimension_start_z = saved_dimension_start.z;
    primitive.dimension_end_x = saved_dimension_end.x;
    primitive.dimension_end_y = saved_dimension_end.y;
    primitive.dimension_end_z = saved_dimension_end.z;
    primitive.label_x = saved_label.x;
    primitive.label_y = saved_label.y;
    primitive.label_z = saved_label.z;
    primitive.arc_radius = dimension_radius;
    primitive.ref_line_end_x = saved_dimension_start.x;
    primitive.ref_line_end_y = saved_dimension_start.y;
    primitive.ref_line_end_z = saved_dimension_start.z;
  }
  return primitive;
}
