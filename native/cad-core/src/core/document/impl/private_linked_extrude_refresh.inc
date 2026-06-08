void refresh_linked_extrude_features(DocumentState &document,
                                     const FeatureEntry &sketch_feature) {
  const auto mark_source_profile_warning = [](FeatureEntry &feature) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning =
        "Extrude source profile no longer exists. Edit the source sketch "
        "or recreate the extrude.";
  };

  for (auto &feature : document.feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value() ||
        feature.extrude_parameters->sketch_feature_id != sketch_feature.id) {
      continue;
    }

    const auto &sketch = sketch_feature.sketch_parameters.value();
    const std::vector<std::string> source_profile_ids =
        feature.extrude_parameters->profile_ids.empty()
            ? std::vector<std::string>{feature.extrude_parameters->profile_id}
            : feature.extrude_parameters->profile_ids;
    std::vector<SketchProfileRegion> source_profiles;
    for (const auto &profile_id : source_profile_ids) {
      const SketchProfileRegion *profile =
          find_equivalent_profile(sketch.profiles, profile_id);
      if (profile == nullptr) {
        mark_source_profile_warning(feature);
        source_profiles.clear();
        break;
      }
      source_profiles.push_back(*profile);
    }
    if (source_profiles.empty()) {
      continue;
    }

    const double depth = feature.extrude_parameters->depth;
    const std::string mode = feature.extrude_parameters->mode;
    const auto target_body_id = feature.extrude_parameters->target_body_id;
    std::optional<ExtrudeFeatureParameters> next_parameters;
    if (source_profiles.size() == 1 &&
        source_profiles.front().kind == "circle") {
      next_parameters = make_extrude_parameters_for_profile(
          sketch_feature, source_profiles.front(), depth);
    } else {
      const auto &first = source_profiles.front();
      const std::string plane_id =
          sketch.plane_frame.has_value()
              ? plane_id_from_frame(sketch.plane_frame.value())
              : sketch.plane_id;
      const std::optional<PlaneFrame> plane_frame =
          sketch.plane_frame.has_value()
              ? std::optional<PlaneFrame>(
                    make_plane_frame(sketch.plane_frame.value()))
              : std::nullopt;
      std::vector<std::string> ids;
      for (const auto &profile : source_profiles) {
        ids.push_back(profile.id);
      }
      next_parameters = ExtrudeFeatureParameters{
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
          .profile_points = first.kind == "circle"
                                ? sample_circle_profile_points(first)
                                : first.points,
          .inner_loops = first.inner_loops,
          .depth = depth,
      };
      for (size_t index = 1; index < source_profiles.size(); ++index) {
        const auto &profile = source_profiles[index];
        next_parameters->additional_profile_points.push_back(
            profile.kind == "circle" ? sample_circle_profile_points(profile)
                                     : profile.points);
        next_parameters->additional_inner_loops.push_back(profile.inner_loops);
      }
    }
    if (!next_parameters.has_value()) {
      feature.status = "warning";
      feature.parameters_summary = "Source profile unsupported";
      feature.dependency_broken = true;
      feature.dependency_warning = "Extrude source profile is no longer "
                                   "supported after the sketch edit.";
      continue;
    }

    feature.extrude_parameters = next_parameters.value();
    feature.extrude_parameters->mode = mode;
    feature.extrude_parameters->target_body_id = target_body_id;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.extrude_parameters->profile_id + " · " +
        std::to_string(feature.extrude_parameters->depth) + " mm";
  }
}
