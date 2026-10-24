void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set line constraints");
  }

  validate_constraint(constraint);

  // Glyph segments are derived geometry — they carry no user-editable
  // constraints.
  require_user_line(*feature.sketch_parameters, line_id);

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  // A rectangle's sides are horizontal/vertical because of its parallel and
  // perpendicular relations, so applying the axis constraint on top of that adds
  // a duplicate. Skip it: the relations already show the glyph, and the
  // duplicate is what made the DOF badge drop from 4 to 1 after "Horizontal".
  if (constraint.has_value() &&
      line_it->constraint != constraint &&
      axis_constraint_is_implied(*feature.sketch_parameters, line_id,
                                 constraint.value())) {
    polysmith::core::log_info(
        "sketch", constraint.value() + " on " + line_id +
                      " is already implied by its parallel/perpendicular "
                      "relation; not stored as a duplicate");
    return;
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;
  line_it->constraint = constraint;
  // Reset driven flag before re-evaluating — constraints are re-checked
  // against the solver on every application.
  line_it->constraint_driven = false;
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
                                   line_it->start_vertex_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_vertex_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_with_append_focus(feature, {line_id});

  // If the new constraint over-constrains the system, mark it as driven
  // so the solver ignores it and the UI renders it in parentheses.
  if (line_it->constraint.has_value()) {
    auto& params = *feature.sketch_parameters;
    // D-009: solver_dofs == -1 means the solver has NOT RUN YET (it is the
    // default), which is not the same as over-constrained. `finalize_new_
    // dimension` documents the same trap and guards it the same way; this path
    // did not, so the FIRST axis constraint on a loose line was demoted to
    // driven and silently failed to straighten it. -1 stays "unknown": nothing
    // to demote, and the DOF badge keeps its own conventions.
    const bool solver_ran = params.solver_dofs != -1;
    if (solver_ran &&
        (params.solver_conflicting_count > 0 ||
         params.solver_redundant_count > 0 || params.solver_dofs < 0 ||
         !params.solver_ok)) {
      line_it->constraint_driven = true;
      polysmith::core::log_info(
          "sketch", "constraint on " + line_id +
          " marked driven (solver reported over-constrained)");
    }
  }
}
