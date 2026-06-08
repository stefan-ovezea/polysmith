DocumentState DocumentManager::create_document() {
  DocumentState document{
      .id = "doc-" + std::to_string(next_document_id_++),
      .name = "Untitled Part",
      .units = "mm",
      .revision = 1,
      .selected_feature_id = std::nullopt,
      .selected_reference_id = std::nullopt,
      .selected_face_id = std::nullopt,
      .selected_edge_ids = {},
      .selected_vertex_ids = {},
      .active_sketch_plane_id = std::nullopt,
      .active_sketch_face_id = std::nullopt,
      .active_sketch_feature_id = std::nullopt,
      .active_sketch_tool = std::nullopt,
      .selected_sketch_point_id = std::nullopt,
      .selected_sketch_entity_id = std::nullopt,
      .selected_sketch_dimension_id = std::nullopt,
      .selected_sketch_profile_id = std::nullopt,
      .selected_sketch_profile_ids = {},
      .timeline_cursor = std::nullopt,
      .feature_history = {make_root_feature()},
      .parameters = {},
      .appearance = {},
      .selection_filter = SelectionFilter{},
  };

  document_ = document;
  document_count_ = 1;
  undo_stack_.clear();
  redo_stack_.clear();
  return document;
}
