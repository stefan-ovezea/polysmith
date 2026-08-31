void DocumentManager::require_document() const {
  if (!document_.has_value()) {
    throw std::runtime_error("No active document");
  }
}

void DocumentManager::push_undo_state() {
  require_document();
  undo_stack_.push_back(document_.value());
}

void DocumentManager::bump_geometry_revision() {
  require_document();
  refresh_history_dependencies(*document_);
  // CAM operations re-resolve their geometry references against the
  // live document after every geometry bump — the CAM analogue of the
  // history dependency pass.  Early-outs when there are no operations.
  // The revision increments after this pass, so hand it the target.
  refresh_cam_dependencies(*document_, document_->revision + 1);
  normalize_timeline_cursor(*document_);
  // Prune selections that reference ids which no longer exist after
  // the mutation that triggered this bump (trim-as-delete, deleted
  // dimensions/fillets/texts, zero-length cleanup, ...). Pure id-set
  // membership — no body compilation.
  prune_document_selection();
  // Increment the revision via a local before assigning back so the
  // literal `document_->revision += 1;` doesn't appear here — that
  // way mass-renaming call sites won't accidentally rewrite this
  // implementation.
  const int next = document_->revision + 1;
  document_->revision = next;
}

void DocumentManager::prune_document_selection() {
  // Build the set of live sketch ids: entities, their endpoints and
  // derived dimension ids, vertices, dimensions, fillets, texts,
  // profiles, and projected points across every sketch feature.
  std::unordered_set<std::string> live;
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& line : sketch.lines) {
      live.insert(line.id);
      live.insert(line.start_vertex_id);
      live.insert(line.end_vertex_id);
    }
    for (const auto& circle : sketch.circles) {
      live.insert(circle.id);
      live.insert(circle.center_vertex_id);
    }
    for (const auto& arc : sketch.arcs) {
      live.insert(arc.id);
      live.insert(arc.start_vertex_id);
      live.insert(arc.end_vertex_id);
      live.insert(arc.center_vertex_id);
    }
    for (const auto& polygon : sketch.polygons) {
      live.insert(polygon.id);
    }
    for (const auto& vertex : sketch.vertices) {
      live.insert(vertex.id);
      if (!vertex.vertex_id.empty()) {
        live.insert(vertex.vertex_id);
      }
    }
    for (const auto& dimension : sketch.dimensions) {
      live.insert(dimension.id);
    }
    for (const auto& fillet : sketch.fillets) {
      live.insert(fillet.id);
    }
    for (const auto& text : sketch.texts) {
      live.insert(text.id);
    }
    for (const auto& profile : sketch.profiles) {
      live.insert(profile.id);
    }
    for (const auto& projected : sketch.projected_points) {
      live.insert(projected.id);
    }
    for (const auto& projection : sketch.projections) {
      live.insert(projection.id);
    }
  }

  const auto is_live = [&](const std::string& id) {
    return live.count(id) != 0;
  };
  const auto erase_dead = [&](std::vector<std::string>& ids) {
    ids.erase(
        std::remove_if(ids.begin(), ids.end(),
                       [&](const std::string& id) { return !is_live(id); }),
        ids.end());
  };

  if (document_->selected_sketch_vertex_id.has_value() &&
      !is_live(document_->selected_sketch_vertex_id.value())) {
    document_->selected_sketch_vertex_id.reset();
  }
  if (document_->selected_sketch_entity_id.has_value() &&
      !is_live(document_->selected_sketch_entity_id.value())) {
    document_->selected_sketch_entity_id.reset();
  }
  if (document_->selected_sketch_dimension_id.has_value() &&
      !is_live(document_->selected_sketch_dimension_id.value())) {
    document_->selected_sketch_dimension_id.reset();
  }
  if (document_->selected_sketch_text_id.has_value() &&
      !is_live(document_->selected_sketch_text_id.value())) {
    document_->selected_sketch_text_id.reset();
  }
  if (document_->selected_sketch_profile_id.has_value() &&
      !is_live(document_->selected_sketch_profile_id.value())) {
    document_->selected_sketch_profile_id.reset();
  }
  erase_dead(document_->selected_sketch_entity_ids);
  erase_dead(document_->selected_sketch_vertex_ids);
  erase_dead(document_->selected_sketch_profile_ids);

  // Re-derive the singular id from the pruned plural list (singular =
  // most recent, mirroring the select_sketch_* semantics). Mutators
  // set selection BEFORE bump_geometry_revision, so this runs last.
  if (!document_->selected_sketch_entity_id.has_value() &&
      !document_->selected_sketch_entity_ids.empty()) {
    document_->selected_sketch_entity_id =
        document_->selected_sketch_entity_ids.back();
  }
  if (!document_->selected_sketch_vertex_id.has_value() &&
      !document_->selected_sketch_vertex_ids.empty()) {
    document_->selected_sketch_vertex_id =
        document_->selected_sketch_vertex_ids.back();
  }
  if (!document_->selected_sketch_profile_id.has_value() &&
      !document_->selected_sketch_profile_ids.empty()) {
    document_->selected_sketch_profile_id =
        document_->selected_sketch_profile_ids.back();
  }
}

void DocumentManager::clear_redo_stack() {
  redo_stack_.clear();
}

FeatureEntry DocumentManager::make_root_feature() {
  return FeatureEntry{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "root_part",
      .name = "Base Part",
      .status = "healthy",
      .parameters_summary = "Document root",
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
  };
}
