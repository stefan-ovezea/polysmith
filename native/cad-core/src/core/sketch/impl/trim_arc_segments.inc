// ================================================================
// split_arc_at_intersections
// ================================================================

std::vector<TrimSegment>
split_arc_at_intersections(const SketchArc &arc,
                           const std::vector<TrimIntersection> &intersections) {
  if (intersections.empty())
    return {};

  const double cx = arc.center_x, cy = arc.center_y, r = arc.radius;

  auto pt_at_angle = [&](double a) -> std::pair<double, double> {
    return {cx + r * std::cos(a), cy + r * std::sin(a)};
  };

  auto [a_start, a_end] = arc_angles(arc);
  const bool ccw = arc.ccw;

  // Build sorted list of angles within the arc sweep.
  std::vector<double> angles;
  for (const auto &is : intersections) {
    double a = wrap_angle(is.param_on_target);
    if (ccw) {
      if (a < a_start)
        a += 2.0 * M_PI;
    } else {
      if (a > a_start)
        a -= 2.0 * M_PI;
    }
    angles.push_back(a);
  }

  // Stably sort in sweep direction.
  std::sort(
      angles.begin(), angles.end(),
      ccw ? [](double a, double b) { return a < b; }
          : [](double a, double b) { return a > b; });

  std::vector<TrimSegment> segments;
  const double k2Pi = 2.0 * M_PI;

  // First segment: from arc start to first intersection.
  {
    double s = a_start, e = angles[0];
    if (ccw) {
      if (e <= s)
        e += k2Pi;
    } else {
      if (e >= s)
        e -= k2Pi;
    }
    auto [sx, sy] = pt_at_angle(s);
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back(
        {TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  }

  // Middle segments.
  for (size_t i = 0; i + 1 < angles.size(); ++i) {
    double s = angles[i], e = angles[i + 1];
    auto [sx, sy] = pt_at_angle(wrap_angle(s));
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back(
        {TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  }

  // Last segment: from last intersection to arc end.
  {
    double s = angles.back(), e = a_end;
    if (ccw) {
      if (e <= s)
        e += k2Pi;
    } else {
      if (e >= s)
        e -= k2Pi;
    }
    auto [sx, sy] = pt_at_angle(wrap_angle(s));
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back(
        {TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  }

  return segments;
}

// ================================================================
// select_clicked_segment (arc)
// ================================================================

int select_clicked_segment(const std::vector<TrimSegment> &segments,
                           const SketchArc &original_arc, double click_x,
                           double click_y) {
  if (segments.empty())
    return -1;

  const double cx = original_arc.center_x;
  const double cy = original_arc.center_y;

  double click_angle = std::atan2(click_y - cy, click_x - cx);
  click_angle = wrap_angle(click_angle);

  auto [a_start, a_end] = arc_angles(original_arc);
  const bool ccw = original_arc.ccw;
  const double k2Pi = 2.0 * M_PI;
  constexpr double kAngTol = 1e-10;

  // Adjust click angle into the arc sweep.
  double test_angle = click_angle;
  if (ccw) {
    if (test_angle < a_start)
      test_angle += k2Pi;
  } else {
    if (test_angle > a_start)
      test_angle -= k2Pi;
  }

  for (size_t i = 0; i < segments.size(); ++i) {
    double s = segments[i].param_start;
    double e = segments[i].param_end;
    if (ccw) {
      if (e <= s)
        e += k2Pi;
    } else {
      if (e >= s)
        e -= k2Pi;
    }
    double ta = test_angle;
    if (ccw && ta < s)
      ta += k2Pi;
    if (!ccw && ta > s)
      ta -= k2Pi;
    if (ta >= s - kAngTol && ta <= e + kAngTol)
      return static_cast<int>(i);
  }

  // Distance fallback.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double dist_sq = point_segment_distance_sq(
        click_x, click_y, segments[i].start_x, segments[i].start_y,
        segments[i].end_x, segments[i].end_y);
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_index = static_cast<int>(i);
    }
  }
  if (best_dist_sq <= kSegmentSelectTolerance * kSegmentSelectTolerance) {
    return best_index;
  }
  return -1;
}
