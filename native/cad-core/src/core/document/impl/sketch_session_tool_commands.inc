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
  document_->feature_history.back().sketch_parameters->face_fingerprint =
      SketchFeatureParameters::FaceFingerprint{
          .valid = true,
          .origin_x = plane_frame.origin_x,
          .origin_y = plane_frame.origin_y,
          .origin_z = plane_frame.origin_z,
          .normal_x = plane_frame.normal_x,
          .normal_y = plane_frame.normal_y,
          .normal_z = plane_frame.normal_z,
      };
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
  // Same session grouping as start_sketch_on_plane (D4): the edits of
  // this face-sketch session collapse into ONE "Sketch Edit" step.
  // After the bump, so the session snapshot carries the post-creation
  // revision (undo must never move the clock backwards).
  begin_undo_group("Sketch Edit");
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

  // Resolve the target plane. Origin reference planes resolve to their
  // hardcoded frame as well, so the placement check below has something to
  // compare against even though those sketches normally store no frame.
  const bool target_is_origin_plane = is_origin_plane_reference(new_plane_id);
  const auto resolved_frame =
      resolve_plane_source_frame(*document_, new_plane_id);

  // Re-parenting: two different intents (see
  // private_sketch_plane_placement_helpers.inc).
  //
  //  * A different plane — the sketch moves onto it, keeping its local
  //    coordinates. Construction planes and faces store their resolved
  //    frame; origin planes keep the legacy no-frame convention.
  //  * An origin plane the sketch already lies in (typically a face
  //    re-parented to XY so the body can be deleted). Adopting XY's frame
  //    would drag the sketch to the model origin, because the face plane is
  //    anchored wherever the exporter put it. Keep the sketch's placement
  //    instead and only slide it onto the target plane.
  //
  // A plane that cannot be resolved (e.g. a face on a deleted body) is
  // rejected up front.
  std::optional<SketchFeatureParameters::SketchPlaneFrame> sketch_frame;
  if (!target_is_origin_plane) {
    if (!resolved_frame.has_value()) {
      throw std::runtime_error(
          "Sketch plane source could not be resolved: " + new_plane_id);
    }
    sketch_frame = to_sketch_plane_frame(resolved_frame.value());
  } else if (resolved_frame.has_value()) {
    // Only a plane the sketch is NOT already attached to can be a
    // re-parent worth preserving placement across; re-selecting the same
    // origin plane stays the legacy no-frame sketch it already is.
    const bool plane_changes =
        feature_it->sketch_parameters->plane_id != new_plane_id;
    const auto current_frame =
        projection_frame(*feature_it->sketch_parameters);
    const auto target_frame = to_sketch_plane_frame(resolved_frame.value());
    if (plane_changes && current_frame.has_value() &&
        planes_are_parallel_same_facing(*current_frame, target_frame)) {
      sketch_frame =
          keep_sketch_placement_on_plane(*current_frame, target_frame);
    }
  }

  push_undo_state();
  clear_redo_stack();

  auto& parameters = *feature_it->sketch_parameters;
  parameters.plane_id = new_plane_id;
  parameters.plane_frame = sketch_frame;
  // A face target carries immutable plane provenance for the refresh
  // walk; origin / construction planes have none.
  if (!face_owner_id(new_plane_id).empty() && sketch_frame.has_value()) {
    parameters.face_fingerprint = SketchFeatureParameters::FaceFingerprint{
        .valid = true,
        .origin_x = sketch_frame->origin_x,
        .origin_y = sketch_frame->origin_y,
        .origin_z = sketch_frame->origin_z,
        .normal_x = sketch_frame->normal_x,
        .normal_y = sketch_frame->normal_y,
        .normal_z = sketch_frame->normal_z,
    };
  } else {
    parameters.face_fingerprint.reset();
  }
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
