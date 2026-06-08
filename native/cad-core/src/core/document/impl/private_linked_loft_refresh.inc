void refresh_linked_loft_features(DocumentState &document,
                                  const FeatureEntry &sketch_feature) {
  const auto mark_loft_profile_warning = [](FeatureEntry &feature,
                                            const std::string &message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto &feature : document.feature_history) {
    if (feature.kind != "loft" || !feature.loft_parameters.has_value()) {
      continue;
    }

    bool references_sketch = false;
    for (const auto &section : feature.loft_parameters->sections) {
      if (section.sketch_feature_id == sketch_feature.id) {
        references_sketch = true;
        break;
      }
    }
    if (!references_sketch) {
      continue;
    }

    const auto &sketch = sketch_feature.sketch_parameters.value();
    LoftFeatureParameters next_parameters = feature.loft_parameters.value();
    bool failed = false;
    for (auto &section : next_parameters.sections) {
      if (section.sketch_feature_id != sketch_feature.id) {
        continue;
      }
      const SketchProfileRegion *profile =
          find_equivalent_profile(sketch.profiles, section.profile_id);
      if (profile == nullptr) {
        mark_loft_profile_warning(
            feature,
            "Loft source profile no longer exists. Edit the source sketch "
            "or recreate the loft.");
        failed = true;
        break;
      }
      const auto next_section =
          make_loft_section_for_profile(sketch_feature, *profile);
      if (!next_section.has_value()) {
        mark_loft_profile_warning(
            feature,
            "Loft source profile now contains unsupported holes. Edit the "
            "source sketch or recreate the loft.");
        failed = true;
        break;
      }
      section = next_section.value();
    }
    if (failed) {
      continue;
    }

    try {
      const TopoDS_Shape next_shape = build_loft_shape(next_parameters);
      if (next_shape.IsNull()) {
        throw std::runtime_error("Loft rebuild produced an empty shape");
      }
    } catch (const std::exception &) {
      mark_loft_profile_warning(
          feature,
          "Loft could not rebuild from the updated profile shape. Edit the "
          "source sketch or recreate the loft.");
      continue;
    }

    feature.loft_parameters = next_parameters;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        std::to_string(feature.loft_parameters->sections.size()) + " sections" +
        (feature.loft_parameters->ruled ? " · ruled" : "");
  }
}
