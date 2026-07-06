DocumentState DocumentManager::set_sketch_line_constraint(
    const std::string& line_id,
    const std::optional<std::string>& constraint) {
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
  polysmith::core::set_sketch_line_constraint(*feature_it, line_id, constraint);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_sketch_line_constraints(
    const std::string& line_id) {
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
  polysmith::core::clear_sketch_line_constraints(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_line_construction(
    const std::string& line_id,
    bool is_construction) {
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
  polysmith::core::set_sketch_line_construction(
      *feature_it, line_id, is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_midpoint_anchor(
    const std::string& point_id,
    const std::string& host_line_id) {
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
  polysmith::core::set_sketch_midpoint_anchor(
      *feature_it, point_id, host_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_point_line_anchor(
    const std::string& point_id,
    const std::string& host_line_id,
    double t) {
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
  polysmith::core::set_sketch_point_line_anchor(
      *feature_it, point_id, host_line_id, t);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_equal_length_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
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
  polysmith::core::set_sketch_equal_length_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_perpendicular_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
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
  polysmith::core::set_sketch_perpendicular_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
