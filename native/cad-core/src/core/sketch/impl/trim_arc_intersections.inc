// ================================================================
// intersect_arc_line
// ================================================================

std::vector<TrimIntersection> intersect_arc_line(const SketchArc &target,
                                                 const SketchLine &other) {
  auto isects = intersect_line_circle(other, SketchCircle{
                                                 .id = target.id,
                                                 .center_x = target.center_x,
                                                 .center_y = target.center_y,
                                                 .radius = target.radius,
                                             });

  auto [a_start, a_end] = arc_angles(target);

  std::vector<TrimIntersection> result;
  for (auto &is : isects) {
    // is.param_on_other is the angle on the circle.
    if (angle_in_arc_sweep(is.param_on_other, a_start, a_end, target.ccw)) {
      // Remap: param_on_target = angle on arc, param_on_other = t on line.
      result.push_back(TrimIntersection{
          .x = is.x,
          .y = is.y,
          .param_on_target = is.param_on_other,
          .param_on_other = is.param_on_target,
          .other_entity_id = other.id,
      });
    }
  }
  return result;
}

// ================================================================
// intersect_arc_circle
// ================================================================

std::vector<TrimIntersection> intersect_arc_circle(const SketchArc &target,
                                                   const SketchCircle &other) {
  auto isects =
      intersect_circle_circle(SketchCircle{.id = target.id,
                                           .center_x = target.center_x,
                                           .center_y = target.center_y,
                                           .radius = target.radius},
                              other);

  auto [a_start, a_end] = arc_angles(target);

  std::vector<TrimIntersection> result;
  for (auto &is : isects) {
    if (angle_in_arc_sweep(is.param_on_target, a_start, a_end, target.ccw)) {
      is.other_entity_id = other.id;
      result.push_back(std::move(is));
    }
  }
  return result;
}

// ================================================================
// intersect_arc_arc
// ================================================================

std::vector<TrimIntersection> intersect_arc_arc(const SketchArc &target,
                                                const SketchArc &other) {
  auto isects =
      intersect_circle_circle(SketchCircle{.id = target.id,
                                           .center_x = target.center_x,
                                           .center_y = target.center_y,
                                           .radius = target.radius},
                              SketchCircle{.id = other.id,
                                           .center_x = other.center_x,
                                           .center_y = other.center_y,
                                           .radius = other.radius});

  auto [ta_start, ta_end] = arc_angles(target);
  auto [oa_start, oa_end] = arc_angles(other);

  std::vector<TrimIntersection> result;
  for (auto &is : isects) {
    if (angle_in_arc_sweep(is.param_on_target, ta_start, ta_end, target.ccw) &&
        angle_in_arc_sweep(is.param_on_other, oa_start, oa_end, other.ccw)) {
      is.other_entity_id = other.id;
      result.push_back(std::move(is));
    }
  }
  return result;
}

// ================================================================
// find_all_intersections (arc target)
// ================================================================

std::vector<TrimIntersection>
find_all_intersections(const SketchArc &target,
                       const SketchFeatureParameters &params) {
  std::vector<TrimIntersection> results;

  // Construction geometry cuts here too — see the line-target note in
  // trim_line_circle_target_intersections.inc.
  for (const auto &line : params.lines) {
    auto isects = intersect_arc_line(target, line);
    for (auto &is : isects)
      results.push_back(std::move(is));
  }

  for (const auto &circle : params.circles) {
    auto isects = intersect_arc_circle(target, circle);
    for (auto &is : isects)
      results.push_back(std::move(is));
  }

  for (const auto &arc : params.arcs) {
    if (arc.id == target.id)
      continue;
    auto isects = intersect_arc_arc(target, arc);
    for (auto &is : isects)
      results.push_back(std::move(is));
  }

  // New geometry kinds through the shared dispatch (line-target note).
  const ExactCurve target_curve = trim_target_curve(target);
  for (const auto &ellipse : params.ellipses) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, trim_target_curve(ellipse), pairs);
    for (const auto &[angle, eangle] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, angle);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = angle, .param_on_other = eangle,
          .other_entity_id = ellipse.id});
    }
  }
  for (const auto &spline : params.splines) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, trim_target_curve(spline), pairs);
    for (const auto &[angle, sparam] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, angle);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = angle, .param_on_other = sparam,
          .other_entity_id = spline.id});
    }
  }

  // Endpoint-touch pass: another arc's STORED endpoints that lie ON
  // the target are genuine junction cuts. The strict angular sweep
  // filter above drops them at random — the stored endpoint coords
  // are a trig roundtrip of the same intersection the solver
  // recomputes analytically, and the two angles differ by ~1e-4 rad,
  // far beyond the sweep's 1e-12 tolerance. A missing junction makes
  // the segment list too coarse, so the trim deletes the piece
  // adjacent to the notch instead of the notch itself (the
  // six-petal flower bug). The profile walk has its own touch
  // records, so this pass stays in the trim engine only.
  {
    auto [as, ae] = arc_angles(target);
    for (const auto &other : params.arcs) {
      if (other.id == target.id) continue;
      auto add_if_on_target = [&](double px, double py) {
        const double dx = px - target.center_x;
        const double dy = py - target.center_y;
        if (std::abs(std::hypot(dx, dy) - target.radius) >
            kTrimCoincidentTolerance) {
          return;
        }
        const double ang = wrap_angle(std::atan2(dy, dx));
        if (!angle_in_arc_sweep(ang, as, ae, target.ccw)) return;
        results.push_back(TrimIntersection{
            .x = px, .y = py,
            .param_on_target = ang,
            .param_on_other = 0.0,
            .other_entity_id = other.id});
      };
      add_if_on_target(other.start_x, other.start_y);
      add_if_on_target(other.end_x, other.end_y);
    }
  }

  // Intersections at the arc's own endpoints produce near-zero-length
  // pieces whose endpoints sit closest to clicks anywhere along the
  // arc's middle — the CW selector's distance fallback then hands the
  // click to that stub instead of the piece the user pointed at (the
  // "middle trim does not split" failure). Drop them when the arc has
  // real intersections to trim with; keep them when they are the ONLY
  // intersections, because then the trim is a plain delete and an
  // empty result would instead leave the arc untouched.
  {
    auto [as, ae] = arc_angles(target);
    constexpr double kArcEndpointAngTol = 1e-6;  // radians
    auto copy = results;
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [&](const TrimIntersection &is) {
                         double a = wrap_angle(is.param_on_target);
                         return (std::abs(std::sin(a - as)) < kArcEndpointAngTol &&
                                 std::abs(std::cos(a - as) - 1.0) <
                                     kArcEndpointAngTol) ||
                                (std::abs(std::sin(a - ae)) < kArcEndpointAngTol &&
                                 std::abs(std::cos(a - ae) - 1.0) <
                                     kArcEndpointAngTol);
                       }),
        results.end());
    if (results.empty())
      results = std::move(copy);
  }

  if (results.empty())
    return results;

  std::sort(results.begin(), results.end(),
            [](const TrimIntersection &a, const TrimIntersection &b) {
              return a.param_on_target < b.param_on_target;
            });

  deduplicate(results);
  return results;
}
