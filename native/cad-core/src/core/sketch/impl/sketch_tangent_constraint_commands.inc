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

  // Look up the target entity — circle or arc (both have center + radius
  // so the tangent math is identical).
  const auto circle_it = std::find_if(
      parameters.circles.begin(),
      parameters.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });
  const auto arc_it = std::find_if(
      parameters.arcs.begin(),
      parameters.arcs.end(),
      [&](const SketchArc& arc) { return arc.id == circle_id; });
  if (circle_it == parameters.circles.end() &&
      arc_it == parameters.arcs.end()) {
    throw std::runtime_error("Sketch circle or arc not found: " + circle_id);
  }

  const double host_center_x =
      circle_it != parameters.circles.end() ? circle_it->center_x : arc_it->center_x;
  const double host_center_y =
      circle_it != parameters.circles.end() ? circle_it->center_y : arc_it->center_y;
  const double host_radius =
      circle_it != parameters.circles.end() ? circle_it->radius : arc_it->radius;

  // Pre-validate the geometric prerequisite. If the start lies
  // inside or on the circle/arc the enforcer would silently no-op every
  // refresh, leaving the relation but never satisfying it. Better
  // to refuse the op so the user knows immediately.
  const double dx = host_center_x - line.start_x;
  const double dy = host_center_y - line.start_y;
  const double d_squared = dx * dx + dy * dy;
  if (d_squared <= host_radius * host_radius + 1e-9) {
    throw std::runtime_error(
        "Tangent constraint requires the line's start to lie outside the "
        "circle or arc");
  }

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-tangent-" + line_id + "-" + circle_id,
      .kind = "tangent_line_circle",
      .first_line_id = line_id,
      .second_line_id = circle_id,
  });

  // A tangency already puts the line's END on the circle - the end IS the contact
  // point - so a point_on_circle/point_on_arc binding for that same vertex is the
  // same statement twice. At the solution the two gradients are parallel, the
  // Jacobian is singular, and planegcs reports a conflicting system: measured as
  // `sketch solver failed - status=2` on every edit of the user's tangent sketch,
  // and the user's SolidCraft copy of the same shape was refused with "its
  // constraints would conflict (k1, k2, k3, k4, k5, k6)" when a parameter changed.
  // The tangency is the stronger statement (it fixes the direction too), so the
  // duplicate binding goes, with a log line rather than silently.
  const std::string contact_vertex_id = line.end_vertex_id;
  const auto dropped = std::remove_if(
      parameters.constraints.begin(), parameters.constraints.end(),
      [&](const SketchConstraint& constraint) {
        if (constraint.kind != "point_on_circle" &&
            constraint.kind != "point_on_arc") {
          return false;
        }
        if (constraint.target_ids.size() < 2) return false;
        return constraint.target_ids[0] == contact_vertex_id &&
               constraint.target_ids[1] == circle_id;
      });
  if (dropped != parameters.constraints.end()) {
    parameters.constraints.erase(dropped, parameters.constraints.end());
    polysmith::core::log_info(
        "sketch",
        "tangency on " + line_id + " replaces the point-on-curve binding at its "
        "contact end (keeping both makes the sketch singular)");
  }

  refresh_sketch_with_append_focus(feature, {line_id, circle_id});
}
