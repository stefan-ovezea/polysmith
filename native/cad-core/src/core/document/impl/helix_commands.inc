DocumentState DocumentManager::create_helix(
    const std::string& axis_source_id,
    const HelixFeatureParameters& parameters) {
  require_document();
  const auto axis = resolve_construction_axis_source(*document_, axis_source_id);
  if (!axis.has_value()) {
    throw std::runtime_error("Helix axis source not found or not linear");
  }
  HelixFeatureParameters next = parameters;
  next.axis_source_id = axis_source_id;
  next.axis_start_x = axis->start_x;
  next.axis_start_y = axis->start_y;
  next.axis_start_z = axis->start_z;
  next.axis_end_x = axis->end_x;
  next.axis_end_y = axis->end_y;
  next.axis_end_z = axis->end_z;
  next.turns = next.pitch > 0.0 ? next.height / next.pitch : 0.0;
  next.points = make_helix_points(axis.value(), next);
  if (next.points.empty()) {
    throw std::runtime_error("Helix parameters are invalid");
  }

  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "helix";
  feature.name = "Helix";
  feature.status = "healthy";
  feature.parameters_summary = std::to_string(next.pitch) + " mm pitch";
  feature.helix_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_helix_parameters(
    const std::string& feature_id,
    const HelixFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "helix" ||
      !feature_it->helix_parameters.has_value()) {
    throw std::runtime_error("update_helix_parameters requires a helix feature");
  }
  const auto axis = resolve_construction_axis_source(
      *document_, feature_it->helix_parameters->axis_source_id);
  if (!axis.has_value()) {
    feature_it->dependency_broken = true;
    feature_it->dependency_warning = "Helix axis source is no longer available.";
    return document_.value();
  }
  HelixFeatureParameters next = parameters;
  next.axis_source_id = feature_it->helix_parameters->axis_source_id;
  next.axis_start_x = axis->start_x;
  next.axis_start_y = axis->start_y;
  next.axis_start_z = axis->start_z;
  next.axis_end_x = axis->end_x;
  next.axis_end_y = axis->end_y;
  next.axis_end_z = axis->end_z;
  next.turns = next.pitch > 0.0 ? next.height / next.pitch : 0.0;
  next.points = make_helix_points(axis.value(), next);
  feature_it->helix_parameters = next;
  feature_it->dependency_broken = next.points.empty();
  feature_it->dependency_warning =
      next.points.empty() ? "Helix parameters are invalid." : "";
  bump_geometry_revision();
  return document_.value();
}
