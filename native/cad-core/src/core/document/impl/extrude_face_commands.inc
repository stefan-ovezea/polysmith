DocumentState DocumentManager::extrude_face(
    const std::string& face_id,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();

  const auto profile = compute_planar_face_profile(*document_, face_id);
  if (!profile.has_value()) {
    throw std::runtime_error("Selected face is not a supported planar face");
  }

  ExtrudeFeatureParameters extrude_parameters{
      .sketch_feature_id = "",
      .profile_id = "face:" + face_id,
      .profile_ids = {"face:" + face_id},
      .plane_id = plane_id_from_frame(profile->plane_frame),
      .plane_frame = profile->plane_frame,
      .profile_kind = "polygon",
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .profile_points = profile->outer_points,
      .inner_loops = profile->inner_loops,
      .depth = depth,
      .mode = mode,
      .target_body_id = target_body_id,
  };

  if (parameters.has_value()) {
    const auto source = extrude_parameters;
    extrude_parameters = parameters.value();
    extrude_parameters.sketch_feature_id = source.sketch_feature_id;
    extrude_parameters.profile_id = source.profile_id;
    extrude_parameters.profile_ids = source.profile_ids;
    extrude_parameters.plane_id = source.plane_id;
    extrude_parameters.plane_frame = source.plane_frame;
    extrude_parameters.profile_kind = source.profile_kind;
    extrude_parameters.start_x = source.start_x;
    extrude_parameters.start_y = source.start_y;
    extrude_parameters.width = source.width;
    extrude_parameters.height = source.height;
    extrude_parameters.radius = source.radius;
    extrude_parameters.profile_points = source.profile_points;
    extrude_parameters.inner_loops = source.inner_loops;
    extrude_parameters.additional_profile_points =
        source.additional_profile_points;
    extrude_parameters.additional_inner_loops = source.additional_inner_loops;
  }
  extrude_parameters.depth = depth;
  extrude_parameters.mode = mode;
  extrude_parameters.target_body_id = target_body_id;
  if (mode.empty()) {
    apply_automatic_extrude_mode(*document_, extrude_parameters, false);
  } else {
    if (extrude_parameters.operation == "new_body" && mode != "new_body") {
      extrude_parameters.operation = mode;
    }
    normalize_extrude_operation_mode(extrude_parameters);
  }
  normalize_extrude_parameters(*document_, extrude_parameters);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_text_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
