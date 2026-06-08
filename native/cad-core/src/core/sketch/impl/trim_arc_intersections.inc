// ================================================================
// intersect_arc_line
// ================================================================

std::vector<TrimIntersection> intersect_arc_line(const SketchArc &target,
                                                 const SketchLine &other) {
  auto isects = intersect_line_circle(other, SketchCircle{
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
      intersect_circle_circle(SketchCircle{.center_x = target.center_x,
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
      intersect_circle_circle(SketchCircle{.center_x = target.center_x,
                                           .center_y = target.center_y,
                                           .radius = target.radius},
                              SketchCircle{.center_x = other.center_x,
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

  int total_lines = 0, total_circles = 0, total_arcs = 0, found = 0;

  for (const auto &line : params.lines) {
    ++total_lines;
    if (line.is_construction)
      continue;
    auto isects = intersect_arc_line(target, line);
    found += static_cast<int>(isects.size());
    for (auto &is : isects)
      results.push_back(std::move(is));
  }

  for (const auto &circle : params.circles) {
    ++total_circles;
    if (circle.is_construction)
      continue;
    auto isects = intersect_arc_circle(target, circle);
    found += static_cast<int>(isects.size());
    for (auto &is : isects)
      results.push_back(std::move(is));
  }

  for (const auto &arc : params.arcs) {
    if (arc.id == target.id)
      continue;
    ++total_arcs;
    if (arc.is_construction)
      continue;
    auto isects = intersect_arc_arc(target, arc);
    found += static_cast<int>(isects.size());
    for (auto &is : isects)
      results.push_back(std::move(is));
  }

  // Filter out intersections at the arc's own endpoints — they
  // produce zero-length segments and prevent meaningful trimming.
  // Keep them only if they're the *only* intersections, otherwise
  // the arc would be fully deleted (the caller treats an empty
  // result as "isolated entity → delete").
  {
    auto [as, ae] = arc_angles(target);
    const double kEpTol = 1e-6;
    auto copy = results;
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [&](const TrimIntersection &is) {
                         double a = wrap_angle(is.param_on_target);
                         return (std::abs(std::sin(a - as)) < kEpTol &&
                                 std::abs(std::cos(a - as) - 1.0) < kEpTol) ||
                                (std::abs(std::sin(a - ae)) < kEpTol &&
                                 std::abs(std::cos(a - ae) - 1.0) < kEpTol);
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
