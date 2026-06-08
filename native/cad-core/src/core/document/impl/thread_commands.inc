DocumentState DocumentManager::create_thread(
    const ThreadFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "thread";
  feature.name = "Thread";
  feature.status = "healthy";
  feature.parameters_summary = parameters.size.empty() ? "Custom thread" : parameters.size;
  feature.thread_parameters = parameters;
  feature.thread_parameters->is_pending = true;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_thread_parameters(
    const std::string& feature_id,
    const ThreadFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "thread" ||
      !feature_it->thread_parameters.has_value()) {
    throw std::runtime_error("update_thread_parameters requires a thread feature");
  }
  feature_it->thread_parameters = parameters;
  feature_it->parameters_summary = parameters.size.empty() ? "Custom thread" : parameters.size;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_thread(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "thread" ||
      !feature_it->thread_parameters.has_value()) {
    throw std::runtime_error("confirm_thread requires a thread feature");
  }
  feature_it->thread_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}
