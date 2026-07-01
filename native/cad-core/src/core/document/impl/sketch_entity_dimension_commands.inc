DocumentState DocumentManager::delete_sketch_dimension(
    const std::string& dimension_id) {
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

  push_undo_state();
  clear_redo_stack();
  polysmith::core::delete_sketch_dimension(*feature_it, dimension_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension_display(
    const std::string& dimension_id,
    const std::string& display_as) {
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

  if (!feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Sketch feature has no parameters");
  }

  auto& dimensions = feature_it->sketch_parameters->dimensions;
  const auto dim_it = std::find_if(
      dimensions.begin(), dimensions.end(),
      [&](const SketchDimension& d) { return d.id == dimension_id; });
  if (dim_it == dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }
  // Only circle_radius dimensions support display_as toggling
  if (dim_it->kind != "circle_radius") {
    throw std::runtime_error(
        "display_as can only be changed on circle radius dimensions");
  }

  push_undo_state();
  clear_redo_stack();
  dim_it->display_as = display_as;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::toggle_sketch_dimension_driven(
    const std::string& dimension_id) {
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

  push_undo_state();
  clear_redo_stack();
  polysmith::core::toggle_sketch_dimension_driven(*feature_it, dimension_id);
  bump_geometry_revision();
  return document_.value();
}
