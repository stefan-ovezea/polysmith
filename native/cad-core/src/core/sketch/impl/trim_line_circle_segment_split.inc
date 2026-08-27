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

  // Degenerate pieces are dropped here rather than by filtering the
  // intersection list upstream. An intersection sitting on one of the
  // line's own endpoints is real and must still count as a cut — it
  // simply produces no piece on the far side of itself. Filtering it
  // out upstream is what used to make a line with only endpoint
  // crossings look "isolated", which deleted it outright.
  auto push_piece = [&](double t0, double t1) {
    auto [sx, sy] = pt(t0);
    auto [ex, ey] = pt(t1);
    const double px = ex - sx, py = ey - sy;
    if (std::sqrt(px * px + py * py) < kTrimCoincidentTolerance) return;
    segments.push_back({TrimSegment::LINE_SEGMENT, t0, t1, sx, sy, ex, ey});
  };

  push_piece(0.0, intersections[0].param_on_target);
  for (size_t i = 0; i + 1 < intersections.size(); ++i) {
    push_piece(intersections[i].param_on_target,
               intersections[i + 1].param_on_target);
  }
  push_piece(intersections.back().param_on_target, 1.0);

  return segments;
}

// ================================================================
// split_circle_at_intersections
// ================================================================

std::vector<TrimSegment> split_circle_at_intersections(
    const SketchCircle& circle,
    const std::vector<TrimIntersection>& intersections) {
  // A lone intersection means the cutting edge is TANGENT: it touches
  // the circle without crossing it, so it does not divide the circle
  // into pieces and there is nothing to trim. The old code still built
  // one "segment" from that single angle back to itself — spanning the
  // full 2π with param_start == param_end — which the caller turned
  // into an arc whose endpoints coincide. The profile walker then read
  // that degenerate arc as a full circle (the "trimmed to a half
  // circle, detected as a full circle" bug). Returning nothing leaves
  // the circle whole, which is the correct CAD behaviour.
  if (intersections.size() < 2) return {};

  const double cx = circle.center_x, cy = circle.center_y, r = circle.radius;

  auto pt_at_angle = [&](double a) -> std::pair<double, double> {
    return {cx + r * std::cos(a), cy + r * std::sin(a)};
  };

  std::vector<TrimSegment> segments;
  const double k2Pi = 2.0 * M_PI;
  constexpr double kMinArcSpan = 1e-9;  // radians

  for (size_t i = 0; i < intersections.size(); ++i) {
    double a_start = intersections[i].param_on_target;
    double a_end   = intersections[(i + 1) % intersections.size()].param_on_target;

    // Going CCW from a_start to a_end.
    if (a_end <= a_start) a_end += k2Pi;

    if (a_end - a_start < kMinArcSpan) continue;

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
