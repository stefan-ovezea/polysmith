// DocumentManager wrappers for sketch text entities — same shape as the
// fillet wrappers (require document → active sketch → undo snapshot →
// core mutation → linked-extrude refresh → revision bump). The core
// mutation runs the glyph expansion inside refresh_sketch_derived_state.

DocumentState DocumentManager::add_sketch_text(const SketchText& text) {
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

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_text(*feature_it, next_sketch_text_id_++,
                                   text);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->texts.back().id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_text(
    const std::string& text_id,
    const SketchText& values) {
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

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_text(*feature_it, text_id, values);
  refresh_linked_extrudes(*document_, *feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_text(
    const std::string& text_id) {
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

  push_undo_state();
  clear_redo_stack();
  polysmith::core::delete_sketch_text(*feature_it, text_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_sketch_entity_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}
