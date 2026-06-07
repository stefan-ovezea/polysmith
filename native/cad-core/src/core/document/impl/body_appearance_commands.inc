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

DocumentState DocumentManager::create_thread(
    const ThreadFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "thread";
  feature.name = "Thread";
  feature.status = "healthy";
  feature.parameters_summary = parameters.size.empty() ? "Custom thread" : parameters.size;
  feature.thread_parameters = parameters;
  feature.thread_parameters->is_pending = true;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_thread_parameters(
    const std::string& feature_id,
    const ThreadFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "thread" ||
      !feature_it->thread_parameters.has_value()) {
    throw std::runtime_error("update_thread_parameters requires a thread feature");
  }
  feature_it->thread_parameters = parameters;
  feature_it->parameters_summary = parameters.size.empty() ? "Custom thread" : parameters.size;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_thread(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "thread" ||
      !feature_it->thread_parameters.has_value()) {
    throw std::runtime_error("confirm_thread requires a thread feature");
  }
  feature_it->thread_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_fastener(
    const FastenerFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "fastener";
  feature.name = "Fastener";
  feature.status = "healthy";
  feature.parameters_summary = parameters.size + " · " + std::to_string(parameters.length) + " mm";
  feature.fastener_parameters = parameters;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fastener_parameters(
    const std::string& feature_id,
    const FastenerFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "fastener" ||
      !feature_it->fastener_parameters.has_value()) {
    throw std::runtime_error("update_fastener_parameters requires a fastener feature");
  }
  feature_it->fastener_parameters = parameters;
  feature_it->parameters_summary = parameters.size + " · " + std::to_string(parameters.length) + " mm";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_move(
    const std::string& target_body_id,
    const MoveFeatureParameters& parameters) {
  require_document();
  if (!compiled_body_exists(*document_, target_body_id)) {
    throw std::runtime_error("Move target body not found: " + target_body_id);
  }

  MoveFeatureParameters next = parameters;
  next.target_body_id = target_body_id;
  next.is_pending = true;

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "move";
  feature.name = "Move";
  feature.status = "healthy";
  feature.parameters_summary = make_move_parameters_summary(next);
  feature.move_parameters = next;
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

DocumentState DocumentManager::update_move_parameters(
    const std::string& feature_id,
    const MoveFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "move" ||
      !feature_it->move_parameters.has_value()) {
    throw std::runtime_error("update_move_parameters requires a move feature");
  }

  MoveFeatureParameters next = parameters;
  next.target_body_id = feature_it->move_parameters->target_body_id;
  next.is_pending = feature_it->move_parameters->is_pending;
  feature_it->move_parameters = next;
  feature_it->parameters_summary = make_move_parameters_summary(next);
  mark_feature_healthy(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_move(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "move" ||
      !feature_it->move_parameters.has_value()) {
    throw std::runtime_error("confirm_move requires a move feature");
  }
  feature_it->move_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_body_copy(
    const std::string& source_body_id,
    const std::string& copy_mode) {
  require_document();

  const std::string normalized_mode =
      copy_mode == "standalone" ? "standalone" : "linked";
  const CompiledBodies compiled = compile_bodies(*document_);
  const auto body_it = std::find_if(
      compiled.bodies.begin(),
      compiled.bodies.end(),
      [&](const CompiledBody& body) { return body.id == source_body_id; });
  if (body_it == compiled.bodies.end()) {
    throw std::runtime_error("Copy source body not found: " + source_body_id);
  }

  push_undo_state();
  clear_redo_stack();

  BodyCopyFeatureParameters params{};
  params.source_body_id = source_body_id;
  params.copy_mode = normalized_mode;
  params.source_body_name = feature_name_by_id(*document_, source_body_id);
  write_copy_frame(params, body_it->local_frame);
  if (normalized_mode == "standalone") {
    params.serialized_shape = serialize_shape_snapshot(body_it->shape);
  }

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "body_copy";
  feature.name = normalized_mode == "standalone" ? "Independent Copy" : "Linked Copy";
  feature.status = "healthy";
  feature.parameters_summary =
      normalized_mode == "standalone" ? "Independent body copy" : "Linked body copy";
  feature.body_copy_parameters = params;
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

DocumentState DocumentManager::unlink_body_copy(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "body_copy" ||
      !feature_it->body_copy_parameters.has_value()) {
    throw std::runtime_error("unlink_body_copy requires a body copy feature");
  }
  if (feature_it->body_copy_parameters->copy_mode != "linked") {
    throw std::runtime_error("Body copy is already independent");
  }

  DocumentState prefix = *document_;
  const auto feature_index =
      static_cast<std::size_t>(std::distance(document_->feature_history.begin(),
                                             feature_it));
  prefix.feature_history.resize(feature_index + 1);
  const CompiledBodies compiled = compile_bodies(prefix);
  const auto body_it = std::find_if(
      compiled.bodies.begin(),
      compiled.bodies.end(),
      [&](const CompiledBody& body) { return body.id == feature_id; });
  if (body_it == compiled.bodies.end() || body_it->shape.IsNull()) {
    throw std::runtime_error("Could not resolve linked copy body for unlink");
  }

  BodyCopyFeatureParameters next = feature_it->body_copy_parameters.value();
  next.copy_mode = "standalone";
  next.serialized_shape = serialize_shape_snapshot(body_it->shape);
  write_copy_frame(next, body_it->local_frame);

  push_undo_state();
  clear_redo_stack();
  feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  feature_it->body_copy_parameters = next;
  feature_it->name = "Independent Copy";
  feature_it->parameters_summary = "Independent body copy";
  mark_feature_healthy(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_body_color(const std::string& body_id,
                                              const std::string& color) {
  require_document();
  if (!is_valid_appearance_color(color)) {
    throw std::runtime_error("Body color must be an opaque #RRGGBB value");
  }
  if (!compiled_body_exists(*document_, body_id)) {
    throw std::runtime_error("Body not found: " + body_id);
  }

  push_undo_state();
  clear_redo_stack();
  auto& overrides = document_->appearance.body_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const BodyAppearanceOverride& entry) {
        return entry.body_id == body_id;
      });
  if (existing == overrides.end()) {
    overrides.push_back(BodyAppearanceOverride{.body_id = body_id,
                                               .color = color});
  } else {
    existing->color = color;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_face_color(const std::string& face_id,
                                              const std::string& color) {
  require_document();
  if (!is_valid_appearance_color(color)) {
    throw std::runtime_error("Face color must be an opaque #RRGGBB value");
  }
  const auto owner_id = owner_body_id_from_face_id(face_id);
  const auto face = resolve_face_for_appearance(*document_, face_id);
  if (!owner_id.has_value() || !compiled_body_exists(*document_, owner_id.value())) {
    throw std::runtime_error("Face not found: " + face_id);
  }
  const std::string signature = face.has_value()
                                    ? appearance_face_signature(face.value())
                                    : "semantic:" + face_id;
  if (signature.empty()) {
    throw std::runtime_error("Face appearance signature could not be resolved");
  }

  push_undo_state();
  clear_redo_stack();
  auto& overrides = document_->appearance.face_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const FaceAppearanceOverride& entry) {
        return entry.face_id == face_id;
      });
  if (existing == overrides.end()) {
    overrides.push_back(FaceAppearanceOverride{
        .face_id = face_id,
        .owner_body_id = owner_id.value(),
        .signature = signature,
        .color = color,
    });
  } else {
    existing->owner_body_id = owner_id.value();
    existing->signature = signature;
    existing->color = color;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_body_color(const std::string& body_id) {
  require_document();
  auto& overrides = document_->appearance.body_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const BodyAppearanceOverride& entry) {
        return entry.body_id == body_id;
      });
  if (existing == overrides.end()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  overrides.erase(std::remove_if(overrides.begin(),
                                 overrides.end(),
                                 [&](const BodyAppearanceOverride& entry) {
                                   return entry.body_id == body_id;
                                 }),
                  overrides.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_face_color(const std::string& face_id) {
  require_document();
  auto& overrides = document_->appearance.face_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const FaceAppearanceOverride& entry) {
        return entry.face_id == face_id;
      });
  if (existing == overrides.end()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  overrides.erase(std::remove_if(overrides.begin(),
                                 overrides.end(),
                                 [&](const FaceAppearanceOverride& entry) {
                                   return entry.face_id == face_id;
                                 }),
                  overrides.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_appearance_overrides() {
  require_document();
  if (document_->appearance.body_colors.empty() &&
      document_->appearance.face_colors.empty()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  document_->appearance.body_colors.clear();
  document_->appearance.face_colors.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_offset_plane(
    const std::string& feature_id, double offset) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "construction_plane" ||
      !feature_it->construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "update_offset_plane requires a construction_plane feature: " +
        feature_id);
  }
  if (feature_it->construction_plane_parameters->plane_type != "offset") {
    throw std::runtime_error(
        "update_offset_plane requires an offset construction plane: " +
        feature_id);
  }

  // Resolve the source frame from the rest of the document (everything
  // before this feature in feature_history is already up to date by
  // the time we get here, and the source must come from earlier in
  // history per `is_selectable_plane_reference`).
  const auto& params = feature_it->construction_plane_parameters.value();
  const auto frame =
      resolve_plane_source_frame(*document_, params.source_plane_id);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Offset plane source could not be resolved: " + params.source_plane_id);
  }

  // No push_undo_state here: this is a live-preview update on the
  // freshly-created construction plane whose own create_offset_plane
  // already pushed an undo step. Pushing here would make a panel
  // session of N debounced offset edits collapse to N user-visible
  // undo steps when the user really wants Cancel to revert the
  // entire session. Mirrors the same comment + rationale in
  // `update_fillet_edges` / `update_fillet_radius`.
  clear_redo_stack();
  polysmith::core::update_construction_plane(*feature_it, offset, frame.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_angle_plane(
    const std::string& feature_id, double angle_degrees) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "construction_plane" ||
      !feature_it->construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "update_angle_plane requires a construction_plane feature: " +
        feature_id);
  }
  if (feature_it->construction_plane_parameters->plane_type != "angle") {
    throw std::runtime_error(
        "update_angle_plane requires an angle construction plane: " +
        feature_id);
  }

  const auto& params = feature_it->construction_plane_parameters.value();
  const auto source_frame =
      resolve_plane_source_frame(*document_, params.source_plane_id);
  const auto axis =
      resolve_angle_plane_axis(*document_, params.source_axis_id);
  if (!source_frame.has_value()) {
    throw std::runtime_error(
        "Angle plane source could not be resolved: " + params.source_plane_id);
  }
  if (!axis.has_value()) {
    throw std::runtime_error(
        "Angle plane axis could not be resolved: " + params.source_axis_id);
  }

  clear_redo_stack();
  polysmith::core::update_angle_plane(
      *feature_it, angle_degrees, source_frame.value(), axis.value());
  bump_geometry_revision();
  return document_.value();
}
