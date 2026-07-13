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
    if (params.solver_conflicting_count > 0 ||
        params.solver_redundant_count > 0 ||
        params.solver_dofs < 0) {
      line_it->constraint_driven = true;
      fprintf(stderr, "[INFO] sketch: constraint on %s marked driven "
              "(solver reported over-constrained)\n", line_id.c_str());
    }
  }
}
