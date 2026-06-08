DocumentState DocumentManager::create_fastener(
    const FastenerFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "fastener";
  feature.name = "Fastener";
  feature.status = "healthy";
  feature.parameters_summary = parameters.size + " · " + std::to_string(parameters.length) + " mm";
  feature.fastener_parameters = parameters;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fastener_parameters(
    const std::string& feature_id,
    const FastenerFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "fastener" ||
      !feature_it->fastener_parameters.has_value()) {
    throw std::runtime_error("update_fastener_parameters requires a fastener feature");
  }
  feature_it->fastener_parameters = parameters;
  feature_it->parameters_summary = parameters.size + " · " + std::to_string(parameters.length) + " mm";
  bump_geometry_revision();
  return document_.value();
}
