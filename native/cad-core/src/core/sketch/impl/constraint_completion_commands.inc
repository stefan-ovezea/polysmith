// Constraint-completion commands: symmetric, midpoint, collinear, and
// curve-curve tangent pairs.  Each stores a constraint entry the solver
// and the ad-hoc enforcement passes can re-apply on every refresh.



void set_sketch_symmetric_constraint(FeatureEntry& feature,
                                     const std::string& point_id,
                                     const std::string& other_point_id,
                                     const std::string& axis_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set symmetric constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  if (point_id == other_point_id) {
    throw std::runtime_error("Symmetric constraint requires two distinct points");
  }
  if (!point_position_of(parameters, point_id).has_value()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }
  if (!point_position_of(parameters, other_point_id).has_value()) {
    throw std::runtime_error("Sketch point not found: " + other_point_id);
  }
  require_line(parameters, axis_line_id);

  // Dedupe by point pair (the axis is part of the constraint identity).
  for (const auto& c : parameters.constraints) {
    if (c.kind != "symmetric" || c.target_ids.size() != 3) continue;
    if (c.target_ids[2] != axis_line_id) continue;
    const bool same_pair =
        (c.target_ids[0] == point_id && c.target_ids[1] == other_point_id) ||
        (c.target_ids[0] == other_point_id && c.target_ids[1] == point_id);
    if (same_pair) {
      refresh_sketch_derived_state(feature);
      return;
    }
  }

  parameters.constraints.push_back(SketchConstraint{
      .constraint_id = next_constraint_id(parameters, "constraint-symmetric-"),
      .kind = "symmetric",
      .target_ids = {point_id, other_point_id, axis_line_id},
  });

  refresh_sketch_with_append_focus(feature, {point_id, other_point_id, axis_line_id});
}

void set_sketch_midpoint_constraint(FeatureEntry& feature,
                                    const std::string& vertex_id,
                                    const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set midpoint constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  if (!point_position_of(parameters, vertex_id).has_value()) {
    throw std::runtime_error("Sketch point not found: " + vertex_id);
  }
  const auto& host = require_line(parameters, line_id);
  if (vertex_id == host.start_vertex_id || vertex_id == host.end_vertex_id) {
    throw std::runtime_error(
        "Midpoint constraint cannot anchor a line to its own endpoint");
  }

  for (const auto& c : parameters.constraints) {
    if (c.kind == "midpoint" && c.target_ids.size() == 2 &&
        c.target_ids[0] == vertex_id && c.target_ids[1] == line_id) {
      refresh_sketch_derived_state(feature);
      return;
    }
  }

  parameters.constraints.push_back(SketchConstraint{
      .constraint_id = next_constraint_id(parameters, "constraint-midpoint-"),
      .kind = "midpoint",
      .target_ids = {vertex_id, line_id},
  });

  // Focus only the anchored VERTEX: the host line stays frozen during
  // the creation solve so the whole group cannot wander its joint
  // translation null space.
  refresh_sketch_with_append_focus(feature, {vertex_id});
}

void set_sketch_collinear_constraint(FeatureEntry& feature,
                                     const std::string& line_id,
                                     const std::string& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set collinear constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  require_line(parameters, line_id);
  require_line(parameters, other_line_id);
  if (line_id == other_line_id) {
    throw std::runtime_error("Collinear constraint requires two distinct lines");
  }

  if (relation_exists(parameters, "collinear", line_id, other_line_id)) {
    refresh_sketch_derived_state(feature);
    return;
  }

  // Collinear supersedes an existing parallel relation between the pair.
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& r) {
                       return r.kind == "parallel" &&
                              ((r.first_line_id == line_id &&
                                r.second_line_id == other_line_id) ||
                               (r.first_line_id == other_line_id &&
                                r.second_line_id == line_id));
                     }),
      parameters.line_relations.end());

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-collinear-" + line_id + "-" + other_line_id,
      .kind = "collinear",
      .first_line_id = line_id,
      .second_line_id = other_line_id,
  });

  // Focus only the DRIVEN line: the reference line stays frozen during
  // the creation solve so the pair cannot wander its joint-translation
  // null space (the reference's position is not part of the relation).
  refresh_sketch_with_append_focus(feature, {line_id});
}

// Tangency between two curves (circle/circle, circle/arc, arc/arc) or
// two lines.  The relation/constraint is re-derived on every refresh;
// internal/external tangency is captured at creation from the current
// geometry and preserved across solves.
void set_sketch_tangent_pair_constraint(FeatureEntry& feature,
                                        const std::string& first_id,
                                        const std::string& second_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set tangent constraints");
  }

  auto& parameters = feature.sketch_parameters.value();

  const bool first_is_line =
      std::any_of(parameters.lines.begin(), parameters.lines.end(),
                  [&](const SketchLine& l) { return l.id == first_id; });
  const bool second_is_line =
      std::any_of(parameters.lines.begin(), parameters.lines.end(),
                  [&](const SketchLine& l) { return l.id == second_id; });

  if (first_is_line && second_is_line) {
    // Line-line tangency = collinear through the shared corner.
    if (relation_exists(parameters, "tangent_line_line", first_id, second_id)) {
      refresh_sketch_derived_state(feature);
      return;
    }
    require_line(parameters, first_id);
    require_line(parameters, second_id);
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-tangent-lines-" + first_id + "-" + second_id,
        .kind = "tangent_line_line",
        .first_line_id = first_id,
        .second_line_id = second_id,
    });
    refresh_sketch_with_append_focus(feature, {first_id, second_id});
    return;
  }

  if (first_is_line || second_is_line) {
    throw std::runtime_error(
        "Mixed line/curve tangency must use set_sketch_tangent_constraint");
  }

  const auto first_curve = curve_center_radius(parameters, first_id);
  const auto second_curve = curve_center_radius(parameters, second_id);
  if (!first_curve.has_value()) {
    throw std::runtime_error("Sketch circle or arc not found: " + first_id);
  }
  if (!second_curve.has_value()) {
    throw std::runtime_error("Sketch circle or arc not found: " + second_id);
  }

  if (pair_constraint_exists(parameters, "tangent_circle_circle", first_id,
                             second_id) ||
      pair_constraint_exists(parameters, "tangent_arc_arc", first_id,
                             second_id)) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const double dx = std::get<0>(second_curve.value()) - std::get<0>(first_curve.value());
  const double dy = std::get<1>(second_curve.value()) - std::get<1>(first_curve.value());
  const double center_dist = std::hypot(dx, dy);
  const double r1 = std::get<2>(first_curve.value());
  const double r2 = std::get<2>(second_curve.value());
  // Internal tangency when one curve sits inside the other.
  const bool internal = center_dist < std::abs(r1 - r2);

  const bool any_arc =
      std::get<3>(first_curve.value()) == "arc" ||
      std::get<3>(second_curve.value()) == "arc";
  const std::string kind = any_arc ? "tangent_arc_arc" : "tangent_circle_circle";

  parameters.constraints.push_back(SketchConstraint{
      .constraint_id = next_constraint_id(parameters, "constraint-tangent-"),
      .kind = kind,
      .target_ids = {first_id, second_id},
      .value = internal ? 1.0 : 0.0,
  });

  refresh_sketch_with_append_focus(feature, {first_id, second_id});
}
