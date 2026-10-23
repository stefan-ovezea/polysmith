void add_sketch_line(FeatureEntry &feature, int line_index, double start_x,
                     double start_y, double end_x, double end_y,
                     bool is_construction, bool snap_start,
                     bool auto_dimensions, bool refresh_immediately,
                     bool infer_constraints, bool share_endpoints,
                     const std::string &line_type) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch lines");
  }

  validate_line(start_x, start_y, end_x, end_y);

  // Imported geometry opts out of endpoint coalescing: see the parameter's
  // comment.  An empty match list leaves the raw coordinates untouched.
  const auto shared_start_point =
      share_endpoints ? find_coincident_endpoint(*feature.sketch_parameters, "",
                                                 start_x, start_y)
                      : std::nullopt;
  const auto shared_end_point =
      share_endpoints ? find_coincident_endpoint(*feature.sketch_parameters, "",
                                                 end_x, end_y)
                      : std::nullopt;

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
      .is_construction = is_construction || !line_type.empty(),
      .line_type = line_type,
  });
  auto &line = feature.sketch_parameters->lines.back();
  if (infer_constraints) {
    apply_line_constraint(line);
  }
  // The start point of user‑drawn lines must not be auto‑merged with
  // nearby geometry (see the phantom‑line bug in the impl log).
  // Rectangle / polygon / mirror lines still merge corners by default.
  if (share_endpoints) {
    snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, line,
                                               snap_start);
  }
  // Endpoint snapping can COLLAPSE a segment: when both ends land on the same
  // existing vertex (a segment shorter than the 0.01 mm coincidence tolerance),
  // the stored line has zero length and every later validation of that line
  // throws.  Throwing here aborted an entire import over one degenerate step —
  // a Fusion sketch export is thousands of 0.005 mm spline steps, so that is
  // the common case, not the exotic one.  Drop the line instead: the rest of
  // the sketch is fine, and the caller asked for one segment, not for a state
  // it cannot represent.
  if (measure_line_length(line) <= kMinimumSketchDimensionValue) {
    log_warn("sketch",
             "dropped a line that collapsed to zero length after endpoint "
             "snapping (segment shorter than the 0.01 mm coincidence "
             "tolerance)");
    feature.sketch_parameters->lines.pop_back();
    return;
  }
  // Construction lines are reference geometry; they don't get a
  // driving length dimension automatically. The user can still apply
  // one explicitly via the dimension tool if they want. Projected
  // geometry (auto_dimensions = false) skips the automatic dimensions
  // too — its endpoints are fixed, so driving dimensions would only
  // over-constrain the solver and flood the sketch with clutter.
  // Reference-line flavours ("centerline" / "infinite") are construction
  // geometry too: everything below keys off the effective flag so a
  // flavoured line never receives automatic dimensions or inference.
  const bool effective_construction = is_construction || !line_type.empty();
  if (!effective_construction && auto_dimensions) {
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
  if (!effective_construction && refresh_immediately) {
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
