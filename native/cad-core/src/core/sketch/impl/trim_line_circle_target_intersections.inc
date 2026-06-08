// ================================================================
// find_all_intersections (line target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchLine& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  int total_lines = 0, skipped_constr = 0, found = 0;

  for (const auto& line : params.lines) {
    if (line.id == target.id) continue;
    ++total_lines;
    if (line.is_construction) { ++skipped_constr; continue; }
    auto isect = intersect_line_line(target, line);
    if (isect.has_value()) { ++found; results.push_back(std::move(*isect)); }
  }

  for (const auto& circle : params.circles) {
    if (circle.is_construction) continue;
    auto isects = intersect_line_circle(target, circle);
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    if (arc.is_construction) continue;
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

  // Filter out intersections at the line's own endpoints — they
  // produce zero-length segments and prevent meaningful trimming.
  // Same protection the arc path already applies at line ~674.
  results.erase(
      std::remove_if(results.begin(), results.end(),
                     [](const TrimIntersection& is) {
                       return is.param_on_target < kTrimCoincidentTolerance ||
                              is.param_on_target > 1.0 - kTrimCoincidentTolerance;
                     }),
      results.end());

  return results;
}

// ================================================================
// find_all_intersections (circle target)
// ================================================================

std::vector<TrimIntersection> find_all_intersections(
    const SketchCircle& target,
    const SketchFeatureParameters& params) {
  std::vector<TrimIntersection> results;

  int total_lines = 0, total_circles = 0, skipped_constr = 0, found = 0;

  for (const auto& line : params.lines) {
    ++total_lines;
    if (line.is_construction) { ++skipped_constr; continue; }
    auto isects = intersect_circle_line(target, line);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& circle : params.circles) {
    if (circle.id == target.id) continue;
    ++total_circles;
    if (circle.is_construction) { ++skipped_constr; continue; }
    auto isects = intersect_circle_circle(target, circle);
    found += static_cast<int>(isects.size());
    for (auto& is : isects) results.push_back(std::move(is));
  }

  for (const auto& arc : params.arcs) {
    if (arc.is_construction) continue;
    auto isects = intersect_arc_circle(arc, target);
    for (auto& is : isects) {
      std::swap(is.param_on_target, is.param_on_other);
      is.other_entity_id = arc.id;
      results.push_back(std::move(is));
      ++found;
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
