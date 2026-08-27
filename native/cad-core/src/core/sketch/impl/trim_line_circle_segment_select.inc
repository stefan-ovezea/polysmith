// ================================================================
// select_clicked_segment (line)
// ================================================================

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchLine& original_line,
    double click_x,
    double click_y) {
  if (segments.empty()) return -1;

  const double abx = original_line.end_x - original_line.start_x;
  const double aby = original_line.end_y - original_line.start_y;
  const double ab_len_sq = abx * abx + aby * aby;

  double click_t = -1.0;
  if (ab_len_sq > kTrimCoincidentTolerance * kTrimCoincidentTolerance) {
    click_t = ((click_x - original_line.start_x) * abx +
               (click_y - original_line.start_y) * aby) / ab_len_sq;
    click_t = std::clamp(click_t, 0.0, 1.0);
  }

  constexpr double kParamTol = 1e-10;
  for (size_t i = 0; i < segments.size(); ++i) {
    if (click_t >= segments[i].param_start - kParamTol &&
        click_t <= segments[i].param_end + kParamTol) {
      return static_cast<int>(i);
    }
  }

  // Distance fallback.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double dist_sq = point_segment_distance_sq(
        click_x, click_y,
        segments[i].start_x, segments[i].start_y,
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

// ================================================================
// select_clicked_segment (circle)
// ================================================================

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchCircle& original_circle,
    double click_x,
    double click_y) {
  if (segments.empty()) return -1;

  const double cx = original_circle.center_x;
  const double cy = original_circle.center_y;

  // Angle of click point relative to circle center.
  double click_angle = std::atan2(click_y - cy, click_x - cx);
  click_angle = wrap_angle(click_angle);

  const double k2Pi = 2.0 * M_PI;
  constexpr double kAngTol = 1e-10;

  for (size_t i = 0; i < segments.size(); ++i) {
    double a_start = segments[i].param_start;
    double a_end   = segments[i].param_end;

    // Arc might wrap around 0.
    bool wrapped = (a_end <= a_start);
    if (wrapped) a_end += k2Pi;

    double test_angle = click_angle;
    if (wrapped && test_angle < a_start) test_angle += k2Pi;

    if (test_angle >= a_start - kAngTol && test_angle <= a_end + kAngTol) {
      return static_cast<int>(i);
    }
  }

  // Distance fallback measured against the arc itself — the chord of a
  // large segment sits far under the click (the sagitta), so the old
  // chord test could hand the click to a neighbouring segment.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double dist_sq =
        point_trim_segment_distance_sq(segments[i], click_x, click_y);
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
