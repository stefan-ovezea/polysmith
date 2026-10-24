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

  // Snapshot the WHOLE sketch: a refusal (or an enforcement failure) can have
  // moved vertices through propagation before the refresh, and restoring only
  // the driven line left the rest of the sketch sheared.
  const SolverGeometrySnapshot sketch_before = capture_solver_geometry(parameters);

  // Save the driven line's full state so we can roll back completely
  // if enforcement fails.
  const auto saved_line_constraint = line.constraint;
  const double saved_line_start_x = line.start_x;
  const double saved_line_start_y = line.start_y;
  const double saved_line_end_x = line.end_x;
  const double saved_line_end_y = line.end_y;

  // Clear the driven line's H/V constraint only when the new relation still has
  // to MOVE this line — an axis statement and a parallel relation can disagree,
  // and then the axis wins unless it goes. When the lines are already parallel
  // there is nothing to move, and wiping the axis would delete a real constraint
  // (a rectangle's bottom side is horizontal AND already parallel to its top
  // side).
  if (!lines_are_parallel(line, other_line)) {
    line.constraint = std::nullopt;
  }

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
          parameters, line.start_vertex_id, line.start_x, line.start_y);
    }

    if (!points_match(saved_line_end_x, saved_line_end_y,
                      line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_vertex_id, line.end_x, line.end_y);
    }

    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
    sync_all_line_dimensions(parameters);
    const int rejections_before = feature.sketch_parameters->solver_rejections;
    refresh_sketch_with_append_focus(
        feature, {line_id, other_line_id.value()});

    // Refuse a relation the solver can only satisfy by destroying geometry.
    // A contradictory pair makes planegcs collapse a line (a zero-length line is
    // trivially parallel AND perpendicular to everything), and the collapsed-line
    // cleanup then deletes it together with its relations. Rejecting the solve
    // marks the sketch solver-failed; throwing here rolls the relation back (the
    // catch restores the geometry) so the user is told the truth instead of
    // silently losing a side of the sketch.
    {
      auto& params = *feature.sketch_parameters;
      const bool rejected =
          params.solver_rejections != rejections_before ||
          params.solver_conflicting_count > 0 ||
          params.solver_redundant_count > 0 ||
          (params.solver_dofs != -1 && params.solver_dofs < 0);
      if (rejected) {
        throw std::runtime_error(std::string("Cannot make these lines ") +
                                 "parallel" +
                                 ": the constraint cannot be satisfied "
                                 "without destroying the sketch");
      }
    }
  } catch (...) {
    // Roll back completely: remove the relation, restore the driven
    // line's endpoints, and restore its original constraint.
    remove_line_relations_for_line(parameters, "parallel", line_id);
    line.start_x = saved_line_start_x;
    line.start_y = saved_line_start_y;
    line.end_x = saved_line_end_x;
    line.end_y = saved_line_end_y;
    line.constraint = saved_line_constraint;
    restore_all_solver_geometry(parameters, sketch_before);
    try {
      refresh_sketch_derived_state(feature);
    } catch (const std::exception&) {
      // Reporting the original refusal matters more than a refresh hiccup.
    }
    throw;
  }
}
