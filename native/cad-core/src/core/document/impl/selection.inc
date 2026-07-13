DocumentState DocumentManager::select_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  document_->selected_feature_id = feature_id;
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
  return document_.value();
}

DocumentState DocumentManager::select_reference(const std::string& reference_id) {
  require_document();

  if (!is_selectable_plane_reference(*document_, reference_id)) {
    throw std::runtime_error("Reference not found: " + reference_id);
  }

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
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
  return document_.value();
}

DocumentState DocumentManager::start_sketch_on_plane(
    const std::string& reference_id) {
  require_document();

  if (!is_selectable_plane_reference(*document_, reference_id)) {
    throw std::runtime_error("Sketch plane not found: " + reference_id);
  }

  push_undo_state();
  clear_redo_stack();

  // Origin reference planes have hardcoded mappings throughout the
  // existing code paths, so we omit `plane_frame` and let the
  // legacy fallbacks take over (matching the historical behaviour).
  // Construction-plane sketches must carry a `plane_frame` because
  // there are no hardcoded mappings for arbitrary frames; we read
  // the cached frame off the source feature.
  std::optional<SketchFeatureParameters::SketchPlaneFrame> sketch_frame;
  if (!is_origin_plane_reference(reference_id)) {
    const auto frame = resolve_plane_source_frame(*document_, reference_id);
    if (!frame.has_value()) {
      throw std::runtime_error(
          "Sketch plane source could not be resolved: " + reference_id);
    }
    sketch_frame = to_sketch_plane_frame(frame.value());
  }

  document_->feature_history.push_back(create_sketch_feature(
      next_feature_id_++, reference_id, sketch_frame));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = reference_id;
  document_->active_sketch_face_id = std::nullopt;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
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

DocumentState DocumentManager::select_face(const std::string& face_id) {
  require_document();

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Face not found: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Face owner not found: " + face_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_edge(const std::string& edge_id,
                                            bool additive) {
  require_document();

  // Edge ids are minted by the viewport as "<owner_body_id>:edge:<index>".
  // We don't validate the index — the body's edge enumeration may shift
  // by topology so a stale id from the UI just becomes a no-op highlight.
  // The owner body id, however, must point at a real feature so we can
  // also bring the body into focus (selected_feature_id) for the
  // hierarchy panel and downstream actions like fillet/chamfer.
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed edge id: " + edge_id);
  }
  const std::string owner_id = edge_id.substr(0, separator);

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Edge owner not found: " + edge_id);
  }

  // Multi-select semantics: shift-click toggles, plain click replaces.
  // Toggling preserves the rest of the selection so users can build up
  // a multi-edge set incrementally for fillet / chamfer. Adding an
  // edge from a different body is allowed at the storage layer; the
  // fillet / chamfer creators are the ones that reject mixed-body
  // selections, since OCCT expects a single target body per feature.
  if (additive) {
    const auto existing = std::find(document_->selected_edge_ids.begin(),
                                    document_->selected_edge_ids.end(),
                                    edge_id);
    if (existing != document_->selected_edge_ids.end()) {
      document_->selected_edge_ids.erase(existing);
    } else {
      document_->selected_edge_ids.push_back(edge_id);
    }
  } else {
    document_->selected_edge_ids = {edge_id};
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_vertex(const std::string& vertex_id,
                                              bool additive) {
  require_document();

  // Vertex ids are minted by the viewport as
  // "<owner_body_id>:vertex:<index>". Same lenience as select_edge:
  // we don't validate the index since topology can shift, but the
  // owner body id must point at a real feature so the hierarchy
  // panel highlights the correct body and downstream actions can
  // resolve the body via selected_feature_id.
  const auto separator = vertex_id.find(":vertex:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed vertex id: " + vertex_id);
  }
  const std::string owner_id = vertex_id.substr(0, separator);

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Vertex owner not found: " + vertex_id);
  }

  // Toggle / replace mirror of select_edge: shift-click adds or
  // removes; plain click replaces. Two-vertex selections drive the
  // distance readout in the UI's Selection panel.
  if (additive) {
    const auto existing = std::find(document_->selected_vertex_ids.begin(),
                                    document_->selected_vertex_ids.end(),
                                    vertex_id);
    if (existing != document_->selected_vertex_ids.end()) {
      document_->selected_vertex_ids.erase(existing);
    } else {
      document_->selected_vertex_ids.push_back(vertex_id);
    }
  } else {
    document_->selected_vertex_ids = {vertex_id};
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

namespace {

std::string edge_owner_id(const std::string& edge_id) {
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos) {
    return "";
  }
  return edge_id.substr(0, separator);
}

}  // namespace
