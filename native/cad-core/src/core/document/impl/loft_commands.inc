DocumentState DocumentManager::loft_profiles(
    const std::vector<std::string>& profile_ids,
    bool ruled) {
  require_document();
  const LoftFeatureParameters parameters =
      make_loft_parameters_for_profiles(document_->feature_history,
                                        profile_ids,
                                        ruled);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_loft_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_loft_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "loft" || !feature_it->loft_parameters.has_value()) {
    throw std::runtime_error(
        "update_loft_profiles requires a loft feature: " + feature_id);
  }

  const bool ruled = feature_it->loft_parameters->ruled;
  const LoftFeatureParameters next_parameters =
      make_loft_parameters_for_profiles(document_->feature_history,
                                        profile_ids,
                                        ruled);
  const TopoDS_Shape next_shape = build_loft_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Loft update produced an empty shape");
  }
  feature_it->loft_parameters = next_parameters;
  feature_it->parameters_summary =
      std::to_string(feature_it->loft_parameters->sections.size()) +
      " sections" + (feature_it->loft_parameters->ruled ? " · ruled" : "");
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_loft_ruled(const std::string& feature_id,
                                                 bool ruled) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  polysmith::core::update_loft_ruled(*feature_it, ruled);
  bump_geometry_revision();
  return document_.value();
}
