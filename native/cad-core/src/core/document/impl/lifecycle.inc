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
  normalize_timeline_cursor(*document_);
  // Increment the revision via a local before assigning back so the
  // literal `document_->revision += 1;` doesn't appear here — that
  // way mass-renaming call sites won't accidentally rewrite this
  // implementation.
  const int next = document_->revision + 1;
  document_->revision = next;
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

DocumentState DocumentManager::create_document() {
  DocumentState document{
      .id = "doc-" + std::to_string(next_document_id_++),
      .name = "Untitled Part",
      .units = "mm",
      .revision = 1,
      .selected_feature_id = std::nullopt,
      .selected_reference_id = std::nullopt,
      .selected_face_id = std::nullopt,
      .selected_edge_ids = {},
      .selected_vertex_ids = {},
      .active_sketch_plane_id = std::nullopt,
      .active_sketch_face_id = std::nullopt,
      .active_sketch_feature_id = std::nullopt,
      .active_sketch_tool = std::nullopt,
      .selected_sketch_point_id = std::nullopt,
      .selected_sketch_entity_id = std::nullopt,
      .selected_sketch_dimension_id = std::nullopt,
      .selected_sketch_profile_id = std::nullopt,
      .selected_sketch_profile_ids = {},
      .timeline_cursor = std::nullopt,
      .feature_history = {make_root_feature()},
      .parameters = {},
      .appearance = {},
      .selection_filter = SelectionFilter{},
  };

  document_ = document;
  document_count_ = 1;
  undo_stack_.clear();
  redo_stack_.clear();
  return document;
}

