void DocumentManager::require_document() const {
  if (!document_.has_value()) {
    throw std::runtime_error("No active document");
  }
}

// Derives the default step name from the pushing method.  The
// source_location function_name comes from the CALL SITE (the
// defaulted argument is evaluated there): on MSVC it carries the
// calling convention and full signature, on GCC/Clang the return type
// and argument list.  Cut at the FIRST '(' before looking for the
// scope separator — the parameter list itself contains "::" on MSVC
// (template arguments like std::allocator), so a raw rfind lands in
// the middle of the signature:
//   "...DocumentManager::add_sketch_line(...)" -> "Add Sketch Line"
// Groups and commands that wrap several internal pushes supply an
// explicit name instead.
std::string undo_step_name_from_location(
    const std::source_location& location) {
  std::string raw(location.function_name());
  const size_t params = raw.find('(');
  if (params != std::string::npos) {
    raw = raw.substr(0, params);
  }
  const size_t scope = raw.rfind("::");
  if (scope != std::string::npos) {
    raw = raw.substr(scope + 2);
  }
  std::string name;
  name.reserve(raw.size());
  bool capitalize = true;
  for (char c : raw) {
    if (c == '_') {
      name.push_back(' ');
      capitalize = true;
    } else if (capitalize) {
      name.push_back(static_cast<char>(std::toupper(
          static_cast<unsigned char>(c))));
      capitalize = false;
    } else {
      name.push_back(c);
    }
  }
  return name;
}

void DocumentManager::push_undo_state(
    const std::source_location& location) {
  require_document();
  // A push means a mutating command ran — the precise "state changed"
  // signal an open group uses to decide whether it leaves an entry
  // (mirrors FreeCAD's potential-transaction).
  if (!undo_group_stack_.empty()) {
    undo_group_stack_.back().changed = true;
  }
  undo_stack_.push_back(document_.value());
  undo_names_.push_back(undo_step_name_from_location(location));
  // Purge the oldest entry when the configured depth is exceeded.
  while (undo_stack_.size() > static_cast<size_t>(max_undo_steps_)) {
    undo_stack_.erase(undo_stack_.begin());
    undo_names_.erase(undo_names_.begin());
  }
  sync_undo_step_names();
}

void DocumentManager::sync_sketch_id_counters() {
  if (!document_.has_value()) {
    return;
  }
  // Numeric suffix of a "kind-N" id; ids without a plain numeric
  // suffix (e.g. "vertex-text-3", "line-glyph-7") don't consume the
  // user counters and are skipped — same rule as the load-time
  // counter restore.
  const auto next_index = [](const std::string& id) {
    const size_t dash = id.rfind('-');
    if (dash == std::string::npos || dash + 1 >= id.size()) {
      return 0;
    }
    const std::string suffix = id.substr(dash + 1);
    if (suffix.find_first_not_of("0123456789") != std::string::npos) {
      return 0;
    }
    return std::stoi(suffix) + 1;
  };
  const auto bump = [](int& counter, int candidate) {
    counter = std::max(counter, candidate);
  };

  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& e : sketch.lines) bump(next_sketch_line_id_, next_index(e.id));
    for (const auto& e : sketch.circles) bump(next_sketch_circle_id_, next_index(e.id));
    for (const auto& e : sketch.polygons) bump(next_sketch_polygon_id_, next_index(e.id));
    for (const auto& e : sketch.arcs) bump(next_sketch_arc_id_, next_index(e.id));
    for (const auto& e : sketch.ellipses) bump(next_sketch_ellipse_id_, next_index(e.id));
    for (const auto& e : sketch.splines) bump(next_sketch_spline_id_, next_index(e.id));
    for (const auto& e : sketch.slots) bump(next_sketch_slot_id_, next_index(e.id));
    for (const auto& e : sketch.fillets) bump(next_sketch_fillet_id_, next_index(e.id));
    for (const auto& e : sketch.chamfers) bump(next_sketch_chamfer_id_, next_index(e.id));
    for (const auto& e : sketch.projected_points) {
      bump(next_sketch_projected_point_id_, next_index(e.id));
    }
    for (const auto& e : sketch.projections) {
      bump(next_sketch_projection_id_, next_index(e.id));
    }
    for (const auto& e : sketch.texts) bump(next_sketch_text_id_, next_index(e.id));
  }
}

void DocumentManager::bump_geometry_revision() {
  require_document();
  sync_sketch_id_counters();
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
  // The undo/redo flags travel with the document payload (D7) so the
  // UI's buttons/hotkeys are never stale — every mutator passes
  // through this choke point.
  document_->can_undo = !undo_stack_.empty();
  document_->can_redo = !redo_stack_.empty();
  sync_undo_step_names();
}

void DocumentManager::sync_undo_step_names() {
  if (!document_.has_value()) {
    return;
  }
  document_->undo_step_names.assign(undo_names_.rbegin(), undo_names_.rend());
  document_->redo_step_names.assign(redo_names_.rbegin(), redo_names_.rend());
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
  redo_names_.clear();
  sync_undo_step_names();
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
