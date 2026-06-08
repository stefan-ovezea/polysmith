DocumentState DocumentManager::select_sketch_profile(const std::string& profile_id,
                                                     bool additive) {
  require_document();

  // Selection of profiles is allowed both inside and outside an active sketch.
  // The owning sketch feature is located by scanning the feature history.
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_id);

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto profile_it = std::find_if(
      feature_it->sketch_parameters->profiles.begin(),
      feature_it->sketch_parameters->profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });

  if (profile_it == feature_it->sketch_parameters->profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& selected = document_->selected_sketch_profile_ids;
    const auto existing = std::find(selected.begin(), selected.end(), profile_id);
    if (existing == selected.end()) {
      selected.push_back(profile_id);
    } else {
      selected.erase(existing);
    }
  } else {
    document_->selected_sketch_profile_ids = {profile_id};
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{document_->selected_sketch_profile_ids.back()};
  return document_.value();
}
