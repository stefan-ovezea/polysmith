LoftFeatureParameters make_loft_parameters_for_profiles(
    std::vector<FeatureEntry>& features,
    const std::vector<std::string>& profile_ids,
    bool ruled) {
  if (profile_ids.size() < 2) {
    throw std::runtime_error("Loft requires at least two sketch profiles");
  }

  LoftFeatureParameters parameters{};
  parameters.ruled = ruled;
  for (const auto& profile_id : profile_ids) {
    const auto sketch_it = find_sketch_feature_owning_profile(features, profile_id);
    if (sketch_it == features.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    const auto& sketch = sketch_it->sketch_parameters.value();
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    if (!profile_it->inner_loops.empty()) {
      throw std::runtime_error("Loft does not support profiles with holes yet");
    }
    const auto section = make_loft_section_for_profile(*sketch_it, *profile_it);
    if (!section.has_value()) {
      throw std::runtime_error("Selected profile is not supported for loft");
    }
    parameters.sections.push_back(section.value());
  }

  return parameters;
}
