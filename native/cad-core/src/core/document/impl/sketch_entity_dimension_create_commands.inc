DocumentState DocumentManager::add_sketch_line_length_dimension(
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
  polysmith::core::add_sketch_line_length_dimension(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-line-" + line_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_vertex_id = std::nullopt;
    document_->selected_sketch_vertex_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_line_angle_dimension(
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
  polysmith::core::add_sketch_line_angle_dimension(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-line-angle-" + line_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_vertex_id = std::nullopt;
    document_->selected_sketch_vertex_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_circle_radius_dimension(
    const std::string& circle_id,
    std::optional<std::string> display_as) {
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
  polysmith::core::add_sketch_circle_radius_dimension(
      *feature_it, circle_id, display_as);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-circle-" + circle_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_vertex_id = std::nullopt;
    document_->selected_sketch_vertex_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_polygon_radius_dimension(
    const std::string& polygon_id) {
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
  polysmith::core::add_sketch_polygon_radius_dimension(*feature_it,
                                                        polygon_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-polygon-" + polygon_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_vertex_id = std::nullopt;
    document_->selected_sketch_vertex_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_point_distance_dimension(
    const std::string& point_a_id,
    const std::string& point_b_id,
    std::optional<std::string> axis) {
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
  polysmith::core::add_sketch_point_distance_dimension(
      *feature_it, point_a_id, point_b_id, axis);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  // Select the newly created dimension
  const std::string dimension_id =
      "dim-point-distance-" + point_a_id + "-" + point_b_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_vertex_id = std::nullopt;
    document_->selected_sketch_vertex_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}
