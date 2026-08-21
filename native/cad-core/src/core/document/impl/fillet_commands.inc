DocumentState DocumentManager::create_fillet(
    const std::vector<std::string>& edge_ids, double radius) {
  require_document();

  if (radius <= 0.0) {
    throw std::runtime_error("Fillet radius must be greater than zero");
  }
  if (edge_ids.empty()) {
    throw std::runtime_error("Fillet requires at least one edge");
  }

  // Validate every edge is well-formed and that all share the same
  // owner body — OCCT's fillet builder operates on a single shape, so
  // mixing edges from multiple bodies has no well-defined semantics.
  // We surface this as an error rather than silently slicing the
  // selection.
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
          "Fillet edges must all belong to the same body");
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

  FilletFeatureParameters params{};
  params.target_body_id = owner_id;
  params.edge_ids = edge_ids;
  params.radius = radius;
  // Born pending so body_compiler retains the pre-fillet shape for
  // edge picking until the UI calls `confirm_fillet`. See
  // FilletFeatureParameters::is_pending.
  params.is_pending = true;

  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · " << radius
          << " mm";

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "fillet",
      .name = "Fillet",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = params,
      .chamfer_parameters = std::nullopt,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  // Keep the picked edges highlighted while the floating panel is
  // open: the body_compiler highlights edges that are in
  // selected_edge_ids, and the panel's "edit edges" interaction
  // (update_fillet_edges) keeps this set in sync. The UI's Confirm
  // path is responsible for clearing the selection once the user is
  // done; Cancel goes through undo() which restores the prior set.
  document_->selected_edge_ids = edge_ids;
  document_->selected_vertex_ids.clear();
  document_->selected_face_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fillet_edges(
    const std::string& feature_id,
    const std::vector<std::string>& edge_ids) {
  require_document();

  if (edge_ids.empty()) {
    throw std::runtime_error("Fillet must keep at least one edge");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "fillet" ||
      !feature_it->fillet_parameters.has_value()) {
    throw std::runtime_error(
        "update_fillet_edges requires a fillet feature: " + feature_id);
  }

  // Every new edge must belong to the feature's existing target body
  // — see create_fillet for why mixed-body sets aren't representable.
  // We compare against the stored target_body_id rather than parsing
  // the first edge's owner so the feature's body identity stays
  // authoritative across edge swaps.
  const std::string& target = feature_it->fillet_parameters->target_body_id;
  for (const auto& edge_id : edge_ids) {
    const auto separator = edge_id.find(":edge:");
    if (separator == std::string::npos || separator == 0) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (edge_id.substr(0, separator) != target) {
      throw std::runtime_error(
          "Fillet edges must all belong to the same body");
    }
  }

  // No push_undo_state here: this is a live-preview update on the
  // freshly-created fillet whose own create_fillet already pushed an
  // undo step. Pushing here would make a panel session of N edge
  // toggles and M radius edits collapse to a single user-visible
  // feature but produce N+M+1 undo steps, so Cancel → undo() would
  // only revert the last edit instead of the whole session. The
  // create_* push covers the entire session.
  feature_it->fillet_parameters->edge_ids = edge_ids;
  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · "
          << feature_it->fillet_parameters->radius << " mm";
  feature_it->parameters_summary = summary.str();
  // Keep the highlight in sync with the live edge set so the user
  // sees what's being filleted while picking.
  document_->selected_edge_ids = edge_ids;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_fillet(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "fillet" ||
      !feature_it->fillet_parameters.has_value()) {
    throw std::runtime_error(
        "confirm_fillet requires a fillet feature: " + feature_id);
  }

  // Idempotent: confirming an already-confirmed fillet is a no-op so
  // the UI doesn't have to track whether it has called this before.
  if (!feature_it->fillet_parameters->is_pending) {
    return document_.value();
  }

  // No push_undo here — the matching create_fillet's undo step covers
  // the entire pending session, so a single undo() rolls back create
  // + all edits + this confirm in one step.
  feature_it->fillet_parameters->is_pending = false;
  // The pending session highlights its edges via selected_edge_ids;
  // confirming clears them core-side (Cancel instead restores the
  // pre-session set through undo()).
  document_->selected_edge_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fillet_radius(
    const std::string& feature_id, double radius) {
  require_document();

  if (radius <= 0.0) {
    throw std::runtime_error("Fillet radius must be greater than zero");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "fillet" || !feature_it->fillet_parameters.has_value()) {
    throw std::runtime_error(
        "update_fillet_radius requires a fillet feature: " + feature_id);
  }

  // See update_fillet_edges for why this command intentionally does
  // not push an undo step.
  feature_it->fillet_parameters->radius = radius;
  std::ostringstream summary;
  summary << feature_it->fillet_parameters->edge_ids.size() << " edge"
          << (feature_it->fillet_parameters->edge_ids.size() == 1 ? "" : "s")
          << " · " << radius << " mm";
  feature_it->parameters_summary = summary.str();
  bump_geometry_revision();
  return document_.value();
}
