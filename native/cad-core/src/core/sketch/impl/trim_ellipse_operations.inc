// ================================================================
// find_all_intersections (ellipse target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchEllipse& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  const ExactCurve target_curve = trim_target_curve(target);
  auto add_pairs = [&](const ExactCurve& other, const std::string& other_id) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, other, pairs);
    for (const auto& [pa, pb] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, pa);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = pa, .param_on_other = pb,
          .other_entity_id = other_id});
    }
  };

  // Construction geometry cuts here too (line-target note in
  // trim_line_circle_target_intersections.inc).
  for (const auto& line : params.lines) {
    add_pairs(trim_target_curve(line), line.id);
  }
  for (const auto& circle : params.circles) {
    add_pairs(trim_target_curve(circle), circle.id);
  }
  for (const auto& arc : params.arcs) {
    add_pairs(trim_target_curve(arc), arc.id);
  }
  for (const auto& ellipse : params.ellipses) {
    if (ellipse.id == target.id) continue;
    add_pairs(trim_target_curve(ellipse), ellipse.id);
  }
  for (const auto& spline : params.splines) {
    add_pairs(trim_target_curve(spline), spline.id);
  }

  // Endpoint-touch pass: another arc's STORED endpoints that lie on
  // the target are junction cuts the sweep filter of the arc solver
  // can drop (see the arc-target note in trim_arc_intersections.inc).
  // OCCT's InterCurveCurve misses endpoint junctions too, so this
  // pass is load-bearing for trimmed ellipse arcs.
  for (const auto& other : params.arcs) {
    auto add_if_on_target = [&](double px, double py) {
      const auto param = exact_curve_param_at_point(target_curve, px, py);
      if (!param.has_value()) return;
      results.push_back(TrimIntersection{
          .x = px, .y = py,
          .param_on_target = param.value(),
          .param_on_other = 0.0,
          .other_entity_id = other.id});
    };
    add_if_on_target(other.start_x, other.start_y);
    add_if_on_target(other.end_x, other.end_y);
  }

  if (results.empty()) return results;

  std::sort(results.begin(), results.end(),
            [](const TrimIntersection& a, const TrimIntersection& b) {
              return a.param_on_target < b.param_on_target;
            });

  deduplicate(results);
  return results;
}

// ================================================================
// split_ellipse_at_intersections
// ================================================================

std::vector<TrimSegment> split_ellipse_at_intersections(
    const SketchEllipse& ellipse,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  // A lone intersection means the cutting edge is TANGENT to the full
  // ellipse: it touches without crossing, so there is nothing to trim
  // (mirrors split_circle_at_intersections).
  if (!ellipse.has_sweep && intersections.size() < 2) return {};

  const ExactCurve curve = trim_target_curve(ellipse);
  auto pt_at_param = [&](double p) -> std::pair<double, double> {
    const SketchProfilePoint pt = exact_curve_point(curve, p);
    return {pt.x, pt.y};
  };

  std::vector<TrimSegment> segments;
  constexpr double kMinSpan = 1e-9;  // radians

  auto push_piece = [&](double s, double e) {
    if (std::abs(e - s) < kMinSpan) return;
    auto [sx, sy] = pt_at_param(s);
    auto [ex, ey] = pt_at_param(e);
    segments.push_back(TrimSegment{
        TrimSegment::ELLIPSE_SEGMENT,
        s, e,
        sx, sy, ex, ey,
        ellipse.center_x, ellipse.center_y, ellipse.a,
        ellipse.has_sweep ? ellipse.ccw : true});
  };

  const double k2Pi = 2.0 * M_PI;

  if (!ellipse.has_sweep) {
    // Full ellipse: N segments between consecutive wrapped angles,
    // going CCW (circle-style).
    for (size_t i = 0; i < intersections.size(); ++i) {
      double a_start = intersections[i].param_on_target;
      double a_end =
          intersections[(i + 1) % intersections.size()].param_on_target;
      if (a_end <= a_start) a_end += k2Pi;
      if (a_end - a_start < kMinSpan) continue;
      push_piece(a_start, a_end < k2Pi ? a_end : a_end - k2Pi);
    }
    return segments;
  }

  // Partial ellipse: arc-style pieces along the stored sweep.
  const double s0 = ellipse.sweep_start_angle;
  const double e0 = ellipse.sweep_end_angle;
  const bool ccw = ellipse.ccw;

  std::vector<double> angles;
  for (const auto& is : intersections) {
    double a = exact_wrap_angle(is.param_on_target);
    if (ccw) {
      if (a < s0) a += k2Pi;
    } else {
      if (a > s0) a -= k2Pi;
    }
    angles.push_back(a);
  }
  std::sort(angles.begin(), angles.end(),
            ccw ? [](double a, double b) { return a < b; }
                : [](double a, double b) { return a > b; });

  {
    double s = s0, e = angles[0];
    if (ccw) {
      if (e <= s) e += k2Pi;
    } else {
      if (e >= s) e -= k2Pi;
    }
    push_piece(s, e);
  }
  for (size_t i = 0; i + 1 < angles.size(); ++i) {
    push_piece(angles[i], angles[i + 1]);
  }
  {
    double s = angles.back(), e = e0;
    if (ccw) {
      if (e <= s) e += k2Pi;
    } else {
      if (e >= s) e -= k2Pi;
    }
    push_piece(s, e);
  }

  return segments;
}

