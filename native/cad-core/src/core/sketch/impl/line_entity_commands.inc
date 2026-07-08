void add_sketch_line(FeatureEntry &feature, int line_index, double start_x,
                     double start_y, double end_x, double end_y,
                     bool is_construction, bool snap_start) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch lines");
  }

  validate_line(start_x, start_y, end_x, end_y);

  const auto shared_start_point = find_coincident_endpoint(
      *feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", end_x, end_y);

  auto& params = *feature.sketch_parameters;
  feature.sketch_parameters->lines.push_back(SketchLine{
      .id = "line-" + std::to_string(line_index),
      .start_point_id =
          shared_start_point.has_value()
              ? std::get<0>(shared_start_point.value())
              : "vertex-" + std::to_string(params.next_vertex_index++),
      .end_point_id = shared_end_point.has_value()
                          ? std::get<0>(shared_end_point.value())
                          : "vertex-" + std::to_string(params.next_vertex_index++),
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .constraint = infer_constraint_hint(start_x, start_y, end_x, end_y),
      .is_construction = is_construction,
  });
  auto &line = feature.sketch_parameters->lines.back();
  apply_line_constraint(line);
  // The start point of user‑drawn lines must not be auto‑merged with
  // nearby geometry (see the phantom‑line bug in the impl log).
  // Rectangle / polygon / mirror lines still merge corners by default.
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, line,
                                             snap_start);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
  // Construction lines are reference geometry; they don't get a
  // driving length dimension automatically. The user can still apply
  // one explicitly via the dimension tool if they want.
  if (!is_construction) {
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-line-" + line.id,
        .kind = "line_length",
        .entity_id = line.id,
        .value = measure_line_length(line),
        .is_auto = true,
    });
    // Line-angle dimension: the absolute angle from the positive X axis
    // (horizontal). Stored in radians. Axis-constrained lines (rectangle
    // sides, Shift-locked lines) get a *driven* (reference-only) angle
    // dimension so the angle is still displayed but cannot be edited.
    const bool constrained_axis = line.constraint.has_value() &&
                                  (line.constraint.value() == "horizontal" ||
                                   line.constraint.value() == "vertical");
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-line-angle-" + line.id,
        .kind = "line_angle",
        .entity_id = line.id,
        .value =
            std::atan2(line.end_y - line.start_y, line.end_x - line.start_x),
        .driven = constrained_axis,
        .is_auto = true,
    });
  }
  if (!is_construction) {
    run_inference_on_new_line(*feature.sketch_parameters,
                              feature.sketch_parameters->lines.back());
  }
  refresh_sketch_with_append_focus(feature, {line.id});
}

void set_sketch_line_construction(FeatureEntry &feature,
                                  const std::string &line_id,
                                  bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can toggle construction lines");
  }

  auto &parameters = *feature.sketch_parameters;
  auto &line = require_line(parameters, line_id);
  if (line.is_construction == is_construction) {
    return;
  }
  line.is_construction = is_construction;

  // Construction is purely a visual / profile-exclusion toggle.
  // Dimensions and constraints are preserved exactly as-is — they
  // keep their driving/driven status regardless of the construction
  // flag (Fusion 360 behaviour).

  refresh_sketch_derived_state(feature);
}
