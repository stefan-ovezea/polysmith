void add_sketch_line(FeatureEntry &feature, int line_index, double start_x,
                     double start_y, double end_x, double end_y,
                     bool is_construction, bool snap_start,
                     bool auto_dimensions, bool refresh_immediately,
                     bool infer_constraints) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch lines");
  }

  validate_line(start_x, start_y, end_x, end_y);

  const auto shared_start_point = find_coincident_endpoint(
      *feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", end_x, end_y);

  auto& params = *feature.sketch_parameters;
  // When the endpoint snaps to an existing point (projected vertex,
  // line endpoint, etc.), use the matched point's exact coordinates
  // instead of the caller's values.  This prevents sub-tolerance drift
  // that would otherwise surface as deformed geometry on reload.
  const double snapped_start_x =
      shared_start_point.has_value() ? std::get<1>(shared_start_point.value()) : start_x;
  const double snapped_start_y =
      shared_start_point.has_value() ? std::get<2>(shared_start_point.value()) : start_y;
  const double snapped_end_x =
      shared_end_point.has_value() ? std::get<1>(shared_end_point.value()) : end_x;
  const double snapped_end_y =
      shared_end_point.has_value() ? std::get<2>(shared_end_point.value()) : end_y;

  feature.sketch_parameters->lines.push_back(SketchLine{
      .id = "line-" + std::to_string(line_index),
      .start_vertex_id =
          shared_start_point.has_value()
              ? std::get<0>(shared_start_point.value())
              : "vertex-" + std::to_string(params.next_vertex_index++),
      .end_vertex_id = shared_end_point.has_value()
                          ? std::get<0>(shared_end_point.value())
                          : "vertex-" + std::to_string(params.next_vertex_index++),
      .start_x = snapped_start_x,
      .start_y = snapped_start_y,
      .end_x = snapped_end_x,
      .end_y = snapped_end_y,
      .constraint = infer_constraints
                        ? infer_constraint_hint(snapped_start_x, snapped_start_y,
                                                snapped_end_x, snapped_end_y)
                        : std::optional<std::string>(),
      .is_construction = is_construction,
  });
  auto &line = feature.sketch_parameters->lines.back();
  if (infer_constraints) {
    apply_line_constraint(line);
  }
  // The start point of user‑drawn lines must not be auto‑merged with
  // nearby geometry (see the phantom‑line bug in the impl log).
  // Rectangle / polygon / mirror lines still merge corners by default.
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, line,
                                             snap_start);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
  // Construction lines are reference geometry; they don't get a
  // driving length dimension automatically. The user can still apply
  // one explicitly via the dimension tool if they want. Projected
  // geometry (auto_dimensions = false) skips the automatic dimensions
  // too — its endpoints are fixed, so driving dimensions would only
  // over-constrain the solver and flood the sketch with clutter.
  if (!is_construction && auto_dimensions) {
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
  // Bulk inserters (projection outlines) skip the per-entity inference
  // and refresh — they refresh once at the end with the full id set.
  if (!is_construction && refresh_immediately) {
    run_inference_on_new_line(*feature.sketch_parameters,
                              feature.sketch_parameters->lines.back());
  }
  if (refresh_immediately) {
    refresh_sketch_with_append_focus(feature, {line.id});
  }
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
