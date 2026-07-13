DocumentState DocumentManager::finish_sketch() {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch to finish");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->status = "healthy";
  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::reenter_sketch(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch feature not found: " + feature_id);
  }

  if (feature_it->kind != "sketch" || !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Feature is not a sketch: " + feature_id);
  }

  // Re-entering does not push to undo: it only flips active flags, so undo
  // continues to refer to real geometry edits.
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = feature_it->sketch_parameters->plane_id;
  // Face id information is not preserved separately in sketch_parameters; the
  // plane_id matches the face id when the sketch was created on a face, which
  // is sufficient for downstream consumers (viewport hides the sketch face,
  // raycasting is plane-frame based).
  document_->active_sketch_face_id =
      feature_it->sketch_parameters->plane_frame.has_value()
          ? std::optional<std::string>(feature_it->sketch_parameters->plane_id)
          : std::nullopt;
  document_->active_sketch_feature_id = feature_it->id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