DocumentState DocumentManager::add_box_feature(
    const BoxFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_box_feature(next_feature_id_++, parameters));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_cylinder_feature(
    const CylinderFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_cylinder_feature(next_feature_id_++, parameters));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_box_feature(
    const std::string& feature_id, const BoxFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_box_feature(*feature_it, parameters);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_cylinder_feature(
    const std::string& feature_id,
    const CylinderFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_cylinder_feature(*feature_it, parameters);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_depth(
    const std::string& feature_id, double depth) {
  require_document();
  if (depth == 0.0) {
    return document_.value();
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_extrude_depth(*feature_it, depth);
  if (feature_it->extrude_parameters.has_value()) {
    feature_it->extrude_parameters->extent_mode = "one_side";
    feature_it->extrude_parameters->side1.extent_type = "distance";
    feature_it->extrude_parameters->side1.distance = std::abs(depth);
    feature_it->extrude_parameters->side2 = std::nullopt;
    normalize_extrude_parameters(*document_, feature_it->extrude_parameters.value());
    apply_extrude_parameters_with_preview_validation(
        *feature_it, feature_it->extrude_parameters.value());
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_mode(
    const std::string& feature_id, const std::string& mode) {
  require_document();

  if (mode != "new_body" && mode != "join" && mode != "cut" &&
      mode != "intersect") {
    throw std::runtime_error("Unsupported extrude mode: " + mode);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind != "extrude" ||
      !feature_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_mode requires an extrude feature: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->extrude_parameters->mode = mode;
  feature_it->extrude_parameters->operation = mode;
  normalize_extrude_operation_mode(feature_it->extrude_parameters.value());
  normalize_extrude_parameters(*document_, feature_it->extrude_parameters.value());
  apply_extrude_parameters_with_preview_validation(
      *feature_it, feature_it->extrude_parameters.value());
  if (feature_it->name == "Extrude" || feature_it->name == "Body") {
    feature_it->name = mode == "new_body" ? "Body" : "Extrude";
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_target_body(
    const std::string& feature_id,
    const std::optional<std::string>& target_body_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind != "extrude" ||
      !feature_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_target_body requires an extrude feature: " +
        feature_id);
  }

  // Target ids that point at the extrude itself (or at a feature that
  // does not exist) are silently coerced to nullopt so the body compiler
  // simply falls back to "most recent body".
  std::optional<std::string> resolved = target_body_id;
  if (resolved.has_value()) {
    if (resolved.value() == feature_id) {
      resolved = std::nullopt;
    } else {
      bool exists = false;
      for (const auto& other : document_->feature_history) {
        if (other.id == resolved.value()) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        resolved = std::nullopt;
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->extrude_parameters->target_body_id = resolved;
  normalize_extrude_operation_mode(feature_it->extrude_parameters.value());
  normalize_extrude_parameters(*document_, feature_it->extrude_parameters.value());
  apply_extrude_parameters_with_preview_validation(
      *feature_it, feature_it->extrude_parameters.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_parameters(
    const std::string& feature_id,
    const ExtrudeFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "extrude" ||
      !feature_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_parameters requires an extrude feature: " + feature_id);
  }

  ExtrudeFeatureParameters next = parameters;
  preserve_extrude_source_geometry(next, feature_it->extrude_parameters.value());
  if (next.depth == 0.0 || next.side1.distance == 0.0 ||
      (next.side2.has_value() && next.side2->distance == 0.0)) {
    return document_.value();
  }
  normalize_extrude_operation_mode(next);
  normalize_extrude_parameters(*document_, next);

  push_undo_state();
  clear_redo_stack();
  apply_extrude_parameters_with_preview_validation(*feature_it, next);
  if (feature_it->name == "Extrude" || feature_it->name == "Body") {
    feature_it->name = next.mode == "new_body" ? "Body" : "Extrude";
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::rename_feature(const std::string& feature_id,
                                              const std::string& name) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Feature name cannot be empty");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->name = name;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_feature_suppressed(
    const std::string& feature_id, bool suppressed) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be suppressed");
  }

  // No-op early exit so we don't pollute undo with a redundant entry
  // when the UI's "Suppress" button is double-clicked.
  if (feature_it->suppressed == suppressed) {
    return document_.value();
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->suppressed = suppressed;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be deleted");
  }

  if (feature_it->kind == "sketch" &&
      document_->active_sketch_feature_id.has_value() &&
      document_->active_sketch_feature_id.value() == feature_id) {
    throw std::runtime_error("Finish the active sketch before deleting it");
  }

  push_undo_state();
  clear_redo_stack();

  const bool deleted_selected =
      document_->selected_feature_id.has_value() &&
      document_->selected_feature_id.value() == feature_id;

  document_->feature_history.erase(feature_it);
  if (deleted_selected) {
    document_->selected_feature_id = std::nullopt;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::undo() {
  require_document();

  if (undo_stack_.empty()) {
    throw std::runtime_error("Nothing to undo");
  }

  redo_stack_.push_back(document_.value());
  document_ = undo_stack_.back();
  undo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::redo() {
  require_document();

  if (redo_stack_.empty()) {
    throw std::runtime_error("Nothing to redo");
  }

  undo_stack_.push_back(document_.value());
  document_ = redo_stack_.back();
  redo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::set_timeline_cursor(int included_action_count) {
  require_document();

  const int max_actions = action_count(document_.value());
  const int clamped =
      std::max(0, std::min(included_action_count, max_actions));

  if (clamped >= max_actions) {
    document_->timeline_cursor = std::nullopt;
  } else {
    document_->timeline_cursor = clamped;
  }

  if (document_->selected_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_feature_id.value())) {
    document_->selected_feature_id = std::nullopt;
  }
  if (document_->selected_reference_id.has_value() &&
      !is_origin_plane_reference(document_->selected_reference_id.value()) &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_reference_id.value())) {
    document_->selected_reference_id = std::nullopt;
  }
  if (document_->active_sketch_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->active_sketch_feature_id.value())) {
    document_->active_sketch_feature_id = std::nullopt;
    document_->active_sketch_plane_id = std::nullopt;
    document_->active_sketch_face_id = std::nullopt;
    document_->active_sketch_tool = std::nullopt;
  }
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_profile_ids.clear();

  document_->revision += 1;
  return document_.value();
}
