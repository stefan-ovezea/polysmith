DocumentState DocumentManager::update_sketch_line(const std::string& line_id,
                                                  double start_x,
                                                  double start_y,
                                                  double end_x,
                                                  double end_y) {
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
  polysmith::core::update_sketch_line(
      *feature_it, line_id, start_x, start_y, end_x, end_y);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_vertex(
    const std::string& point_id,
    double x,
    double y,
    const std::string& bind_to_vertex_id) {
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
  polysmith::core::update_sketch_vertex(*feature_it, point_id, x, y);

  // A drag that lands on a standalone Point-tool point must SIMPLIFY the
  // sketch, not leave a duplicate vertex hiding under the endpoint: bind the
  // point to the dropped vertex (which merges every line that shared the
  // point onto the dropped id, and records the coincident constraint the
  // viewport badges) and then drop the now-orphaned `SketchPoint` record.
  // Same undo step as the move. A non-Point target id is ignored, so dragging
  // onto an ordinary endpoint keeps today's behaviour.
  if (!bind_to_vertex_id.empty() && bind_to_vertex_id != point_id &&
      feature_it->sketch_parameters.has_value()) {
    auto& parameters = *feature_it->sketch_parameters;
    const auto point_it = std::find_if(
        parameters.points.begin(), parameters.points.end(),
        [&](const SketchPoint& point) { return point.id == bind_to_vertex_id; });
    if (point_it != parameters.points.end()) {
      // Point id -> dropped vertex id: the merged lines now reference the
      // vertex the user dropped, and the constraint entry lands on those
      // lines. The point record's id is no longer referenced anywhere.
      polysmith::core::set_sketch_coincident_constraint(
          *feature_it, bind_to_vertex_id, point_id);
      parameters.points.erase(
          std::remove_if(parameters.points.begin(), parameters.points.end(),
                         [&](const SketchPoint& point) {
                           return point.id == bind_to_vertex_id;
                         }),
          parameters.points.end());
      polysmith::core::refresh_sketch_derived_state(*feature_it);
    }
  }

  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::move_sketch_entities(
    const std::vector<std::string>& entity_ids,
    double dx,
    double dy,
    double center_x,
    double center_y,
    double angle_deg) {
  return transform_sketch_entities(entity_ids, dx, dy, center_x, center_y,
                                   angle_deg, /*scale=*/1.0,
                                   /*copy=*/false);
}

DocumentState DocumentManager::transform_sketch_entities(
    const std::vector<std::string>& entity_ids,
    double dx,
    double dy,
    double center_x,
    double center_y,
    double angle_deg,
    double scale,
    bool copy) {
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

  // One undo step for the whole transform — never call the per-entity
  // update commands from here (they push undo per call).
  push_undo_state();
  clear_redo_stack();
  polysmith::core::transform_sketch_entities(*feature_it, entity_ids, dx, dy,
                                             center_x, center_y, angle_deg,
                                             scale, copy);
  refresh_linked_extrudes(*document_, *feature_it);
  // Selection is deliberately preserved so the user can repeat the
  // transform on the same entities.
  bump_geometry_revision();
  return document_.value();
}
