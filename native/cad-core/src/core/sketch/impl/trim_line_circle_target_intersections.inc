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
  const ExactCurve target_curve = trim_target_curve(target);
  for_each_trim_candidate(params, target.id, [&](const auto& other) {
    using T = std::decay_t<decltype(other)>;
    if constexpr (std::is_same_v<T, SketchLine>) {
      auto isect = intersect_line_line(target, other);
      if (isect.has_value()) results.push_back(std::move(*isect));
    } else if constexpr (std::is_same_v<T, SketchCircle>) {
      auto isects = intersect_line_circle(target, other);
      for (auto& is : isects) results.push_back(std::move(is));
    } else if constexpr (std::is_same_v<T, SketchArc>) {
      auto isects = intersect_arc_line(other, target);
      for (auto& is : isects) {
        std::swap(is.param_on_target, is.param_on_other);
        is.other_entity_id = other.id;
        results.push_back(std::move(is));
      }
    } else {
      // Ellipse and spline route through the shared curve dispatch so
      // the trim sees exactly what the profile walk sees.
      append_shared_curve_pairs(results, target_curve,
                                trim_target_curve(other), other.id);
    }
  });

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

  // Intersections at the line's own endpoints are KEPT. They used to be
  // filtered here, but the filter compared `param_on_target` — a
  // dimensionless t in [0, 1] — against kTrimCoincidentTolerance, which
  // is 0.01 MILLIMETRES. That discarded every crossing in the first and
  // last 1% of the line's length (a 10 mm dead band on a 1000 mm line),
  // and when it emptied the list the caller fell into its "no
  // intersections — isolated entity" branch and deleted the whole line.
  // Endpoint intersections are harmless: the splitters drop the
  // zero-length segments they produce.
  return finish_trim_intersections(std::move(results));
}

// ================================================================
// find_all_intersections (circle target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchCircle& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  // Construction geometry cuts here too — see the line-target note above.
  const ExactCurve target_curve = trim_target_curve(target);
  for_each_trim_candidate(params, target.id, [&](const auto& other) {
    using T = std::decay_t<decltype(other)>;
    if constexpr (std::is_same_v<T, SketchLine>) {
      auto isects = intersect_circle_line(target, other);
      for (auto& is : isects) results.push_back(std::move(is));
    } else if constexpr (std::is_same_v<T, SketchCircle>) {
      auto isects = intersect_circle_circle(target, other);
      for (auto& is : isects) results.push_back(std::move(is));
    } else if constexpr (std::is_same_v<T, SketchArc>) {
      auto isects = intersect_arc_circle(other, target);
      for (auto& is : isects) {
        std::swap(is.param_on_target, is.param_on_other);
        is.other_entity_id = other.id;
        results.push_back(std::move(is));
      }
    } else {
      // New geometry kinds through the shared dispatch (line-target note).
      append_shared_curve_pairs(results, target_curve,
                                trim_target_curve(other), other.id);
    }
  });

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

  return finish_trim_intersections(std::move(results));
}
