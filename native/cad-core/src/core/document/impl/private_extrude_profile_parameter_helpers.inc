std::optional<ExtrudeFeatureParameters> make_extrude_parameters_for_profile(
    const FeatureEntry& sketch_feature,
    const SketchProfileRegion& profile,
    double depth) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  if (profile.kind == "polygon") {
    ExtrudeFeatureParameters params{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "polygon",
        .start_x = 0.0,
        .start_y = 0.0,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = profile.points,
        .inner_loops = profile.inner_loops,
        .depth = depth,
    };

    if (!profile.ordered_edge_ids.empty()) {
      params.sketch_edge_ids = profile.ordered_edge_ids;
      // Copy the sketch geometry that the wire builder needs so it can
      // reconstruct exact OCCT curves (lines → GC_MakeSegment, arcs →
      // GC_MakeArcOfCircle, circles → gp_Circ) from the profile boundary.
      for (const auto& line : sketch.lines) {
        if (!line.is_construction) params.sketch_lines.push_back(line);
      }
      for (const auto& arc : sketch.arcs) {
        if (!arc.is_construction) params.sketch_arcs.push_back(arc);
      }
      for (const auto& circle : sketch.circles) {
        if (!circle.is_construction) params.sketch_circles.push_back(circle);
      }
    }

    // Carry exact arc descriptors so the wire builder never guesses.
    params.arc_descriptors = profile.arc_descriptors;

    return params;
  }

  if (profile.kind == "circle") {
    ExtrudeFeatureParameters params{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "circle",
        .start_x = profile.center_x,
        .start_y = profile.center_y,
        .width = 0.0,
        .height = 0.0,
        .radius = profile.radius,
        .profile_points = {},
        .depth = depth,
    };

    // Carry the sketch geometry so that cut/join modes can route this
    // circle through the wire path (matching the target body's
    // construction method for OCCT 8 boolean compatibility).
    if (profile.source_circle_id.has_value()) {
      for (const auto& circle : sketch.circles) {
        if (circle.id == profile.source_circle_id.value() &&
            !circle.is_construction) {
          params.sketch_circles.push_back(circle);
          params.sketch_edge_ids.push_back(circle.id);
          break;
        }
      }
      for (const auto& line : sketch.lines) {
        if (!line.is_construction) params.sketch_lines.push_back(line);
      }
      for (const auto& arc : sketch.arcs) {
        if (!arc.is_construction) params.sketch_arcs.push_back(arc);
      }
    }

    return params;
  }

  return std::nullopt;
}

std::optional<ExtrudeFeatureParameters> make_extrude_parameters_for_profiles(
    const FeatureEntry& sketch_feature,
    const std::vector<SketchProfileRegion>& profiles,
    double depth) {
  if (profiles.empty() || !sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }
  if (profiles.size() == 1) {
    return make_extrude_parameters_for_profile(sketch_feature,
                                               profiles.front(),
                                               depth);
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const auto& first = profiles.front();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;
  std::vector<std::string> ids;
  for (const auto& profile : profiles) {
    ids.push_back(profile.id);
  }

  ExtrudeFeatureParameters parameters{
      .sketch_feature_id = sketch_feature.id,
      .profile_id = ids.front(),
      .profile_ids = ids,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = "polygon",
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .profile_points = profile_boundary_points(first),
      .inner_loops = first.inner_loops,
      .depth = depth,
  };

  // Copy the sketch geometry so the wire-path builder can construct
  // exact OCCT curves for the primary profile boundary.  Additional
  // profiles are handled separately by wrap_with_additional_profiles
  // in build_single_depth_extrude — they are built as polygon prisms
  // and combined with the wire-path primary into one compound.
  for (const auto& line : sketch.lines) {
    if (!line.is_construction) parameters.sketch_lines.push_back(line);
  }
  for (const auto& arc : sketch.arcs) {
    if (!arc.is_construction) parameters.sketch_arcs.push_back(arc);
  }
  for (const auto& circle : sketch.circles) {
    if (!circle.is_construction) parameters.sketch_circles.push_back(circle);
  }
  // Use the ordered edge IDs from the first profile only, so
  // sketch_edge_ids always matches profile_points (also from first).
  if (!first.ordered_edge_ids.empty()) {
    parameters.sketch_edge_ids = first.ordered_edge_ids;
  }

  for (size_t index = 1; index < profiles.size(); ++index) {
    const auto& profile = profiles[index];
    parameters.additional_profile_points.push_back(
        profile_boundary_points(profile));
    parameters.additional_inner_loops.push_back(profile.inner_loops);
    parameters.additional_sketch_edge_ids.push_back(
        profile.ordered_edge_ids);
    parameters.additional_arc_descriptors.push_back(
        profile.arc_descriptors);
  }

  return parameters;
}

void preserve_extrude_source_geometry(ExtrudeFeatureParameters& target,
                                      const ExtrudeFeatureParameters& source) {
  target.sketch_feature_id = source.sketch_feature_id;
  target.profile_id = source.profile_id;
  target.profile_ids = source.profile_ids;
  target.open_entity_ids = source.open_entity_ids;
  target.plane_id = source.plane_id;
  target.plane_frame = source.plane_frame;
  target.profile_kind = source.profile_kind;
  target.start_x = source.start_x;
  target.start_y = source.start_y;
  target.width = source.width;
  target.height = source.height;
  target.radius = source.radius;
  target.profile_points = source.profile_points;
  target.inner_loops = source.inner_loops;
  target.additional_profile_points = source.additional_profile_points;
  target.additional_inner_loops = source.additional_inner_loops;
  target.additional_sketch_edge_ids = source.additional_sketch_edge_ids;
  target.additional_arc_descriptors = source.additional_arc_descriptors;
  target.sketch_edge_ids = source.sketch_edge_ids;
  target.sketch_lines = source.sketch_lines;
  target.sketch_arcs = source.sketch_arcs;
  target.sketch_circles = source.sketch_circles;
  target.arc_descriptors = source.arc_descriptors;
}

void normalize_extrude_operation_mode(ExtrudeFeatureParameters& parameters) {
  if (parameters.operation.empty()) {
    parameters.operation =
        parameters.mode.empty() ? std::string("new_body") : parameters.mode;
  }
  if (parameters.operation == "join" && !parameters.target_body_id.has_value()) {
    parameters.mode = "new_body";
    return;
  }
  parameters.mode = parameters.operation;
}

void apply_automatic_extrude_mode(const DocumentState& document,
                                  ExtrudeFeatureParameters& parameters,
                                  bool grouped_touching_profiles) {
  parameters.operation = "new_body";
  parameters.mode = "new_body";
  parameters.target_body_id = std::nullopt;

  if (const auto touching =
          find_touching_body_for_extrude(document, parameters);
      touching.has_value()) {
    parameters.operation = "join";
    parameters.mode = "join";
    parameters.target_body_id = touching;
    return;
  }

  if (const auto intersected =
          find_intersecting_body_for_extrude(document, parameters);
      intersected.has_value()) {
    parameters.operation = "cut";
    parameters.mode = "cut";
    parameters.target_body_id = intersected;
    return;
  }

  if (grouped_touching_profiles) {
    parameters.operation = "join";
    parameters.mode = "new_body";
  }
}
