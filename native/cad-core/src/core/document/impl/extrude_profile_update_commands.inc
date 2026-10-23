DocumentState DocumentManager::update_extrude_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();

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

  if (profile_ids.empty()) {
    // Deselecting every surface empties the extrude.  Clear the profile
    // geometry so the body compiler emits no shape, but keep the feature in
    // the timeline so the panel can show "0 selected" and a later re-select
    // rebuilds it through the normal parameter builder.
    auto& params = extrude_it->extrude_parameters.value();
    params.profile_id.clear();
    params.profile_ids.clear();
    params.profile_points.clear();
    params.inner_loops.clear();
    params.additional_profile_points.clear();
    params.additional_inner_loops.clear();
    params.sketch_lines.clear();
    params.sketch_arcs.clear();
    params.sketch_circles.clear();
    params.sketch_edge_ids.clear();
    params.boundary_edges.clear();
    params.arc_descriptors.clear();
    params.inner_loop_edges.clear();
    params.additional_sketch_edge_ids.clear();
    params.additional_arc_descriptors.clear();
    params.additional_boundary_edges.clear();
    bump_geometry_revision();
    return document_.value();
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
  // Delegate to the shared parameter builder — it handles multi-profile
  // groups, sets up additional profiles and inner loops, and deliberately
  // leaves sketch curve data empty so the shape builder routes through
  // the polygon-prism path (which handles every profile in the group).
  const std::optional<ExtrudeFeatureParameters> built_parameters =
      make_extrude_parameters_for_profiles(*sketch_it, selected_profiles, depth);
  if (!built_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found");
  }

  // Copy the existing parameters and overwrite only what the profile
  // determines. Re-binding used to force `side1.distance` back to `depth` and
  // drop everything else the user had set (two-sided/symmetric extents, thin
  // walls, targets, taper).
  ExtrudeFeatureParameters next_parameters = *extrude_it->extrude_parameters;
  preserve_extrude_source_geometry(next_parameters, *built_parameters);
  next_parameters.depth = depth;
  extrude_it->extrude_parameters = next_parameters;

  // This is the repair path for a feature that lost a source region: a
  // successful re-bind must clear the stale failure state, or the body stays
  // suppressed and confirming the panel looks like it did nothing.
  extrude_it->status = "healthy";
  extrude_it->dependency_broken = false;
  extrude_it->dependency_warning.clear();
  extrude_it->parameters_summary =
      extrude_it->extrude_parameters->profile_id + " · " +
      std::to_string(extrude_it->extrude_parameters->depth) + " mm";
  bump_geometry_revision();
  return document_.value();
}
