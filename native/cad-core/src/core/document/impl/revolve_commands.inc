DocumentState DocumentManager::revolve_profile(
    const std::string& profile_id,
    const std::string& axis_entity_id,
    double angle_degrees) {
  require_document();
  const RevolveFeatureParameters parameters =
      make_revolve_parameters(document_->feature_history,
                              profile_id,
                              axis_entity_id,
                              angle_degrees);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_revolve_feature(next_feature_id_++, parameters));
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

DocumentState DocumentManager::update_revolve_profile(
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
  if (feature_it->kind != "revolve" ||
      !feature_it->revolve_parameters.has_value()) {
    throw std::runtime_error(
        "update_revolve_profile requires a revolve feature: " + feature_id);
  }

  const RevolveFeatureParameters next_parameters =
      make_revolve_parameters(document_->feature_history,
                              profile_id,
                              feature_it->revolve_parameters->axis_entity_id,
                              feature_it->revolve_parameters->angle_degrees);
  const TopoDS_Shape next_shape = build_revolve_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Revolve update produced an empty shape");
  }
  feature_it->revolve_parameters = next_parameters;
  feature_it->parameters_summary =
      next_parameters.profile_id + " · " +
      std::to_string(next_parameters.angle_degrees) + " deg";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_axis(
    const std::string& feature_id,
    const std::string& axis_entity_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "revolve" ||
      !feature_it->revolve_parameters.has_value()) {
    throw std::runtime_error(
        "update_revolve_axis requires a revolve feature: " + feature_id);
  }

  const RevolveFeatureParameters next_parameters =
      make_revolve_parameters(document_->feature_history,
                              feature_it->revolve_parameters->profile_id,
                              axis_entity_id,
                              feature_it->revolve_parameters->angle_degrees);
  const TopoDS_Shape next_shape = build_revolve_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Revolve update produced an empty shape");
  }
  feature_it->revolve_parameters = next_parameters;
  feature_it->parameters_summary =
      next_parameters.profile_id + " · " +
      std::to_string(next_parameters.angle_degrees) + " deg";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_angle(
    const std::string& feature_id,
    double angle_degrees) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  polysmith::core::update_revolve_angle(*feature_it, angle_degrees);
  bump_geometry_revision();
  return document_.value();
}
