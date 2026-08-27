DocumentState DocumentManager::trim_sketch_entity(
    const std::string& entity_id,
    double click_x,
    double click_y,
    std::optional<int> segment_index,
    std::vector<std::string>* minted_vertex_ids) {
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
  // The caller rarely needs the minted list, but the freeze below
  // always does — collect into a local scratch when none was passed.
  std::vector<std::string> scratch_minted;
  std::vector<std::string>* minted =
      minted_vertex_ids != nullptr ? minted_vertex_ids : &scratch_minted;
  polysmith::core::trim_sketch_entity(
      *feature_it, entity_id, click_x, click_y, segment_index, minted);

  // Refresh derived state: rebuild points, recompute profiles, reify
  // dimensions, and re-sync projection/dependency state.
  polysmith::core::refresh_sketch_derived_state(*feature_it);

  // Freeze the vertices this trim minted. refresh_sketch_derived_state
  // rebuilt the vertex table with is_fixed = false, so the planegcs
  // pass can move a fresh split point and re-open the loop the trim
  // just welded. Split points sit exactly on analytic intersections;
  // they are definitional, not degrees of freedom.
  if (!minted->empty()) {
    const std::unordered_set<std::string> minted_set(
        minted->begin(), minted->end());
    for (auto& vertex : feature_it->sketch_parameters->vertices) {
      if (minted_set.count(vertex.id)) vertex.is_fixed = true;
    }
  }

  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = entity_id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
