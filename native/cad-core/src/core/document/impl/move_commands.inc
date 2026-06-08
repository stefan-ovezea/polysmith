DocumentState DocumentManager::create_move(
    const std::string& target_body_id,
    const MoveFeatureParameters& parameters) {
  require_document();
  if (!compiled_body_exists(*document_, target_body_id)) {
    throw std::runtime_error("Move target body not found: " + target_body_id);
  }

  MoveFeatureParameters next = parameters;
  next.target_body_id = target_body_id;
  next.is_pending = true;

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "move";
  feature.name = "Move";
  feature.status = "healthy";
  feature.parameters_summary = make_move_parameters_summary(next);
  feature.move_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_move_parameters(
    const std::string& feature_id,
    const MoveFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "move" ||
      !feature_it->move_parameters.has_value()) {
    throw std::runtime_error("update_move_parameters requires a move feature");
  }

  MoveFeatureParameters next = parameters;
  next.target_body_id = feature_it->move_parameters->target_body_id;
  next.is_pending = feature_it->move_parameters->is_pending;
  feature_it->move_parameters = next;
  feature_it->parameters_summary = make_move_parameters_summary(next);
  mark_feature_healthy(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_move(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "move" ||
      !feature_it->move_parameters.has_value()) {
    throw std::runtime_error("confirm_move requires a move feature");
  }
  feature_it->move_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}
