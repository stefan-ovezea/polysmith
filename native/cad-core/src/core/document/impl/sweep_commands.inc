DocumentState DocumentManager::update_sweep_path(
    const std::string& feature_id,
    const std::string& path_entity_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "sweep" || !feature_it->sweep_parameters.has_value()) {
    throw std::runtime_error(
        "update_sweep_path requires a sweep feature: " + feature_id);
  }

  const SweepFeatureParameters next_parameters =
      make_sweep_parameters(document_->feature_history,
                            feature_it->sweep_parameters->profile_id,
                            path_entity_id);
  const TopoDS_Shape next_shape = build_sweep_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Sweep update produced an empty shape");
  }
  feature_it->sweep_parameters = next_parameters;
  feature_it->parameters_summary = "Profile · path";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::sweep_profile(
    const std::string& profile_id,
    const std::string& path_entity_id) {
  require_document();
  const SweepFeatureParameters parameters =
      make_sweep_parameters(document_->feature_history, profile_id, path_entity_id);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_sweep_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sweep_profile(
    const std::string& feature_id,
    const std::string& profile_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "sweep" || !feature_it->sweep_parameters.has_value()) {
    throw std::runtime_error(
        "update_sweep_profile requires a sweep feature: " + feature_id);
  }

  const SweepFeatureParameters next_parameters =
      make_sweep_parameters(document_->feature_history,
                            profile_id,
                            feature_it->sweep_parameters->path_entity_id);
  const TopoDS_Shape next_shape = build_sweep_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Sweep update produced an empty shape");
  }
  feature_it->sweep_parameters = next_parameters;
  feature_it->parameters_summary = "Profile · path";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}
