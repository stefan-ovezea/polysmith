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
  bump_geometry_revision();
  return document_.value();
}
