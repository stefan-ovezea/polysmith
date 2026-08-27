// ================================================================
// find_all_intersections (line target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchLine& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  // Construction geometry participates as a cutting edge: users draw
  // reference curves specifically to trim against them, and the
  // original tool design called for it. Construction curves still
  // never form profiles — that remains the face walk's own rule.
  for (const auto& line : params.lines) {
    if (line.id == target.id) continue;
    auto isect = intersect_line_line(target, line);
    if (isect.has_value()) results.push_back(std::move(*isect));
  }

  for (const auto& circle : params.circles) {
    auto isects = intersect_line_circle(target, circle);
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    auto isects = intersect_arc_line(arc, target);
    for (auto& is : isects) {
      std::swap(is.param_on_target, is.param_on_other);
      is.other_entity_id = arc.id;
      results.push_back(std::move(is));
    }
  }

  // New geometry kinds route through the shared curve-intersection
  // dispatch so the trim sees exactly what the profile walk sees.
  const ExactCurve target_curve = trim_target_curve(target);
  for (const auto& ellipse : params.ellipses) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, trim_target_curve(ellipse), pairs);
    for (const auto& [t, angle] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, t);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = t, .param_on_other = angle,
          .other_entity_id = ellipse.id});
    }
  }
  for (const auto& spline : params.splines) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, trim_target_curve(spline), pairs);
    for (const auto& [t, sparam] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, t);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = t, .param_on_other = sparam,
          .other_entity_id = spline.id});
    }
  }

  // Endpoint-touch pass: another arc's STORED endpoints that lie on
  // the target line are junction cuts the sweep filter of the arc
  // solver can drop (see the arc-target note in
  // trim_arc_intersections.inc). The profile walk has its own touch
  // records, so this stays trim-only.
  for (const auto& other : params.arcs) {
    auto add_if_on_target = [&](double px, double py) {
      const double abx = target.end_x - target.start_x;
      const double aby = target.end_y - target.start_y;
      const double len_sq = abx * abx + aby * aby;
      const double t =
          len_sq > 1e-18
              ? std::max(0.0, std::min(
                                  1.0, ((px - target.start_x) * abx +
                                        (py - target.start_y) * aby) /
                                           len_sq))
              : 0.0;
      const double qx = target.start_x + t * abx;
      const double qy = target.start_y + t * aby;
      if (std::hypot(px - qx, py - qy) > kTrimCoincidentTolerance) return;
      results.push_back(TrimIntersection{
          .x = px, .y = py,
          .param_on_target = t,
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

  // Intersections at the line's own endpoints are KEPT. They used to be
  // filtered here, but the filter compared `param_on_target` — a
  // dimensionless t in [0, 1] — against kTrimCoincidentTolerance, which
  // is 0.01 MILLIMETRES. That discarded every crossing in the first and
  // last 1% of the line's length (a 10 mm dead band on a 1000 mm line),
  // and when it emptied the list the caller fell into its "no
  // intersections — isolated entity" branch and deleted the whole line.
  // Endpoint intersections are harmless: the splitters drop the
  // zero-length segments they produce.
  return results;
}

// ================================================================
// find_all_intersections (circle target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchCircle& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  // Construction geometry cuts here too — see the line-target note above.
  for (const auto& line : params.lines) {
    auto isects = intersect_circle_line(target, line);
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& circle : params.circles) {
    if (circle.id == target.id) continue;
    auto isects = intersect_circle_circle(target, circle);
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    auto isects = intersect_arc_circle(arc, target);
    for (auto& is : isects) {
      std::swap(is.param_on_target, is.param_on_other);
      is.other_entity_id = arc.id;
      results.push_back(std::move(is));
    }
  }

  // New geometry kinds through the shared dispatch (line-target note).
  const ExactCurve target_curve = trim_target_curve(target);
  for (const auto& ellipse : params.ellipses) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, trim_target_curve(ellipse), pairs);
    for (const auto& [angle, eangle] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, angle);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = angle, .param_on_other = eangle,
          .other_entity_id = ellipse.id});
    }
  }
  for (const auto& spline : params.splines) {
    std::vector<std::pair<double, double>> pairs;
    sketch_curve_intersections(target_curve, trim_target_curve(spline), pairs);
    for (const auto& [angle, sparam] : pairs) {
      const SketchProfilePoint p = exact_curve_point(target_curve, angle);
      results.push_back(TrimIntersection{
          .x = p.x, .y = p.y,
          .param_on_target = angle, .param_on_other = sparam,
          .other_entity_id = spline.id});
    }
  }

  // Endpoint-touch pass: another arc's STORED endpoints that lie on
  // the target circle (arc-solver sweep filter can drop them — see
  // the arc-target note in trim_arc_intersections.inc). Trim-only;
  // the profile walk has its own touch records.
  for (const auto& other : params.arcs) {
    auto add_if_on_target = [&](double px, double py) {
      const double dx = px - target.center_x;
      const double dy = py - target.center_y;
      if (std::abs(std::hypot(dx, dy) - target.radius) >
          kTrimCoincidentTolerance) {
        return;
      }
      results.push_back(TrimIntersection{
          .x = px, .y = py,
          .param_on_target = wrap_angle(std::atan2(dy, dx)),
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
