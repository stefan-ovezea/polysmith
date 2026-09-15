DocumentState DocumentManager::start_sketch_on_face(
    const std::string& face_id,
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  require_document();

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Sketch face not found: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch face owner not found: " + face_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_sketch_feature(next_feature_id_++, face_id, plane_frame));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->active_sketch_plane_id = face_id;
  document_->active_sketch_face_id = face_id;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_text_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::redefine_sketch_plane(
    const std::string& feature_id,
    const std::string& new_plane_id) {
  require_document();

  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "sketch" ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error(
        "redefine_sketch_plane requires a sketch feature");
  }

  // Origin reference planes keep the legacy no-frame convention
  // (hardcoded mappings); construction planes and face targets
  // resolve their frame from the current document — the same sources
  // start_sketch_on_plane / start_sketch_on_face accept. A plane
  // that cannot be resolved (e.g. a face on a deleted body) is
  // rejected up front.
  std::optional<SketchFeatureParameters::SketchPlaneFrame> sketch_frame;
  if (!is_origin_plane_reference(new_plane_id)) {
    const auto frame = resolve_plane_source_frame(*document_, new_plane_id);
    if (!frame.has_value()) {
      throw std::runtime_error(
          "Sketch plane source could not be resolved: " + new_plane_id);
    }
    sketch_frame = to_sketch_plane_frame(frame.value());
  }

  push_undo_state();
  clear_redo_stack();

  auto& parameters = *feature_it->sketch_parameters;
  parameters.plane_id = new_plane_id;
  parameters.plane_frame = sketch_frame;
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();

  // Keep the session state consistent when the redefined sketch is
  // the ACTIVE one (face picks carry active_sketch_face_id; plane
  // picks leave it empty — mirrors the start_sketch_* setters).
  if (document_->active_sketch_feature_id.has_value() &&
      document_->active_sketch_feature_id.value() == feature_id) {
    document_->active_sketch_plane_id = new_plane_id;
    document_->active_sketch_face_id =
        face_owner_id(new_plane_id).empty()
            ? std::nullopt
            : std::optional<std::string>(new_plane_id);
  }

  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_tool(const std::string& tool) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  if (!is_supported_sketch_tool(tool)) {
    throw std::runtime_error("Unsupported sketch tool: " + tool);
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

  polysmith::core::set_sketch_tool(*feature_it, tool);
  document_->active_sketch_tool = tool;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}
