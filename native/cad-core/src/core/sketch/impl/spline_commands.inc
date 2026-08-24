// Spline entity (feature/sketch).  v1: control-point creation — the
// user's clicks become the B-spline poles.  Each pole is a regular
// movable vertex (snapped to coincident geometry like line endpoints)
// so pole dragging re-fits the curve through the existing vertex
// paths.  The solver has no spline registration: the curve is always
// the clamped open-uniform B-spline of the poles (spline_math.h), and
// any solver moves on shared pole vertices propagate via the pole
// sync in refresh_sketch_derived_state.

void add_sketch_spline(FeatureEntry& feature,
                       int spline_index,
                       const std::vector<std::pair<double, double>>& points,
                       bool is_construction,
                       bool refresh_immediately) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch splines");
  }
  if (points.size() < 2) {
    throw std::runtime_error("Sketch splines need at least 2 poles");
  }

  auto& params = *feature.sketch_parameters;

  SketchSpline spline;
  spline.id = "spline-" + std::to_string(spline_index);
  spline.degree = std::min(3, static_cast<int>(points.size()) - 1);
  spline.is_construction = is_construction;
  spline.pole_vertex_ids.reserve(points.size());
  spline.pole_xs.reserve(points.size());
  spline.pole_ys.reserve(points.size());

  // Vertex unification mirrors line endpoints: coincident geometry
  // shares one vertex, and the snapped coordinate is authoritative.
  for (const auto& [px, py] : points) {
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
