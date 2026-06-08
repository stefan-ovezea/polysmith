SweepFeatureParameters make_sweep_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& path_entity_id) {
  const auto profile_sketch_it =
      find_sketch_feature_owning_profile(features, profile_id);
  if (profile_sketch_it == features.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  const auto& profile_sketch = profile_sketch_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      profile_sketch.profiles.begin(),
      profile_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == profile_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto path_sketch_it =
      find_sketch_feature_owning_path_entity(features, path_entity_id);
  if (path_sketch_it == features.end()) {
    throw std::runtime_error("Sweep path entity not found: " + path_entity_id);
  }

  const std::string plane_id =
      profile_sketch.plane_frame.has_value()
          ? plane_id_from_frame(profile_sketch.plane_frame.value())
          : profile_sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      profile_sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(
                make_plane_frame(profile_sketch.plane_frame.value()))
          : std::nullopt;
  const auto path_segments = make_sweep_path_segments(
      path_sketch_it->sketch_parameters.value(), path_entity_id);
  const auto& first_segment = path_segments.front();
  const auto& last_segment = path_segments.back();

  return SweepFeatureParameters{
      .sketch_feature_id = profile_sketch_it->id,
      .profile_id = profile_it->id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = profile_it->kind,
      .profile_points = profile_it->kind == "circle"
                            ? sample_circle_profile_points(*profile_it)
                            : profile_it->points,
      .inner_loops = profile_it->inner_loops,
      .path_sketch_feature_id = path_sketch_it->id,
      .path_entity_id = path_entity_id,
      .path_start_x = first_segment.start_x,
      .path_start_y = first_segment.start_y,
      .path_start_z = first_segment.start_z,
      .path_end_x = last_segment.end_x,
      .path_end_y = last_segment.end_y,
      .path_end_z = last_segment.end_z,
      .path_segments = path_segments,
  };
}
