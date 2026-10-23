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

    // Re-derive the profile geometry with the same builder the creation path
    // uses, then copy ONLY the profile-derived fields onto the existing
    // parameters.
    //
    // Rebuilding the whole struct here (the previous behaviour) reset every
    // other field to its default: side1 fell back to its 10 mm "unset"
    // sentinel (the body came out half as thick), operation reset from join to
    // new_body (each selected profile became its own body), and the boundary
    // edges the renderer needs were dropped — which is what fed an empty
    // contour to THREE.Path.closePath() and blanked the window.  Any field
    // added later is preserved by the copy instead of silently lost.
    // See wiki/Sketch-Defect-Log.md D-001b.
    std::optional<ExtrudeFeatureParameters> built_parameters;
    try {
      built_parameters = make_extrude_parameters_for_profiles(
          sketch_feature, source_profiles, depth);
    } catch (const std::exception &error) {
      // A sketch edit must not fail because this extrude's profile became
      // invalid (e.g. two holes now touch).  Report it on the feature and let
      // the edit stand; re-extruding surfaces the message to the user.
      feature.status = "warning";
      feature.parameters_summary = "Source profile invalid";
      feature.dependency_broken = true;
      feature.dependency_warning = error.what();
      continue;
    }
    if (!built_parameters.has_value()) {
      feature.status = "warning";
      feature.parameters_summary = "Source profile unsupported";
      feature.dependency_broken = true;
      feature.dependency_warning = "Extrude source profile is no longer "
                                   "supported after the sketch edit.";
      continue;
    }

    ExtrudeFeatureParameters next_parameters = *feature.extrude_parameters;
    preserve_extrude_source_geometry(next_parameters, *built_parameters);
    next_parameters.depth = depth;
    feature.extrude_parameters = next_parameters;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.extrude_parameters->profile_id + " · " +
        std::to_string(feature.extrude_parameters->depth) + " mm";
  }
}