// ================================================================
// select_clicked_segment (ellipse)
// ================================================================

int select_clicked_segment(const std::vector<TrimSegment>& segments,
                           const SketchEllipse& original_ellipse,
                           double click_x, double click_y) {
  if (segments.empty()) return -1;

  const double cx = original_ellipse.center_x;
  const double cy = original_ellipse.center_y;
  const double cu = std::cos(original_ellipse.rotation);
  const double su = std::sin(original_ellipse.rotation);
  const double lx = (click_x - cx) * cu + (click_y - cy) * su;
  const double ly = -(click_x - cx) * su + (click_y - cy) * cu;
  const double click_angle =
      exact_wrap_angle(std::atan2(ly / original_ellipse.b,
                                  lx / original_ellipse.a));

  const double k2Pi = 2.0 * M_PI;
  constexpr double kAngTol = 1e-10;

  if (!original_ellipse.has_sweep) {
    // Full ellipse: wrap-aware span test, circle-style.
    double test_angle = click_angle;
    for (size_t i = 0; i < segments.size(); ++i) {
      double s = segments[i].param_start;
      double e = segments[i].param_end;
      if (e <= s) e += k2Pi;
      double ta = test_angle;
      if (ta < s) ta += k2Pi;
      if (ta >= s - kAngTol && ta <= e + kAngTol) {
        return static_cast<int>(i);
      }
    }
  } else {
    // Partial ellipse: lift the click into the sweep frame, arc-style.
    double test_angle = click_angle;
    if (original_ellipse.ccw) {
      if (test_angle < original_ellipse.sweep_start_angle) test_angle += k2Pi;
    } else {
      if (test_angle > original_ellipse.sweep_start_angle) test_angle -= k2Pi;
    }
    for (size_t i = 0; i < segments.size(); ++i) {
      double s = segments[i].param_start;
      double e = segments[i].param_end;
      if (original_ellipse.ccw) {
        if (e <= s) e += k2Pi;
        if (test_angle >= s - kAngTol && test_angle <= e + kAngTol) {
          return static_cast<int>(i);
        }
      } else {
        if (e >= s) e -= k2Pi;
        if (test_angle <= s + kAngTol && test_angle >= e - kAngTol) {
          return static_cast<int>(i);
        }
      }
    }
  }

  // Fallback: nearest segment endpoint within the selection tolerance.
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  for (size_t i = 0; i < segments.size(); ++i) {
    const double ds = std::hypot(click_x - segments[i].start_x,
                                 click_y - segments[i].start_y);
    const double de = std::hypot(click_x - segments[i].end_x,
                                 click_y - segments[i].end_y);
    const double d = std::min(ds, de);
    if (d < best_dist_sq) {
      best_dist_sq = d;
      best_index = static_cast<int>(i);
    }
  }
  if (best_dist_sq <= kSegmentSelectTolerance * kSegmentSelectTolerance) {
    return best_index;
  }
  return -1;
}
