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
