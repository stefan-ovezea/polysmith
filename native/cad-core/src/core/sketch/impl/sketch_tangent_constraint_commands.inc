void set_sketch_tangent_constraint(FeatureEntry& feature,
                                   const std::string& line_id,
                                   const std::string& circle_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can set tangent constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  // Tangent is one-per-line — there's no useful semantics for a
  // single line being tangent to multiple circles simultaneously
  // (it would over-constrain the end point). Drop any prior tangent
  // before reseating.
  remove_line_relations_for_line(parameters, "tangent_line_circle", line_id);

  if (circle_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto circle_it = std::find_if(
      parameters.circles.begin(),
      parameters.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });
  if (circle_it == parameters.circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  // Pre-validate the geometric prerequisite. If the start lies
  // inside or on the circle the enforcer would silently no-op every
  // refresh, leaving the relation but never satisfying it. Better
  // to refuse the op so the user knows immediately.
  const double dx = circle_it->center_x - line.start_x;
  const double dy = circle_it->center_y - line.start_y;
  const double d_squared = dx * dx + dy * dy;
  if (d_squared <= circle_it->radius * circle_it->radius + 1e-9) {
    throw std::runtime_error(
        "Tangent constraint requires the line's start to lie outside the "
        "circle");
  }

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-tangent-" + line_id + "-" + circle_id,
      .kind = "tangent_line_circle",
      .first_line_id = line_id,
      .second_line_id = circle_id,
  });

  refresh_sketch_with_append_focus(feature, {line_id, circle_id});
}
