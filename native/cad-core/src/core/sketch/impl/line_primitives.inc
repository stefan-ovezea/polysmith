void add_sketch_line(FeatureEntry& feature,
                     int line_index,
                     double start_x,
                     double start_y,
                     double end_x,
                     double end_y,
                     bool is_construction,
                     bool snap_start) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch lines");
  }

  validate_line(start_x, start_y, end_x, end_y);

  const auto shared_start_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", end_x, end_y);

  feature.sketch_parameters->lines.push_back(SketchLine{
      .id = "line-" + std::to_string(line_index),
      .start_point_id = shared_start_point.has_value()
                            ? std::get<0>(shared_start_point.value())
                            : "point-line-" + std::to_string(line_index) + "-start",
      .end_point_id = shared_end_point.has_value()
                          ? std::get<0>(shared_end_point.value())
                          : "point-line-" + std::to_string(line_index) + "-end",
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .constraint = infer_constraint_hint(start_x, start_y, end_x, end_y),
      .is_construction = is_construction,
  });
  auto& line = feature.sketch_parameters->lines.back();
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
    });
    // Line-angle dimension: the absolute angle from the positive X axis
    // (horizontal). Stored in radians. Axis-constrained lines (rectangle
    // sides, Shift-locked lines) get a *driven* (reference-only) angle
    // dimension so the angle is still displayed but cannot be edited.
    const bool constrained_axis =
        line.constraint.has_value() &&
        (line.constraint.value() == "horizontal" ||
         line.constraint.value() == "vertical");
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-line-angle-" + line.id,
        .kind = "line_angle",
        .entity_id = line.id,
        .value = std::atan2(line.end_y - line.start_y,
                            line.end_x - line.start_x),
        .driven = constrained_axis,
        .is_auto = true,
    });
  }
  if (!is_construction) {
    run_inference_on_new_line(*feature.sketch_parameters,
                              feature.sketch_parameters->lines.back());
  }
  refresh_sketch_derived_state(feature);
}

void set_sketch_line_construction(FeatureEntry& feature,
                                  const std::string& line_id,
                                  bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can toggle construction lines");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& line = require_line(parameters, line_id);
  if (line.is_construction == is_construction) {
    return;
  }
  line.is_construction = is_construction;

  // Keep the dimension list in sync with the new role: solid lines
  // get driving length + angle dimensions, construction lines lose
  // theirs since they're reference-only. Existing user-applied
  // dimensions on a line are preserved across role changes; we only
  // manage the automatic entries created at construction time.
  const std::string auto_dim_id = "dim-line-" + line.id;
  const std::string auto_angle_dim_id = "dim-line-angle-" + line.id;
  const auto auto_dim_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == auto_dim_id; });
  const auto auto_angle_dim_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == auto_angle_dim_id; });
  if (is_construction) {
    if (auto_dim_it != parameters.dimensions.end()) {
      parameters.dimensions.erase(auto_dim_it);
    }
    if (auto_angle_dim_it != parameters.dimensions.end()) {
      parameters.dimensions.erase(auto_angle_dim_it);
    }
  } else {
    if (auto_dim_it == parameters.dimensions.end()) {
      parameters.dimensions.push_back(SketchDimension{
          .id = auto_dim_id,
          .kind = "line_length",
          .entity_id = line.id,
          .value = measure_line_length(line),
          .is_auto = true,
      });
    }
    if (auto_angle_dim_it == parameters.dimensions.end()) {
      const bool constrained_axis =
          line.constraint.has_value() &&
          (line.constraint.value() == "horizontal" ||
           line.constraint.value() == "vertical");
      parameters.dimensions.push_back(SketchDimension{
          .id = auto_angle_dim_id,
          .kind = "line_angle",
          .entity_id = line.id,
          .value = std::atan2(line.end_y - line.start_y,
                              line.end_x - line.start_x),
          .driven = constrained_axis,
          .is_auto = true,
      });
    }
  }

  refresh_sketch_derived_state(feature);
}

void set_sketch_midpoint_anchor(FeatureEntry& feature,
                                const std::string& point_id,
                                const std::string& host_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold midpoint anchors");
  }
  auto& parameters = *feature.sketch_parameters;

  // Drop any pre-existing anchor for the same point so the user can
  // re-target without leaving stale relations behind.
  parameters.midpoint_anchors.erase(
      std::remove_if(
          parameters.midpoint_anchors.begin(),
          parameters.midpoint_anchors.end(),
          [&](const SketchMidpointAnchor& anchor) {
            return anchor.point_id == point_id;
          }),
      parameters.midpoint_anchors.end());

  if (host_line_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto host_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == host_line_id; });
  if (host_it == parameters.lines.end()) {
    // The host line was deleted between snap-time and commit-time.
    // Silently skip — there is nothing to anchor to.
    return;
  }

  // Use a stable id derived from the bound point so the relation
  // round-trips through serialization without churning.
  parameters.midpoint_anchors.push_back(SketchMidpointAnchor{
      .id = "midpoint-anchor-" + point_id,
      .point_id = point_id,
      .line_id = host_line_id,
  });

  refresh_sketch_derived_state(feature);
}

