DocumentState DocumentManager::create_chamfer(
    const std::vector<std::string>& edge_ids, double distance) {
  require_document();

  if (distance <= 0.0) {
    throw std::runtime_error("Chamfer distance must be greater than zero");
  }
  if (edge_ids.empty()) {
    throw std::runtime_error("Chamfer requires at least one edge");
  }

  // Same single-body validation as create_fillet — see comment there
  // for why mixed-body selections aren't representable.
  const std::string owner_id = edge_owner_id(edge_ids.front());
  if (owner_id.empty()) {
    throw std::runtime_error("Malformed edge id: " + edge_ids.front());
  }
  for (const auto& edge_id : edge_ids) {
    const std::string id_owner = edge_owner_id(edge_id);
    if (id_owner.empty()) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (id_owner != owner_id) {
      throw std::runtime_error(
          "Chamfer edges must all belong to the same body");
    }
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Edge owner not found: " + owner_id);
  }

  push_undo_state();
  clear_redo_stack();

  ChamferFeatureParameters params{};
  params.target_body_id = owner_id;
  params.edge_ids = edge_ids;
  params.distance = distance;
  // Same rationale as create_fillet — see comment there.
  params.is_pending = true;

  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · " << distance
          << " mm";

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "chamfer",
      .name = "Chamfer",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = std::nullopt,
      .chamfer_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  // Same rationale as create_fillet: keep the chamfered edges
  // highlighted while the floating panel is open. See comment there.
  document_->selected_edge_ids = edge_ids;
  document_->selected_vertex_ids.clear();
  document_->selected_face_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_chamfer_edges(
    const std::string& feature_id,
    const std::vector<std::string>& edge_ids) {
  require_document();

  if (edge_ids.empty()) {
    throw std::runtime_error("Chamfer must keep at least one edge");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "chamfer" ||
      !feature_it->chamfer_parameters.has_value()) {
    throw std::runtime_error(
        "update_chamfer_edges requires a chamfer feature: " + feature_id);
  }

  // See update_fillet_edges for the same-body invariant rationale.
  const std::string& target = feature_it->chamfer_parameters->target_body_id;
  for (const auto& edge_id : edge_ids) {
    const auto separator = edge_id.find(":edge:");
    if (separator == std::string::npos || separator == 0) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (edge_id.substr(0, separator) != target) {
      throw std::runtime_error(
          "Chamfer edges must all belong to the same body");
    }
  }

  // See update_fillet_edges for why this command intentionally does
  // not push an undo step.
  feature_it->chamfer_parameters->edge_ids = edge_ids;
  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · "
          << feature_it->chamfer_parameters->distance << " mm";
  feature_it->parameters_summary = summary.str();
  document_->selected_edge_ids = edge_ids;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_chamfer_distance(
    const std::string& feature_id, double distance) {
  require_document();

  if (distance <= 0.0) {
    throw std::runtime_error("Chamfer distance must be greater than zero");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "chamfer" ||
      !feature_it->chamfer_parameters.has_value()) {
    throw std::runtime_error(
        "update_chamfer_distance requires a chamfer feature: " + feature_id);
  }

  // See update_fillet_edges for why this command intentionally does
  // not push an undo step.
  feature_it->chamfer_parameters->distance = distance;
  std::ostringstream summary;
  summary << feature_it->chamfer_parameters->edge_ids.size() << " edge"
          << (feature_it->chamfer_parameters->edge_ids.size() == 1 ? "" : "s")
          << " · " << distance << " mm";
  feature_it->parameters_summary = summary.str();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_chamfer(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "chamfer" ||
      !feature_it->chamfer_parameters.has_value()) {
    throw std::runtime_error(
        "confirm_chamfer requires a chamfer feature: " + feature_id);
  }

  if (!feature_it->chamfer_parameters->is_pending) {
    return document_.value();
  }

  // See `confirm_fillet` for why this does not push an undo step.
  feature_it->chamfer_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}
