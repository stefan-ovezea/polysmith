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
    const auto& first = selected_profiles.front();
    const std::string plane_id =
        sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                       : sketch.plane_id;
    const std::optional<PlaneFrame> plane_frame =
        sketch.plane_frame.has_value()
            ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
            : std::nullopt;
    std::vector<std::string> ids;
    for (const auto& profile : selected_profiles) {
      ids.push_back(profile.id);
    }
    next_parameters = ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_it->id,
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
        .profile_points = first.kind == "circle"
                              ? sample_circle_profile_points(first)
                              : first.points,
        .inner_loops = first.inner_loops,
        .depth = depth,
    };
    for (size_t index = 1; index < selected_profiles.size(); ++index) {
      const auto& profile = selected_profiles[index];
      next_parameters->additional_profile_points.push_back(
          profile.kind == "circle" ? sample_circle_profile_points(profile)
                                   : profile.points);
      next_parameters->additional_inner_loops.push_back(profile.inner_loops);
    }
  }
  if (!next_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found");
  }
  next_parameters->mode = mode;
  next_parameters->target_body_id = target_body_id;
  extrude_it->extrude_parameters = next_parameters.value();
  extrude_it->parameters_summary =
      extrude_it->extrude_parameters->profile_id + " · " +
      std::to_string(extrude_it->extrude_parameters->depth) + " mm";
  bump_geometry_revision();
  return document_.value();
}
