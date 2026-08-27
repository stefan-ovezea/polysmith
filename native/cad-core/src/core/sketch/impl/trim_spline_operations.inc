// ================================================================
// find_all_intersections (spline target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchSpline& target,
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
  // trim_line_circle_target_intersections.inc). No endpoint-touch
  // pass needed: spline junction splits arrive through
  // spline_profile_intersections' segment-endpoint handling.
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
    add_pairs(trim_target_curve(ellipse), ellipse.id);
  }
  for (const auto& spline : params.splines) {
    if (spline.id == target.id) continue;
    add_pairs(trim_target_curve(spline), spline.id);
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
// split_spline_at_intersections
// ================================================================

std::vector<TrimSegment> split_spline_at_intersections(
    const SketchSpline& spline,
    const std::vector<TrimIntersection>& intersections) {
  if (intersections.empty()) return {};

  const ExactCurve curve = trim_target_curve(spline);
  auto pt_at = [&](double t) -> std::pair<double, double> {
    const SketchProfilePoint p = exact_curve_point(curve, t);
    return {p.x, p.y};
  };

  std::vector<TrimSegment> segments;
  auto push_piece = [&](double t0, double t1) {
    auto [sx, sy] = pt_at(t0);
    auto [ex, ey] = pt_at(t1);
    const double px = ex - sx, py = ey - sy;
    if (std::sqrt(px * px + py * py) < kTrimCoincidentTolerance) return;
    segments.push_back({TrimSegment::LINE_SEGMENT, t0, t1, sx, sy, ex, ey});
  };

  // Spline params live in [0, 1] — line-style split. A single
  // interior intersection legally splits an open curve (a tangent
  // touch still divides it, unlike a closed circle/ellipse).
  push_piece(0.0, intersections[0].param_on_target);
  for (size_t i = 0; i + 1 < intersections.size(); ++i) {
    push_piece(intersections[i].param_on_target,
               intersections[i + 1].param_on_target);
  }
  push_piece(intersections.back().param_on_target, 1.0);

  return segments;
}

// ================================================================
// select_clicked_segment (spline)
// ================================================================

int select_clicked_segment(const std::vector<TrimSegment>& segments,
                           const SketchSpline& original_spline,
                           double click_x, double click_y) {
  if (segments.empty()) return -1;

  const ExactCurve curve = trim_target_curve(original_spline);

  // Primary: project the click onto the curve. A projection that lands
  // STRICTLY inside a segment is authoritative; a projection at a
  // segment boundary is ambiguous (OCCT can report either endpoint for
  // a symmetric curve) and falls through to the sampled fallback.
  const auto param = exact_curve_param_at_point(curve, click_x, click_y);
  if (param.has_value()) {
    constexpr double kBoundaryTol = 1e-9;
    for (size_t i = 0; i < segments.size(); ++i) {
      if (param.value() > segments[i].param_start + kBoundaryTol &&
          param.value() < segments[i].param_end - kBoundaryTol) {
        return static_cast<int>(i);
      }
    }
  }

  // Fallback: nearest SEGMENT by dense curve sampling — the endpoint
  // distance metric mis-picks when the click sits deep inside a long
  // segment (the apex is far from both cut points).
  double best_dist_sq = std::numeric_limits<double>::max();
  int best_index = -1;
  constexpr int kSamples = 24;
  for (size_t i = 0; i < segments.size(); ++i) {
    for (int k = 0; k <= kSamples; ++k) {
      const double t =
          segments[i].param_start +
          (segments[i].param_end - segments[i].param_start) *
              (static_cast<double>(k) / kSamples);
      const SketchProfilePoint p = exact_curve_point(curve, t);
      const double dx = click_x - p.x, dy = click_y - p.y;
      const double d = dx * dx + dy * dy;
      if (d < best_dist_sq) {
        best_dist_sq = d;
        best_index = static_cast<int>(i);
      }
    }
  }
  if (best_dist_sq <= kSegmentSelectTolerance * kSegmentSelectTolerance) {
    return best_index;
  }
  return -1;
}
