std::optional<LoftSectionParameters> make_loft_section_for_profile(
    const FeatureEntry& sketch_feature,
    const SketchProfileRegion& profile) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }
  if (!profile.inner_loops.empty()) {
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

  return LoftSectionParameters{
      .sketch_feature_id = sketch_feature.id,
      .profile_id = profile.id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_points = profile.kind == "circle"
                            ? sample_circle_profile_points(profile)
                            : profile.points,
  };
}
