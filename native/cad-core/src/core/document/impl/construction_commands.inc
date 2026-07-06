DocumentState DocumentManager::create_offset_plane(
    const std::string& source_plane_id, double offset) {
  require_document();

  // The source must be a real plane that exists in the current
  // document. We allow the three origin planes, any earlier
  // construction plane, any sketch profile id, and any planar body
  // face id (face ids of the form "<body_id>:face:<index>"). The
  // validity check is intentionally lighter than `select_face`'s —
  // `resolve_plane_source_frame` returns nullopt for unknown /
  // non-planar / missing sources, so the `frame.has_value()` guard
  // below catches every failure mode in one place.
  const auto frame =
      resolve_plane_source_frame(*document_, source_plane_id);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Offset plane source not found: " + source_plane_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_construction_plane_feature(
      next_feature_id_++, source_plane_id, offset, frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  // Select the new plane both as a feature (for the hierarchy /
  // timeline) and as a reference (for the viewport quad highlight
  // and the Sketch button gate). Origin-plane selection only sets
  // selected_reference_id, but construction planes are *both* a
  // feature and a reference, so we keep both in sync.
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_midplane(
    const std::string& first_source_id,
    const std::string& second_source_id) {
  require_document();
  const auto first_frame =
      resolve_plane_source_frame(*document_, first_source_id);
  const auto second_frame =
      resolve_plane_source_frame(*document_, second_source_id);
  if (!first_frame.has_value() || !second_frame.has_value()) {
    throw std::runtime_error("Midplane source not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_midplane_feature(
      next_feature_id_++,
      first_source_id,
      second_source_id,
      first_frame.value(),
      second_frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_tangent_plane(
    const std::string& source_face_id) {
  require_document();
  const auto tangent_frame =
      resolve_tangent_plane_source_frame(*document_, source_face_id);
  if (!tangent_frame.has_value()) {
    throw std::runtime_error("Tangent plane source face not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_tangent_plane_feature(
      next_feature_id_++, source_face_id, tangent_frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_angle_plane(
    const std::string& source_plane_id,
    const std::string& source_axis_id,
    double angle_degrees) {
  require_document();
  const auto source_frame =
      resolve_plane_source_frame(*document_, source_plane_id);
  const auto axis =
      resolve_angle_plane_axis(*document_, source_axis_id);
  if (!source_frame.has_value()) {
    throw std::runtime_error("Angle plane source not found");
  }
  if (!axis.has_value()) {
    throw std::runtime_error("Angle plane axis not found or not linear");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_angle_plane_feature(
      next_feature_id_++,
      source_plane_id,
      source_axis_id,
      angle_degrees,
      source_frame.value(),
      axis.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_construction_axis(
    const std::string& source_id) {
  require_document();
  const auto axis = resolve_construction_axis_source(*document_, source_id);
  if (!axis.has_value()) {
    throw std::runtime_error(
        "Construction axis source not found or not linear: " + source_id);
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "construction_axis";
  feature.name = "Axis";
  feature.status = "healthy";
  feature.parameters_summary =
      axis->source_kind == "edge" ? "From edge" : "From sketch line";
  feature.construction_axis_parameters = axis.value();
  document_->feature_history.push_back(feature);

  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_construction_point(
    const std::string& source_id) {
  require_document();
  const auto point = resolve_construction_point_source(*document_, source_id);
  if (!point.has_value()) {
    throw std::runtime_error(
        "Construction point source not found: " + source_id);
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "construction_point";
  feature.name = "Point";
  feature.status = "healthy";
  feature.parameters_summary =
      point->source_kind == "vertex" ? "From vertex" : "From sketch point";
  feature.construction_point_parameters = point.value();
  document_->feature_history.push_back(feature);

  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
