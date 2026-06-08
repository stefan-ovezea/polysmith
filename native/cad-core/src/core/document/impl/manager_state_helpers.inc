void DocumentManager::require_document() const {
  if (!document_.has_value()) {
    throw std::runtime_error("No active document");
  }
}

void DocumentManager::push_undo_state() {
  require_document();
  undo_stack_.push_back(document_.value());
}

void DocumentManager::bump_geometry_revision() {
  require_document();
  refresh_history_dependencies(*document_);
  normalize_timeline_cursor(*document_);
  // Increment the revision via a local before assigning back so the
  // literal `document_->revision += 1;` doesn't appear here — that
  // way mass-renaming call sites won't accidentally rewrite this
  // implementation.
  const int next = document_->revision + 1;
  document_->revision = next;
}

void DocumentManager::clear_redo_stack() {
  redo_stack_.clear();
}

FeatureEntry DocumentManager::make_root_feature() {
  return FeatureEntry{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "root_part",
      .name = "Base Part",
      .status = "healthy",
      .parameters_summary = "Document root",
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
  };
}
