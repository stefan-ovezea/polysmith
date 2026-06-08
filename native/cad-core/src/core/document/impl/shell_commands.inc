DocumentState DocumentManager::create_shell(const std::string& face_id,
                                            double thickness) {
  require_document();

  if (thickness <= 0.0) {
    throw std::runtime_error("Shell thickness must be greater than zero");
  }

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Malformed face id: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Shell face owner not found: " + owner_id);
  }

  push_undo_state();
  clear_redo_stack();

  ShellFeatureParameters params{};
  params.target_body_id = owner_id;
  params.removed_face_ids = {face_id};
  params.thickness = thickness;
  params.is_pending = true;

  std::ostringstream summary;
  summary << "1 face · " << thickness << " mm";

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "shell",
      .name = "Shell",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = std::nullopt,
      .chamfer_parameters = std::nullopt,
      .shell_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = face_id;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_reference_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_shell_thickness(
    const std::string& feature_id,
    double thickness) {
  require_document();

  if (thickness <= 0.0) {
    throw std::runtime_error("Shell thickness must be greater than zero");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "shell" ||
      !feature_it->shell_parameters.has_value()) {
    throw std::runtime_error(
        "update_shell_thickness requires a shell feature: " + feature_id);
  }

  feature_it->shell_parameters->thickness = thickness;
  std::ostringstream summary;
  summary << feature_it->shell_parameters->removed_face_ids.size() << " face"
          << (feature_it->shell_parameters->removed_face_ids.size() == 1 ? ""
                                                                         : "s")
          << " · " << thickness << " mm";
  feature_it->parameters_summary = summary.str();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_shell(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "shell" ||
      !feature_it->shell_parameters.has_value()) {
    throw std::runtime_error(
        "confirm_shell requires a shell feature: " + feature_id);
  }

  if (!feature_it->shell_parameters->is_pending) {
    return document_.value();
  }

  feature_it->shell_parameters->is_pending = false;
  document_->selected_face_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}
