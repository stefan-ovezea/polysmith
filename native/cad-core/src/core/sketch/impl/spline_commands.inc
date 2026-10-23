// Spline entity (feature/sketch).
//
// Two creation modes (parity T2.1)://   "control" (default) — the user's clicks ARE the B-spline poles, exactly as
//   before. "fit" — the clicks are FIT points: the curve is solved so that it
//   passes through every one of them (spline_fit_poles), and the resulting
//   poles are stored as ordinary poles. The fit points themselves are not
//   stored, so dragging a pole afterwards edits the curve without preserving
//   the interpolation — the same trade Fusion makes for an unrefined fit
//   spline.
//
// Each pole is a regular movable vertex (snapped to coincident geometry like
// line endpoints) so pole dragging re-fits the curve through the existing
// vertex paths. The solver has no spline registration: the curve is always the
// clamped open-uniform B-spline of the poles (spline_math.h).
void add_sketch_spline(FeatureEntry& feature,
                       int spline_index,
                       const std::vector<std::pair<double, double>>& points,
                       bool is_construction,
                       bool refresh_immediately,
                       const std::string& mode,
                       int degree) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch splines");
  }
  if (points.size() < 2) {
    throw std::runtime_error("Sketch splines need at least 2 poles");
  }
  const bool fit_mode = mode == "fit";
  if (!fit_mode && !mode.empty() && mode != "control") {
    throw std::runtime_error("Unknown sketch spline mode: " + mode);
  }

  auto& params = *feature.sketch_parameters;

  // Degree: explicit request clamped to what the point count supports, else
  // the historical default of 3. The cap is 5 (T2.1); every consumer takes the
  // degree from the record (wire builder, profile engine, viewport), so raising
  // it here is all that is needed.
  const int max_degree =
      std::min(kSplineMaxDegree, static_cast<int>(points.size()) - 1);
  const int resolved_degree =
      degree > 0 ? std::min(degree, max_degree) : std::min(3, max_degree);

  // In fit mode the solved poles pass through the clicked points; in control
  // mode the clicked points are the poles.
  std::vector<std::pair<double, double>> poles;
  if (fit_mode) {
    poles.reserve(points.size());
    for (const auto& pole : spline_fit_poles(points, resolved_degree)) {
      poles.emplace_back(pole.x, pole.y);
    }
  } else {
    poles = points;
  }

  SketchSpline spline;
  spline.id = "spline-" + std::to_string(spline_index);
  spline.degree = resolved_degree;
  spline.is_construction = is_construction;
  spline.pole_vertex_ids.reserve(poles.size());
  spline.pole_xs.reserve(poles.size());
  spline.pole_ys.reserve(poles.size());

  // Vertex unification mirrors line endpoints: coincident geometry
  // shares one vertex, and the snapped coordinate is authoritative.
  for (const auto& [px, py] : poles) {
    const auto shared = find_coincident_endpoint(params, "", px, py);
    if (shared.has_value()) {
      spline.pole_vertex_ids.push_back(std::get<0>(shared.value()));
      spline.pole_xs.push_back(std::get<1>(shared.value()));
      spline.pole_ys.push_back(std::get<2>(shared.value()));
    } else {
      spline.pole_vertex_ids.push_back(
          "vertex-" + std::to_string(params.next_vertex_index++));
      spline.pole_xs.push_back(px);
      spline.pole_ys.push_back(py);
    }
  }

  params.splines.push_back(std::move(spline));

  if (refresh_immediately) {
    refresh_sketch_with_append_focus(
        feature, {"spline-" + std::to_string(spline_index)});
  }
}
