// Shared intersection traversal for find_all_intersections. The five
// target-kind overloads used to each carry their own copy of the
// entity loops, the shared-curve pair appender, and the sort + dedup
// tail — the same boilerplate drifting apart per kind. These helpers
// are the ONE copy: each overload keeps only its per-kind analytic
// fast paths and its endpoint-touch passes.

// Visits every other non-construction-or-not entity of the sketch —
// construction geometry participates as a cutting edge by design
// (users draw reference curves specifically to trim against them;
// construction curves still never form profiles — that remains the
// face walk's own rule).
template <typename Fn>
void for_each_trim_candidate(const SketchFeatureParameters& params,
                             const std::string& target_id, Fn&& fn) {
  for (const auto& line : params.lines) {
    if (line.id == target_id) continue;
    fn(line);
  }
  for (const auto& circle : params.circles) {
    if (circle.id == target_id) continue;
    fn(circle);
  }
  for (const auto& arc : params.arcs) {
    if (arc.id == target_id) continue;
    fn(arc);
  }
  for (const auto& ellipse : params.ellipses) {
    if (ellipse.id == target_id) continue;
    fn(ellipse);
  }
  for (const auto& spline : params.splines) {
    if (spline.id == target_id) continue;
    fn(spline);
  }
}

// Appends the intersections of `other` against `target_curve` through
// the shared exact-curve dispatch (the same layer the profile walk
// uses, so trim and the walk can never disagree). `param_on_target`
// carries the target-curve parameter.
void append_shared_curve_pairs(std::vector<TrimIntersection>& results,
                               const ExactCurve& target_curve,
                               const ExactCurve& other_curve,
                               const std::string& other_id) {
  std::vector<std::pair<double, double>> pairs;
  sketch_curve_intersections(target_curve, other_curve, pairs);
  for (const auto& [param_on_target, param_on_other] : pairs) {
    const SketchProfilePoint p = exact_curve_point(target_curve, param_on_target);
    results.push_back(TrimIntersection{
        .x = p.x, .y = p.y,
        .param_on_target = param_on_target, .param_on_other = param_on_other,
        .other_entity_id = other_id});
  }
}

// The shared tail: sort by target parameter and deduplicate points
// that coincide within kTrimCoincidentTolerance.
std::vector<TrimIntersection> finish_trim_intersections(
    std::vector<TrimIntersection> results) {
  if (results.empty()) return results;
  std::sort(results.begin(), results.end(),
            [](const TrimIntersection& a, const TrimIntersection& b) {
              return a.param_on_target < b.param_on_target;
            });
  deduplicate(results);
  return results;
}