// Anchor a point to the body of a host line at a specific parametric
// position `t` in [0, 1]. Used by the line-body snap to make the
// bound point ride with the host as it moves. Mirrors
// `set_sketch_midpoint_anchor` byte-for-byte except it also accepts
// (and clamps) the `t` value and stores it on the anchor record.
void set_sketch_point_line_anchor(FeatureEntry& feature,
                                  const std::string& point_id,
                                  const std::string& host_line_id,
                                  double t) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold point-line anchors");
  }
  auto& parameters = *feature.sketch_parameters;

  // Drop any pre-existing anchor for the same point. Also clear any
  // midpoint anchor on the same point so the two relations don't
  // fight each other (a midpoint anchor is a degenerate point-line
  // anchor at t=0.5; the explicit point-line anchor wins).
  parameters.point_line_anchors.erase(
      std::remove_if(
          parameters.point_line_anchors.begin(),
          parameters.point_line_anchors.end(),
          [&](const SketchPointLineAnchor& anchor) {
            return anchor.point_id == point_id;
          }),
      parameters.point_line_anchors.end());
  parameters.midpoint_anchors.erase(
      std::remove_if(
          parameters.midpoint_anchors.begin(),
          parameters.midpoint_anchors.end(),
          [&](const SketchMidpointAnchor& anchor) {
            return anchor.point_id == point_id;
          }),
      parameters.midpoint_anchors.end());

  if (host_line_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto host_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == host_line_id; });
  if (host_it == parameters.lines.end()) {
    // The host line was deleted between snap-time and commit-time.
    // Silently skip — there is nothing to anchor to.
    return;
  }

  // Clamp `t` so out-of-range UI input can never produce an anchor
  // that re-projects past the segment's endpoints.
  const double clamped_t = std::max(0.0, std::min(1.0, t));

  parameters.point_line_anchors.push_back(SketchPointLineAnchor{
      .id = "point-line-anchor-" + point_id,
      .point_id = point_id,
      .line_id = host_line_id,
      .t = clamped_t,
  });

  refresh_sketch_derived_state(feature);
}

void add_sketch_rectangle(FeatureEntry& feature,
                          int& next_line_index,
                          double start_x,
                          double start_y,
                          double end_x,
                          double end_y,
                          bool is_construction) {
  // Stash the indices used for each side so we can build the
  // line-id pair for the equal-length relations after the loop.
  // `add_sketch_line` itself reads the line id from `line-{index}`,
  // so the only way to know the resulting ids is to capture the
  // counter values we passed in.
  const int top_index = next_line_index++;
  const int right_index = next_line_index++;
  const int bottom_index = next_line_index++;
  const int left_index = next_line_index++;

  add_sketch_line(feature, top_index, start_x, start_y, end_x, start_y,
                  is_construction);
  add_sketch_line(feature, right_index, end_x, start_y, end_x, end_y,
                  is_construction);
  add_sketch_line(feature, bottom_index, end_x, end_y, start_x, end_y,
                  is_construction);
  add_sketch_line(feature, left_index, start_x, end_y, start_x, start_y,
                  is_construction);

  // H/V constraints on each side are already inferred by
  // `add_sketch_line` (via `infer_constraint_hint`) because the
  // sides are exactly axis-aligned by construction. What's missing
  // is the equal-length pairing: top↔bottom and left↔right. Adding
  // these two relations means editing one side's length dimension
  // also updates its mirror, matching common CAD workflow's behavior.
  if (!feature.sketch_parameters.has_value()) {
    return;
  }
  auto& parameters = *feature.sketch_parameters;
  const std::string top_id = "line-" + std::to_string(top_index);
  const std::string right_id = "line-" + std::to_string(right_index);
  const std::string bottom_id = "line-" + std::to_string(bottom_index);
  const std::string left_id = "line-" + std::to_string(left_index);

  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(),
                     parameters.dimensions.end(),
                     [&](const SketchDimension& dimension) {
                       return dimension.kind == "line_angle" &&
                              (dimension.entity_id == top_id ||
                               dimension.entity_id == right_id ||
                               dimension.entity_id == bottom_id ||
                               dimension.entity_id == left_id);
                     }),
      parameters.dimensions.end());

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + top_id,
      .kind = "equal_length",
      .first_line_id = top_id,
      .second_line_id = bottom_id,
  });
  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + left_id,
      .kind = "equal_length",
      .first_line_id = left_id,
      .second_line_id = right_id,
  });
  refresh_sketch_derived_state(feature);
}

// Reflect a 2D point across the line passing through (ax, ay) and
// (bx, by). Standard formula: project P onto the line to find the
// foot F, then P' = 2F - P. Returns NaN-free results as long as
// the line has non-zero length, which `validate_line` guarantees
