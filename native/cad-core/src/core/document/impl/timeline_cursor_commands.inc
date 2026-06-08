DocumentState DocumentManager::set_timeline_cursor(int included_action_count) {
  require_document();

  const int max_actions = action_count(document_.value());
  const int clamped =
      std::max(0, std::min(included_action_count, max_actions));

  if (clamped >= max_actions) {
    document_->timeline_cursor = std::nullopt;
  } else {
    document_->timeline_cursor = clamped;
  }

  if (document_->selected_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_feature_id.value())) {
    document_->selected_feature_id = std::nullopt;
  }
  if (document_->selected_reference_id.has_value() &&
      !is_origin_plane_reference(document_->selected_reference_id.value()) &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_reference_id.value())) {
    document_->selected_reference_id = std::nullopt;
  }
  if (document_->active_sketch_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->active_sketch_feature_id.value())) {
    document_->active_sketch_feature_id = std::nullopt;
    document_->active_sketch_plane_id = std::nullopt;
    document_->active_sketch_face_id = std::nullopt;
    document_->active_sketch_tool = std::nullopt;
  }
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_profile_ids.clear();

  document_->revision += 1;
  return document_.value();
}
