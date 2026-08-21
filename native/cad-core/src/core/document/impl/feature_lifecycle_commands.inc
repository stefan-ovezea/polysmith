DocumentState DocumentManager::rename_feature(const std::string& feature_id,
                                              const std::string& name) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Feature name cannot be empty");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->name = name;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_feature_suppressed(
    const std::string& feature_id, bool suppressed) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be suppressed");
  }

  // No-op early exit so we don't pollute undo with a redundant entry
  // when the UI's "Suppress" button is double-clicked.
  if (feature_it->suppressed == suppressed) {
    return document_.value();
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->suppressed = suppressed;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be deleted");
  }

  if (feature_it->kind == "sketch" &&
      document_->active_sketch_feature_id.has_value() &&
      document_->active_sketch_feature_id.value() == feature_id) {
    throw std::runtime_error("Finish the active sketch before deleting it");
  }

  push_undo_state();
  clear_redo_stack();

  const bool deleted_selected =
      document_->selected_feature_id.has_value() &&
      document_->selected_feature_id.value() == feature_id;

  document_->feature_history.erase(feature_it);
  if (deleted_selected) {
    document_->selected_feature_id = std::nullopt;
  }

  // Drop 3D selections owned by the deleted feature — its faces,
  // edges, and vertices are gone, and leaving their ids selected
  // would re-light (possibly different) topology after undo/redo or
  // on the next body whose ids collide.
  const auto owner_of = [](const std::string& id, const char* kind) {
    const std::string needle = std::string(":") + kind + ":";
    const auto separator = id.find(needle);
    return separator == std::string::npos
               ? std::string()
               : id.substr(0, separator);
  };
  if (document_->selected_face_id.has_value()) {
    const std::string owner =
        owner_of(document_->selected_face_id.value(), "face");
    if (owner == feature_id) {
      document_->selected_face_id = std::nullopt;
    }
  }
  document_->selected_edge_ids.erase(
      std::remove_if(
          document_->selected_edge_ids.begin(),
          document_->selected_edge_ids.end(),
          [&](const std::string& id) {
            return owner_of(id, "edge") == feature_id;
          }),
      document_->selected_edge_ids.end());
  document_->selected_vertex_ids.erase(
      std::remove_if(
          document_->selected_vertex_ids.begin(),
          document_->selected_vertex_ids.end(),
          [&](const std::string& id) {
            return owner_of(id, "vertex") == feature_id;
          }),
      document_->selected_vertex_ids.end());

  // Freeze construction planes that reference the deleted feature:
  // they keep their cached frame (position) but sever the reference,
  // so sketches hosted on them stay healthy. Mirrors the Detach
  // Projections semantics for projection links.
  for (auto& remaining : document_->feature_history) {
    if (remaining.kind != "construction_plane" ||
        !remaining.construction_plane_parameters.has_value()) {
      continue;
    }
    auto& params = remaining.construction_plane_parameters.value();
    const auto references_deleted = [&](const std::string& source_id) {
      return source_id == feature_id ||
             source_id.rfind(feature_id + ":", 0) == 0;
    };
    const bool depends_on_deleted =
        references_deleted(params.source_plane_id) ||
        std::any_of(params.source_plane_ids.begin(),
                    params.source_plane_ids.end(), references_deleted) ||
        references_deleted(params.source_axis_id);
    if (depends_on_deleted) {
      params.plane_type = "detached";
      params.source_plane_id.clear();
      params.source_plane_ids.clear();
      params.source_axis_id.clear();
      remaining.parameters_summary = "Detached";
    }
  }

  bump_geometry_revision();
  return document_.value();
}
