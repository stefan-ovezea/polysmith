DocumentState DocumentManager::create_hole(
    const std::string& face_id,
    double center_x,
    double center_y,
    double center_z,
    const HoleFeatureParameters& parameters) {
  require_document();
  const auto frame = resolve_plane_source_frame(*document_, face_id);
  if (!frame.has_value()) {
    throw std::runtime_error("Hole requires a planar face source");
  }
  const auto face_marker = face_id.find(":face:");
  if (face_marker == std::string::npos || face_marker == 0) {
    throw std::runtime_error("Hole requires a body face id");
  }

  HoleFeatureParameters next = parameters;
  next.target_body_id = face_id.substr(0, face_marker);
  next.source_face_id = face_id;
  next.plane_frame = frame.value();
  const double dx = center_x - frame->origin_x;
  const double dy = center_y - frame->origin_y;
  const double dz = center_z - frame->origin_z;
  next.center_x = dx * frame->x_axis_x + dy * frame->x_axis_y + dz * frame->x_axis_z;
  next.center_y = dx * frame->y_axis_x + dy * frame->y_axis_y + dz * frame->y_axis_z;
  next.is_pending = true;
  if (next.thread_enabled && next.thread_depth <= 0.0) {
    next.thread_depth = next.depth;
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "hole";
  feature.name = "Hole";
  feature.status = "healthy";
  feature.parameters_summary = next.hole_type + " · " +
                               std::to_string(next.diameter) + " mm";
  feature.hole_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_hole_parameters(
    const std::string& feature_id,
    const HoleFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "hole" ||
      !feature_it->hole_parameters.has_value()) {
    throw std::runtime_error("update_hole_parameters requires a hole feature");
  }
  HoleFeatureParameters next = parameters;
  next.target_body_id = feature_it->hole_parameters->target_body_id;
  next.source_face_id = feature_it->hole_parameters->source_face_id;
  next.plane_frame = feature_it->hole_parameters->plane_frame;
  next.center_x = feature_it->hole_parameters->center_x;
  next.center_y = feature_it->hole_parameters->center_y;
  next.is_pending = feature_it->hole_parameters->is_pending;
  feature_it->hole_parameters = next;
  feature_it->parameters_summary = next.hole_type + " · " +
                                   std::to_string(next.diameter) + " mm";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_hole(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "hole" ||
      !feature_it->hole_parameters.has_value()) {
    throw std::runtime_error("confirm_hole requires a hole feature");
  }
  feature_it->hole_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}
