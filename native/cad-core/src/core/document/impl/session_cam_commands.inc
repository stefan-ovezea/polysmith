void DocumentManager::save_document_to_path(const std::string& file_path) const {
  require_document();
  if (file_path.empty()) {
    throw std::runtime_error("Save path cannot be empty");
  }

  const nlohmann::json payload = polysmith::protocol::to_payload(document_.value());

  std::ofstream stream(file_path);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file for writing: " + file_path);
  }
  stream << payload.dump(2);
  if (!stream.good()) {
    throw std::runtime_error("Failed to write document to: " + file_path);
  }
}

DocumentState DocumentManager::load_document_from_path(
    const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Load path cannot be empty");
  }

  std::ifstream stream(file_path);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file for reading: " + file_path);
  }
  std::stringstream buffer;
  buffer << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    throw std::runtime_error("Failed to read document from: " + file_path);
  }

  nlohmann::json payload;
  try {
    payload = nlohmann::json::parse(buffer.str());
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Document parse error: ") +
                             error.what());
  }

  DocumentState loaded = polysmith::protocol::document_from_payload(payload);

  // Saved profile lists can be stale — saved by an older build whose
  // arrangement missed a region, or saved mid-edit.  Recompute every
  // sketch's profiles from its geometry so surfaces are selectable and
  // extrudable immediately after load.
  for (auto& feature : loaded.feature_history) {
    if (feature.kind == "sketch") {
      polysmith::core::refresh_sketch_profiles(feature);
    }
  }
  // Drop profile selections whose ids no longer exist after the
  // recompute (ids embed the entity set, which can change).
  if (loaded.selected_sketch_profile_id.has_value() ||
      !loaded.selected_sketch_profile_ids.empty()) {
    std::set<std::string> live_ids;
    for (const auto& feature : loaded.feature_history) {
      if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
        continue;
      }
      for (const auto& profile : feature.sketch_parameters->profiles) {
        live_ids.insert(profile.id);
      }
    }
    if (loaded.selected_sketch_profile_id.has_value() &&
        !live_ids.count(loaded.selected_sketch_profile_id.value())) {
      loaded.selected_sketch_profile_id.reset();
    }
    loaded.selected_sketch_profile_ids.erase(
        std::remove_if(loaded.selected_sketch_profile_ids.begin(),
                       loaded.selected_sketch_profile_ids.end(),
                       [&](const std::string& id) { return !live_ids.count(id); }),
        loaded.selected_sketch_profile_ids.end());
  }

  // Replace the live document with the loaded one. Clear undo/redo, since
  // their previous contents reference a different document timeline.
  document_ = loaded;
  undo_stack_.clear();
  redo_stack_.clear();
  if (document_count_ == 0) {
    document_count_ = 1;
  }

  // Restore id counters so subsequent feature/sketch additions don't
  // collide with ids already present in the loaded document. Walk the
  // feature history and bump every counter to one past the highest seen
  // value.
  next_document_id_ = std::max(next_document_id_, trailing_integer(loaded.id) + 1);

  for (const auto& feature : loaded.feature_history) {
    next_feature_id_ = std::max(next_feature_id_, trailing_integer(feature.id) + 1);

    if (!feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& line : sketch.lines) {
      next_sketch_line_id_ =
          std::max(next_sketch_line_id_, trailing_integer(line.id) + 1);
    }
    for (const auto& circle : sketch.circles) {
      next_sketch_circle_id_ =
          std::max(next_sketch_circle_id_, trailing_integer(circle.id) + 1);
    }
    for (const auto& arc : sketch.arcs) {
      next_sketch_arc_id_ =
          std::max(next_sketch_arc_id_, trailing_integer(arc.id) + 1);
    }
    for (const auto& fillet : sketch.fillets) {
      next_sketch_fillet_id_ =
          std::max(next_sketch_fillet_id_, trailing_integer(fillet.id) + 1);
    }
    for (const auto& projected : sketch.projected_points) {
      next_sketch_projected_point_id_ = std::max(
          next_sketch_projected_point_id_,
          trailing_integer(projected.id) + 1);
    }
    for (const auto& projection : sketch.projections) {
      next_sketch_projection_id_ = std::max(
          next_sketch_projection_id_,
          trailing_integer(projection.id) + 1);
    }
  }

  if (document_) {
    bump_geometry_revision();
  }

  return document_.value();
}

std::optional<DocumentState> DocumentManager::get_document() const {
  return document_;
}

SessionState DocumentManager::get_session_state() const {
  return SessionState{
      .document_count = document_count_,
      .active_document_id =
          document_.has_value() ? std::make_optional(document_->id) : std::nullopt,
      .can_undo = !undo_stack_.empty(),
      .can_redo = !redo_stack_.empty(),
  };
}

// ── CAM ─────────────────────────────────────────────────────
//
// CAM workspace is being rebuilt on new target-schema types
// (cam_types.h).  All state lives in DocumentState::cam.
// These methods are stubs — they return the current document
// unchanged.  Real implementations will be added when the CAM
// workspace is rewritten.

DocumentState DocumentManager::cam_setup_create(const CamSetup&) {
  require_document();
  return document_.value();
}

std::optional<CamSetup> DocumentManager::cam_setup_get() const {
  if (!document_.has_value() || document_->cam.setups.empty())
    return std::nullopt;
  return document_->cam.setups[0];
}

DocumentState DocumentManager::cam_setup_update(const CamSetup&) {
  require_document();
  return document_.value();
}

DocumentState DocumentManager::cam_tool_add(const ToolEntry&) {
  require_document();
  return document_.value();
}

DocumentState DocumentManager::cam_tool_update(const std::string&, const ToolEntry&) {
  require_document();
  return document_.value();
}

DocumentState DocumentManager::cam_tool_delete(const std::string&) {
  require_document();
  return document_.value();
}

std::vector<ToolEntry> DocumentManager::cam_tool_list() const {
  if (!document_.has_value()) return {};
  return document_->cam.tool_library;
}

DocumentState DocumentManager::cam_operation_add(const CamOperation&) {
  require_document();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_update(const std::string&, const CamOperation&) {
  require_document();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_delete(const std::string&) {
  require_document();
  return document_.value();
}
