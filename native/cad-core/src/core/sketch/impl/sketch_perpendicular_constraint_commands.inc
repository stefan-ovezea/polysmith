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
    refresh_sketch_with_append_focus(
        feature, {line_id, other_line_id.value()});
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
