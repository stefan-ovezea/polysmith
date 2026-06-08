DocumentState DocumentManager::select_sketch_dimension(
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

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });

  if (dimension_it == feature_it->sketch_parameters->dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}
