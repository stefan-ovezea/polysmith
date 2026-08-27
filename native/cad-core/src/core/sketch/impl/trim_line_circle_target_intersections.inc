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

  if (results.empty()) return results;

  std::sort(results.begin(), results.end(),
            [](const TrimIntersection& a, const TrimIntersection& b) {
              return a.param_on_target < b.param_on_target;
            });

  deduplicate(results);
  return results;
}
