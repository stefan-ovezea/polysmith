void refresh_linked_revolve_features(DocumentState &document,
                                     const FeatureEntry &sketch_feature) {
  const auto mark_revolve_warning = [](FeatureEntry &feature,
                                       const std::string &message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile or axis unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto &feature : document.feature_history) {
    if (feature.kind != "revolve" || !feature.revolve_parameters.has_value()) {
      continue;
    }
    const bool references_profile_sketch =
        feature.revolve_parameters->sketch_feature_id == sketch_feature.id;
    const bool references_axis_sketch =
        feature.revolve_parameters->axis_sketch_feature_id == sketch_feature.id;
    if (!references_profile_sketch && !references_axis_sketch) {
      continue;
    }

    RevolveFeatureParameters next_parameters =
        feature.revolve_parameters.value();
    const FeatureEntry *profile_sketch_feature = nullptr;
    const FeatureEntry *axis_sketch_feature = nullptr;
    const SketchProfileRegion *profile = nullptr;
    const SketchLine *axis_line = nullptr;

    for (const auto &candidate : document.feature_history) {
      if (candidate.id == next_parameters.sketch_feature_id) {
        profile_sketch_feature = &candidate;
      }
      if (candidate.id == next_parameters.axis_sketch_feature_id) {
        axis_sketch_feature = &candidate;
      }
    }
    if (profile_sketch_feature == nullptr ||
        !profile_sketch_feature->sketch_parameters.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve source profile sketch no longer exists. Edit the source "
          "sketch or recreate the revolve.");
      continue;
    }
    if (axis_sketch_feature == nullptr ||
        !axis_sketch_feature->sketch_parameters.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve axis sketch no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }

    profile = find_equivalent_profile(
        profile_sketch_feature->sketch_parameters->profiles,
        next_parameters.profile_id);
    axis_line = find_line_by_id(axis_sketch_feature->sketch_parameters.value(),
                                next_parameters.axis_entity_id);
    if (profile == nullptr) {
      mark_revolve_warning(
          feature,
          "Revolve source profile no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }
    if (axis_line == nullptr) {
      mark_revolve_warning(
          feature, "Revolve axis line no longer exists. Edit the source sketch "
                   "or recreate the revolve.");
      continue;
    }

    const auto rebuilt = make_revolve_parameters_for_profile(
        *profile_sketch_feature, *profile, *axis_sketch_feature, *axis_line,
        next_parameters.angle_degrees);
    if (!rebuilt.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve source profile or axis is no longer supported after "
          "the sketch edit.");
      continue;
    }

    try {
      const TopoDS_Shape next_shape = build_revolve_shape(rebuilt.value());
      if (next_shape.IsNull()) {
        throw std::runtime_error("Revolve rebuild produced an empty shape");
      }
    } catch (const std::exception &) {
      mark_revolve_warning(
          feature,
          "Revolve could not rebuild from the updated profile or axis. "
          "Edit the source sketch or recreate the revolve.");
      continue;
    }

    feature.revolve_parameters = rebuilt.value();
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.revolve_parameters->profile_id + " · " +
        std::to_string(feature.revolve_parameters->angle_degrees) + " deg";
  }
}
