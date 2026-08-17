DocumentState DocumentManager::update_extrude_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();
  if (profile_ids.empty()) {
    throw std::runtime_error("No sketch profiles selected");
  }

  const auto extrude_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (extrude_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (extrude_it->kind != "extrude" ||
      !extrude_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_profiles requires an extrude feature: " + feature_id);
  }

  const auto sketch_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_ids.front());
  if (sketch_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_ids.front());
  }

  const auto& sketch = sketch_it->sketch_parameters.value();
  std::vector<SketchProfileRegion> selected_profiles;
  for (const auto& profile_id : profile_ids) {
    const auto owner_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (owner_it == document_->feature_history.end() ||
        owner_it->id != sketch_it->id) {
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

  const double depth = extrude_it->extrude_parameters->depth;
  const std::string mode = extrude_it->extrude_parameters->mode;
  const auto target_body_id = extrude_it->extrude_parameters->target_body_id;
  std::optional<ExtrudeFeatureParameters> next_parameters;
  if (selected_profiles.size() == 1 && selected_profiles.front().kind == "circle") {
    next_parameters =
        make_extrude_parameters_for_profile(*sketch_it,
                                            selected_profiles.front(),
                                            depth);
  } else {
    // Delegate to the shared parameter builder — it handles multi-profile
    // groups, sets up additional profiles and inner loops, and deliberately
    // leaves sketch curve data empty so the shape builder routes through
    // the polygon-prism path (which handles every profile in the group).
    next_parameters =
        make_extrude_parameters_for_profiles(*sketch_it,
                                             selected_profiles,
                                             depth);
  }
  if (!next_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found");
  }
  next_parameters->mode = mode;
  next_parameters->target_body_id = target_body_id;
  next_parameters->side1.distance = std::abs(depth);
  extrude_it->extrude_parameters = next_parameters.value();
  extrude_it->parameters_summary =
      extrude_it->extrude_parameters->profile_id + " · " +
      std::to_string(extrude_it->extrude_parameters->depth) + " mm";
  bump_geometry_revision();
  return document_.value();
}
