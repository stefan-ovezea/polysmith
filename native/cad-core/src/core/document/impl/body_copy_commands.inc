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
