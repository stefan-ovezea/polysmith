bool has_feature_id(const DocumentState& document, const std::string& id) {
  return std::any_of(document.feature_history.begin(),
                     document.feature_history.end(),
                     [&](const FeatureEntry& feature) {
                       return feature.id == id;
                     });
}

DocumentState document_at_timeline_cursor(const DocumentState& source) {
  if (!source.timeline_cursor.has_value()) {
    return source;
  }

  DocumentState snapshot = source;
  snapshot.feature_history.clear();

  int remaining_actions = std::max(0, source.timeline_cursor.value());
  for (const auto& feature : source.feature_history) {
    if (feature.kind == "root_part") {
      snapshot.feature_history.push_back(feature);
      continue;
    }
    if (remaining_actions <= 0) {
      continue;
    }
    snapshot.feature_history.push_back(feature);
    --remaining_actions;
  }

  if (snapshot.selected_feature_id.has_value() &&
      !has_feature_id(snapshot, snapshot.selected_feature_id.value())) {
    snapshot.selected_feature_id = std::nullopt;
  }
  if (snapshot.selected_reference_id.has_value() &&
      snapshot.selected_reference_id->rfind("ref-plane-", 0) != 0 &&
      !has_feature_id(snapshot, snapshot.selected_reference_id.value())) {
    snapshot.selected_reference_id = std::nullopt;
  }
  if (snapshot.active_sketch_feature_id.has_value() &&
      !has_feature_id(snapshot, snapshot.active_sketch_feature_id.value())) {
    snapshot.active_sketch_feature_id = std::nullopt;
    snapshot.active_sketch_plane_id = std::nullopt;
    snapshot.active_sketch_face_id = std::nullopt;
    snapshot.active_sketch_tool = std::nullopt;
    snapshot.selected_sketch_vertex_id = std::nullopt;
    snapshot.selected_sketch_entity_id = std::nullopt;
    snapshot.selected_sketch_dimension_id = std::nullopt;
    snapshot.selected_sketch_profile_id = std::nullopt;
    snapshot.selected_sketch_vertex_ids.clear();
    snapshot.selected_sketch_entity_ids.clear();
    snapshot.selected_sketch_profile_ids.clear();
  }
  snapshot.selected_face_id = std::nullopt;
  snapshot.selected_edge_ids.clear();
  snapshot.selected_vertex_ids.clear();

  return snapshot;
}
