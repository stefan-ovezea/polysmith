void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch lines");
  }

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;

  line_it->start_x = start_x;
  line_it->start_y = start_y;
  line_it->end_x = end_x;
  line_it->end_y = end_y;
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void update_sketch_point(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch points");
  }

  auto& parameters = feature.sketch_parameters.value();
  const SketchPoint* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_point(parameters, point_id);

  if (point->kind == "center") {
    const std::string prefix = "point-circle-";
    const std::string suffix = "-center";
    if (point_id.rfind(prefix, 0) != 0 ||
        point_id.size() <= prefix.size() + suffix.size() ||
        point_id.substr(point_id.size() - suffix.size()) != suffix) {
      throw std::runtime_error("Unsupported sketch center point: " + point_id);
    }

    const std::string circle_id =
        point_id.substr(prefix.size(),
                        point_id.size() - prefix.size() - suffix.size());
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) { return circle.id == circle_id; });
    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for point: " + point_id);
    }

    circle_it->center_x = x;
    circle_it->center_y = y;
    refresh_sketch_derived_state(feature);
    return;
  }

  propagate_connected_point_move(parameters, point_id, x, y);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set line constraints");
  }

  validate_constraint(constraint);

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;
  line_it->constraint = constraint;
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_equal_length_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set equal-length constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "equal_length", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be equal-length to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());

  // Canonical ID: sorted pair so the same two lines always get the
  // same relation id regardless of which is passed as `line_id`.
  const std::string el_a = std::min(line_id, other_line_id.value());
  const std::string el_b = std::max(line_id, other_line_id.value());

  // Remove any pre-existing relation between this pair (regardless of
  // order or id) before inserting — guards against file-load duplicates.
  remove_line_relations_for_line(parameters, "equal_length", other_line_id.value());

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + el_a + "-" + el_b,
      .kind = "equal_length",
      .first_line_id = line_id,
      .second_line_id = other_line_id.value(),
  });

  const double target_length = measure_line_length(other_line);
  const double previous_end_x = line.end_x;
  const double previous_end_y = line.end_y;
  drive_line_length_respecting_fixed_points(parameters, line, target_length);
  snap_line_endpoints_to_coincident_geometry(parameters, line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

  if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
    propagate_connected_point_move(
        parameters, line.end_point_id, line.end_x, line.end_y);
  }

  sync_all_line_dimensions(parameters);
  enforce_equal_length_relations(parameters, line_id);
  enforce_perpendicular_relations(parameters, line_id);
  enforce_parallel_relations(parameters, line_id);
  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_perpendicular_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set perpendicular constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "perpendicular", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be perpendicular to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());

  // Save the driven line's full state so we can roll back completely
  // if enforcement fails.
  const auto saved_line_constraint = line.constraint;
  const double saved_line_start_x = line.start_x;
  const double saved_line_start_y = line.start_y;
  const double saved_line_end_x = line.end_x;
  const double saved_line_end_y = line.end_y;

  // Clear the driven line's H/V constraint — it's incompatible with
  // perpendicular. The reference line keeps its own constraints;
  // enforcement will skip it silently (see drive_line_perpendicular_to_reference).
  line.constraint = std::nullopt;

  {
    const std::string pa = std::min(line_id, other_line_id.value());
    const std::string pb = std::max(line_id, other_line_id.value());
    remove_line_relations_for_line(parameters, "perpendicular", other_line_id.value());
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-perpendicular-" + pa + "-" + pb,
        .kind = "perpendicular",
        .first_line_id = line_id,
        .second_line_id = other_line_id.value(),
    });
  }

  try {
    drive_line_perpendicular_to_reference(line, other_line, parameters);
    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    if (!points_match(saved_line_start_x, saved_line_start_y,
                      line.start_x, line.start_y)) {
      propagate_connected_point_move(
          parameters, line.start_point_id, line.start_x, line.start_y);
    }

    if (!points_match(saved_line_end_x, saved_line_end_y,
                      line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
  } catch (...) {
    // Roll back completely: remove the relation, restore the driven
    // line's endpoints, and restore its original constraint.
    remove_line_relations_for_line(parameters, "perpendicular", line_id);
    line.start_x = saved_line_start_x;
    line.start_y = saved_line_start_y;
    line.end_x = saved_line_end_x;
    line.end_y = saved_line_end_y;
    line.constraint = saved_line_constraint;
    throw;
  }
}

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

  refresh_sketch_derived_state(feature);
}

void set_sketch_parallel_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set parallel constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "parallel", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be parallel to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());

  // Save the driven line's full state so we can roll back completely
  // if enforcement fails.
  const auto saved_line_constraint = line.constraint;
  const double saved_line_start_x = line.start_x;
  const double saved_line_start_y = line.start_y;
  const double saved_line_end_x = line.end_x;
  const double saved_line_end_y = line.end_y;

  // Clear the driven line's H/V constraint — it's incompatible with
  // parallel. The reference line keeps its own constraints;
  // enforcement will skip it silently.
  line.constraint = std::nullopt;

  {
    const std::string pa = std::min(line_id, other_line_id.value());
    const std::string pb = std::max(line_id, other_line_id.value());
    remove_line_relations_for_line(parameters, "parallel", other_line_id.value());
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-parallel-" + pa + "-" + pb,
        .kind = "parallel",
        .first_line_id = line_id,
        .second_line_id = other_line_id.value(),
    });
  }

  try {
    drive_line_parallel_to_reference(line, other_line, parameters);
    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    if (!points_match(saved_line_start_x, saved_line_start_y,
                      line.start_x, line.start_y)) {
      propagate_connected_point_move(
          parameters, line.start_point_id, line.start_x, line.start_y);
    }

    if (!points_match(saved_line_end_x, saved_line_end_y,
                      line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
  } catch (...) {
    // Roll back completely: remove the relation, restore the driven
    // line's endpoints, and restore its original constraint.
    remove_line_relations_for_line(parameters, "parallel", line_id);
    line.start_x = saved_line_start_x;
    line.start_y = saved_line_start_y;
    line.end_x = saved_line_end_x;
    line.end_y = saved_line_end_y;
    line.constraint = saved_line_constraint;
    throw;
  }
}

void clear_sketch_line_constraints(FeatureEntry& feature,
                                   const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can clear line constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  // Clear the inline H/V constraint.
  line.constraint = std::nullopt;

  // Clear every line‑line relation that involves this line.
  remove_line_relations_for_line(parameters, "equal_length", line_id);
  remove_line_relations_for_line(parameters, "perpendicular", line_id);
  remove_line_relations_for_line(parameters, "parallel", line_id);
  remove_line_relations_for_line(parameters, "tangent_line_circle", line_id);

  // Clear midpoint and point‑line anchors that reference this line.
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& a) {
                       return a.line_id == line_id;
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& a) {
                       return a.line_id == line_id;
                     }),
      parameters.point_line_anchors.end());

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}
