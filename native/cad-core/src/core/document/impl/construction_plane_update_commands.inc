DocumentState DocumentManager::update_offset_plane(
    const std::string& feature_id, double offset) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "construction_plane" ||
      !feature_it->construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "update_offset_plane requires a construction_plane feature: " +
        feature_id);
  }
  if (feature_it->construction_plane_parameters->plane_type != "offset") {
    throw std::runtime_error(
        "update_offset_plane requires an offset construction plane: " +
        feature_id);
  }

  // Resolve the source frame from the rest of the document (everything
  // before this feature in feature_history is already up to date by
  // the time we get here, and the source must come from earlier in
  // history per `is_selectable_plane_reference`).
  const auto& params = feature_it->construction_plane_parameters.value();
  const auto frame =
      resolve_plane_source_frame(*document_, params.source_plane_id);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Offset plane source could not be resolved: " + params.source_plane_id);
  }

  // No push_undo_state here: this is a live-preview update on the
  // freshly-created construction plane whose own create_offset_plane
  // already pushed an undo step. Pushing here would make a panel
  // session of N debounced offset edits collapse to N user-visible
  // undo steps when the user really wants Cancel to revert the
  // entire session. Mirrors the same comment + rationale in
  // `update_fillet_edges` / `update_fillet_radius`.
  clear_redo_stack();
  polysmith::core::update_construction_plane(*feature_it, offset, frame.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_angle_plane(
    const std::string& feature_id, double angle_degrees) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "construction_plane" ||
      !feature_it->construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "update_angle_plane requires a construction_plane feature: " +
        feature_id);
  }
  if (feature_it->construction_plane_parameters->plane_type != "angle") {
    throw std::runtime_error(
        "update_angle_plane requires an angle construction plane: " +
        feature_id);
  }

  const auto& params = feature_it->construction_plane_parameters.value();
  const auto source_frame =
      resolve_plane_source_frame(*document_, params.source_plane_id);
  const auto axis =
      resolve_angle_plane_axis(*document_, params.source_axis_id);
  if (!source_frame.has_value()) {
    throw std::runtime_error(
        "Angle plane source could not be resolved: " + params.source_plane_id);
  }
  if (!axis.has_value()) {
    throw std::runtime_error(
        "Angle plane axis could not be resolved: " + params.source_axis_id);
  }

  clear_redo_stack();
  polysmith::core::update_angle_plane(
      *feature_it, angle_degrees, source_frame.value(), axis.value());
  bump_geometry_revision();
  return document_.value();
}
