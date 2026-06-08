// ================================================================
// split_line_at_intersections
// ================================================================

std::vector<TrimSegment> split_line_at_intersections(
    const SketchLine& line,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  const double abx = line.end_x - line.start_x;
  const double aby = line.end_y - line.start_y;

  auto pt = [&](double t) -> std::pair<double, double> {
    return {line.start_x + t * abx, line.start_y + t * aby};
  };

  std::vector<TrimSegment> segments;

  auto [sx, sy] = pt(0.0);
  auto [ex, ey] = pt(intersections[0].param_on_target);
  segments.push_back({TrimSegment::LINE_SEGMENT, 0.0,
                      intersections[0].param_on_target, sx, sy, ex, ey});

  for (size_t i = 0; i + 1 < intersections.size(); ++i) {
    auto [s, sY] = pt(intersections[i].param_on_target);
    auto [e, eY] = pt(intersections[i + 1].param_on_target);
    segments.push_back({TrimSegment::LINE_SEGMENT,
                        intersections[i].param_on_target,
                        intersections[i + 1].param_on_target, s, sY, e, eY});
  }

  auto [lsx, lsy] = pt(intersections.back().param_on_target);
  auto [lex, ley] = pt(1.0);
  segments.push_back({TrimSegment::LINE_SEGMENT,
                      intersections.back().param_on_target, 1.0, lsx, lsy, lex, ley});

  return segments;
}

// ================================================================
// split_circle_at_intersections
// ================================================================

std::vector<TrimSegment> split_circle_at_intersections(
    const SketchCircle& circle,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  const double cx = circle.center_x, cy = circle.center_y, r = circle.radius;

  auto pt_at_angle = [&](double a) -> std::pair<double, double> {
    return {cx + r * std::cos(a), cy + r * std::sin(a)};
  };

  std::vector<TrimSegment> segments;
  const double k2Pi = 2.0 * M_PI;

  for (size_t i = 0; i < intersections.size(); ++i) {
    double a_start = intersections[i].param_on_target;
    double a_end   = intersections[(i + 1) % intersections.size()].param_on_target;

    // Going CCW from a_start to a_end.
    if (a_end <= a_start) a_end += k2Pi;

    auto [sx, sy] = pt_at_angle(a_start);
    auto [ex, ey] = pt_at_angle(a_end >= k2Pi ? wrap_angle(a_end) : a_end);

    segments.push_back(TrimSegment{
        TrimSegment::ARC_SEGMENT,
        a_start,
        a_end < k2Pi ? a_end : a_end - k2Pi,
        sx, sy, ex, ey,
        cx, cy, r,
        true /* ccw */
    });
  }

  return segments;
}
