DocumentState DocumentManager::update_sketch_dimension(
    const std::string& dimension_id,
    double value,
    std::optional<std::string> expression) {
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
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });
  if (dimension_it == feature_it->sketch_parameters->dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }
  if (dimension_it->kind == "circle_radius") {
    for (const auto& projection : feature_it->sketch_parameters->projections) {
      if (std::find(projection.generated_circle_ids.begin(),
                    projection.generated_circle_ids.end(),
                    dimension_it->entity_id) !=
          projection.generated_circle_ids.end()) {
        throw std::runtime_error(
            "Projected circle dimensions are driven by their source geometry");
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_dimension(*feature_it, dimension_id, value, expression);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  if (feature_it->sketch_parameters.has_value()) {
    const auto updated_dimension_it = std::find_if(
        feature_it->sketch_parameters->dimensions.begin(),
        feature_it->sketch_parameters->dimensions.end(),
        [&](const SketchDimension& dimension) {
          return dimension.id == dimension_id;
        });

    if (updated_dimension_it != feature_it->sketch_parameters->dimensions.end()) {
      document_->selected_sketch_entity_id = updated_dimension_it->entity_id;
      document_->selected_sketch_dimension_id = updated_dimension_it->id;
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension_label_position(
    const std::string& dimension_id,
    double label_x,
    double label_y) {
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

  auto& dimensions = feature_it->sketch_parameters->dimensions;
  const auto dimension_it = std::find_if(
      dimensions.begin(),
      dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });
  if (dimension_it == dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  push_undo_state();
  clear_redo_stack();
  dimension_it->label_x = label_x;
  dimension_it->label_y = label_y;
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
