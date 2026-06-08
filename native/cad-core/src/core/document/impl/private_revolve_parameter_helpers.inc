RevolveFeatureParameters make_revolve_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& axis_entity_id,
    double angle_degrees) {
  const auto profile_sketch_it =
      find_sketch_feature_owning_profile(features, profile_id);
  if (profile_sketch_it == features.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  auto& profile_sketch = profile_sketch_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      profile_sketch.profiles.begin(),
      profile_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == profile_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto axis_sketch_it =
      find_sketch_feature_owning_line(features, axis_entity_id);
  if (axis_sketch_it == features.end()) {
    throw std::runtime_error("Revolve axis line not found: " + axis_entity_id);
  }
  const SketchLine* axis_line =
      find_line_by_id(axis_sketch_it->sketch_parameters.value(), axis_entity_id);
  if (axis_line == nullptr) {
    throw std::runtime_error("Revolve axis line not found: " + axis_entity_id);
  }

  const auto parameters =
      make_revolve_parameters_for_profile(*profile_sketch_it,
                                          *profile_it,
                                          *axis_sketch_it,
                                          *axis_line,
                                          angle_degrees);
  if (!parameters.has_value()) {
    throw std::runtime_error("Selected profile or axis is not supported for revolve");
  }
  return parameters.value();
}
