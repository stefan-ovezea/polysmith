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
  //
  // Intersections sitting on the arc's OWN endpoints are the tricky
  // case: their analytic angle is a trig roundtrip of the stored
  // endpoint coordinates and lands within ~1e-4 rad of a_start/a_end.
  // The old wrap fixups used `<=` / `>=`, so a boundary hit at exact
  // equality with a_start (or a_end) was shifted a full 2π — that
  // manufactured ghost FULL-CIRCLE segments alongside the real pieces.
  // The click then landed in a ghost ("the whole circle appears back
  // in red"), the trim "deleted" the piece that does not exist and
  // silently did nothing. Snap boundary hits to the sweep edges
  // first, then shift only what is genuinely outside the sweep.
  std::vector<double> angles;
  // "This intersection IS the arc endpoint" = coincident tolerance as
  // a fraction of the radius (the same dimensional rule the profile
  // walk and the piece drop below use).
  const double snap_tol = kTrimCoincidentTolerance / std::max(r, 1e-9);
  auto angular_gap = [](double x, double y) {
    // x and y may sit on different unwrapped axes (y can be negative
    // down to -2π), so the raw difference can exceed 2π — normalize
    // first, then fold to [0, π]. The naive min(d, 2π-d) goes NEGATIVE
    // for d > 2π and snaps everything.
    double d = std::fmod(std::abs(x - y), 2.0 * M_PI);
    if (d > M_PI) d = 2.0 * M_PI - d;
    return d;
  };
  // arc_angles returns both sweep ends WRAPPED into [0, 2π) — a wrap-
  // through-zero arc stores a_end < a_start. Unwrap the end so every
  // angle in the sweep lives on one ascending (ccw) / descending (cw)
  // axis; clamping against the wrapped end collapsed every interior
  // angle onto it (one-piece result → whole arc deleted).
  double a_end_u = a_end;
  if (ccw) {
    if (a_end_u < a_start) a_end_u += 2.0 * M_PI;
  } else {
    if (a_end_u > a_start) a_end_u -= 2.0 * M_PI;
  }
  for (const auto &is : intersections) {
    double a = wrap_angle(is.param_on_target);
    if (angular_gap(a, a_start) < snap_tol) {
      a = a_start;
    } else if (angular_gap(a, a_end_u) < snap_tol) {
      a = a_end_u;
    }
    if (ccw) {
      if (a < a_start) a += 2.0 * M_PI;
      if (a > a_end_u) a = a_end_u;  // wrap-around of a start-boundary hit
    } else {
      if (a > a_start) a -= 2.0 * M_PI;
      if (a < a_end_u) a = a_end_u;
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

  // Degenerate pieces are dropped here rather than by filtering the
  // intersection list upstream. An intersection landing on one of the
  // arc's own endpoints is a real cut — it just leaves no piece beyond
  // itself — so it must still split the arc.
  // The drop is DIMENSIONAL (arc length), not angular: a stub below
  // the coincident tolerance is unclickable and, when kept, captures
  // clicks that belong to the neighbouring real piece. The old 1e-9
  // rad angular floor kept ~1e-4-rad trig-roundtrip stubs alive.
  auto push_piece = [&](double s, double e) {
    if (std::abs(e - s) * r < kTrimCoincidentTolerance) return;
    auto [sx, sy] = pt_at_angle(wrap_angle(s));
    auto [ex, ey] = pt_at_angle(wrap_angle(e));
    segments.push_back(
        {TrimSegment::ARC_SEGMENT, s, e, sx, sy, ex, ey, cx, cy, r, ccw});
  };

  // First segment: from arc start to first intersection. The angles
  // are already inside the sweep (snap/shift/clamp above), so the
  // wrap fixup must be STRICT: equality here means the cut sits on the
  // boundary and the piece is degenerate — shifting it a full 2π
  // manufactured the ghost full-circle segment (the "wrong walk
  // direction" bug).
  {
    double s = a_start, e = angles[0];
    if (ccw) {
      if (e < s)
        e += k2Pi;
    } else {
      if (e > s)
        e -= k2Pi;
    }
    push_piece(s, e);
  }

  // Middle segments.
  for (size_t i = 0; i + 1 < angles.size(); ++i) {
    push_piece(angles[i], angles[i + 1]);
  }

  // Last segment: from last intersection to arc end (unwrapped — the
  // angles are already on the same axis).
  {
    double s = angles.back(), e = a_end_u;
    if (ccw) {
      if (e < s)
        e += k2Pi;
    } else {
      if (e > s)
        e -= k2Pi;
    }
    push_piece(s, e);
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
    if (ccw) {
      if (ta < s)
        ta += k2Pi;
      if (ta >= s - kAngTol && ta <= e + kAngTol)
        return static_cast<int>(i);
    } else {
      // CW intervals are descending (e < s) and test_angle was already
      // normalized into the sweep once above.  Re-shifting per segment
      // here would push clicks on later segments out of range, and
      // testing [s, e] ascending is an empty interval — so test the
      // reversed range instead.
      if (ta <= s + kAngTol && ta >= e - kAngTol)
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
