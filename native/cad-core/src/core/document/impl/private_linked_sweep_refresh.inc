void refresh_linked_sweep_features(DocumentState &document,
                                   const FeatureEntry &sketch_feature) {
  const auto mark_sweep_warning = [](FeatureEntry &feature,
                                     const std::string &message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile or path unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto &feature : document.feature_history) {
    if (feature.kind != "sweep" || !feature.sweep_parameters.has_value()) {
      continue;
    }
    const bool references_profile_sketch =
        feature.sweep_parameters->sketch_feature_id == sketch_feature.id;
    const bool references_path_sketch =
        feature.sweep_parameters->path_sketch_feature_id == sketch_feature.id;
    if (!references_profile_sketch && !references_path_sketch) {
      continue;
    }

    try {
      const SweepFeatureParameters rebuilt = make_sweep_parameters(
          document.feature_history, feature.sweep_parameters->profile_id,
          feature.sweep_parameters->path_entity_id);
      const TopoDS_Shape next_shape = build_sweep_shape(rebuilt);
      if (next_shape.IsNull()) {
        throw std::runtime_error("Sweep rebuild produced an empty shape");
      }
      feature.sweep_parameters = rebuilt;
      feature.status = "healthy";
      feature.dependency_broken = false;
      feature.dependency_warning.clear();
      feature.parameters_summary = "Profile · path";
    } catch (const std::exception &) {
      mark_sweep_warning(
          feature,
          "Sweep could not rebuild from the updated profile or path. Edit the "
          "source sketch or recreate the sweep.");
    }
  }
}
