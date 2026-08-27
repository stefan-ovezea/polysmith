DocumentState DocumentManager::add_sketch_chamfer(
    const std::string& corner_vertex_id,
    const std::string& line_a_id,
    const std::string& line_b_id,
    double distance_a,
    double distance_b) {
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

  // Trim points share the line endpoint counter so the points table
  // stays uniform; the chamfer line uses the line counter and the
  // record has its own counter — separate id namespaces (mirrors the
  // fillet wrapper).
  const int trim_a_point_index = next_sketch_line_id_++;
  const int trim_b_point_index = next_sketch_line_id_++;
  polysmith::core::add_sketch_chamfer(*feature_it,
                                      next_sketch_chamfer_id_++,
                                      trim_a_point_index,
                                      trim_b_point_index,
                                      next_sketch_line_id_++,
                                      corner_vertex_id,
                                      line_a_id,
                                      line_b_id,
                                      distance_a,
                                      distance_b);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->chamfers.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_chamfer(
    const std::string& chamfer_id,
    double distance_a,
    double distance_b) {
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
  polysmith::core::update_sketch_chamfer(*feature_it, chamfer_id,
                                         distance_a, distance_b);
  refresh_linked_extrudes(*document_, *feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_chamfer(
    const std::string& chamfer_id) {
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
  polysmith::core::delete_sketch_chamfer(*feature_it, chamfer_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_sketch_entity_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}
