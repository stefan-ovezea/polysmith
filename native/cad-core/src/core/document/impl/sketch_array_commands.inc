DocumentState DocumentManager::create_linear_array(
    const std::vector<std::string>& entity_ids,
    double dx,
    double dy,
    int count) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
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

  // One undo step for the whole array.
  push_undo_state();
  clear_redo_stack();
  polysmith::core::create_linear_array(*feature_it, entity_ids, dx, dy, count);
  refresh_linked_extrudes(*document_, *feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_circular_array(
    const std::vector<std::string>& entity_ids,
    double center_x,
    double center_y,
    int count,
    double total_angle_deg) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
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

  // One undo step for the whole array.
  push_undo_state();
  clear_redo_stack();
  polysmith::core::create_circular_array(*feature_it, entity_ids, center_x,
                                         center_y, count, total_angle_deg);
  refresh_linked_extrudes(*document_, *feature_it);
  bump_geometry_revision();
  return document_.value();
}
