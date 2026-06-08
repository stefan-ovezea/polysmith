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
