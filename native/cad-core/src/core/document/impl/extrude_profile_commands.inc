DocumentState DocumentManager::extrude_profile(
    const std::string& profile_id,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  return extrude_profiles({profile_id}, depth, mode, target_body_id, parameters);
}

DocumentState DocumentManager::extrude_profiles(
    const std::vector<std::string>& profile_ids,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();
  if (profile_ids.empty()) {
    throw std::runtime_error("No sketch profiles selected");
  }

  // Extrusion runs on any sketch profile in the document, even if its parent
  // sketch is finished (i.e. not the active sketch).
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_ids.front());

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_ids.front());
  }

  const auto& sketch = feature_it->sketch_parameters.value();
  std::vector<SketchProfileRegion> selected_profiles;
  for (const auto& profile_id : profile_ids) {
    const auto owner_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (owner_it == document_->feature_history.end() ||
        owner_it->id != feature_it->id) {
      throw std::runtime_error(
          "Selected profiles must belong to the same sketch plane");
    }
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    selected_profiles.push_back(*profile_it);
  }

  const bool automatic_mode = mode.empty();
  std::vector<std::vector<SketchProfileRegion>> profile_groups;
  if (selected_profiles.size() == 1 || mode == "cut" || mode == "intersect" ||
      (mode == "join" && target_body_id.has_value())) {
    profile_groups.push_back(selected_profiles);
  } else if (automatic_mode || mode == "join") {
    profile_groups = group_touching_profiles(selected_profiles);
  } else {
    for (const auto& profile : selected_profiles) {
      profile_groups.push_back({profile});
    }
  }

  std::vector<ExtrudeFeatureParameters> extrude_parameter_groups;
  extrude_parameter_groups.reserve(profile_groups.size());
  for (const auto& group : profile_groups) {
    std::optional<ExtrudeFeatureParameters> extrude_parameters =
        make_extrude_parameters_for_profiles(*feature_it, group, depth);
    if (!extrude_parameters.has_value()) {
      throw std::runtime_error("Sketch profile not found");
    }
    if (parameters.has_value()) {
      const auto source = *extrude_parameters;
      *extrude_parameters = parameters.value();
      preserve_extrude_source_geometry(*extrude_parameters, source);
    }
    extrude_parameters->depth = depth;
    extrude_parameters->side1.distance = std::abs(depth);
    extrude_parameters->target_body_id = target_body_id;
    if (automatic_mode) {
      apply_automatic_extrude_mode(*document_,
                                   extrude_parameters.value(),
                                   group.size() > 1);
    } else {
      const bool split_join_without_target =
          mode == "join" && !target_body_id.has_value();
      extrude_parameters->mode = split_join_without_target ? "new_body" : mode;
      if (extrude_parameters->operation == "new_body" &&
          mode != "new_body") {
        extrude_parameters->operation = mode;
      }
      normalize_extrude_operation_mode(extrude_parameters.value());
    }
    normalize_extrude_parameters(*document_, extrude_parameters.value());
    extrude_parameter_groups.push_back(extrude_parameters.value());
  }

  push_undo_state();
  clear_redo_stack();
  for (const auto& extrude_parameters : extrude_parameter_groups) {
    document_->feature_history.push_back(
        create_extrude_feature(next_feature_id_++, extrude_parameters));
  }
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}
