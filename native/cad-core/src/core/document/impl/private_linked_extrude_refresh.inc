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
    // S30b: a profile that no longer resolves used to take the WHOLE extrude
    // with it (dependency_broken, no body at all): deleting one hole circle
    // destroyed the plate. Keep the profiles that still resolve and report the
    // shortfall instead, so the feature degrades to "fewer holes" rather than
    // vanishing.
    std::vector<SketchProfileRegion> source_profiles;
    std::vector<std::string> missing_profile_ids;
    for (const auto &profile_id : source_profile_ids) {
      const SketchProfileRegion *profile =
          find_equivalent_profile(sketch.profiles, profile_id);
      if (profile == nullptr) {
        missing_profile_ids.push_back(profile_id);
        continue;
      }
      source_profiles.push_back(*profile);
    }
    if (source_profiles.empty()) {
      mark_source_profile_warning(feature);
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
    if (!missing_profile_ids.empty()) {
      // S30b: the body is rebuilt from the survivors; say what was dropped and
      // keep the feature usable. dependency_broken stays false on purpose - the
      // geometry is intact, it is simply smaller than before.
      feature.status = "warning";
      feature.dependency_warning =
          std::to_string(missing_profile_ids.size()) + " of " +
          std::to_string(source_profile_ids.size()) +
          " profiles no longer exist after the sketch edit; extruding the " +
          std::to_string(source_profiles.size()) + " that remain.";
    }
  }
}
