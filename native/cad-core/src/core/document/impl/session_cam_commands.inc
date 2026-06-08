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

// ── CAM Setup ──────────────────────────────────────────────────

DocumentState DocumentManager::cam_setup_create(const CamSetup& setup) {
  require_document();
  document_->cam_setup = setup;
  bump_geometry_revision();
  return document_.value();
}

std::optional<CamSetup> DocumentManager::cam_setup_get() const {
  if (!document_.has_value()) return std::nullopt;
  return document_->cam_setup;
}

DocumentState DocumentManager::cam_setup_update(const CamSetup& setup) {
  require_document();
  document_->cam_setup = setup;
  bump_geometry_revision();
  return document_.value();
}

// ── CAM Tool Library ───────────────────────────────────────────

DocumentState DocumentManager::cam_tool_add(const CamToolDefinition& tool) {
  require_document();
  document_->tool_library.push_back(tool);
  return document_.value();
}

DocumentState DocumentManager::cam_tool_update(const std::string& toolId,
                                                const CamToolDefinition& tool) {
  require_document();
  for (auto& t : document_->tool_library) {
    if (t.id == toolId) {
      t = tool;
      break;
    }
  }
  return document_.value();
}

DocumentState DocumentManager::cam_tool_delete(const std::string& toolId) {
  require_document();
  auto& tools = document_->tool_library;
  tools.erase(std::remove_if(tools.begin(), tools.end(),
                             [&](const CamToolDefinition& t) { return t.id == toolId; }),
              tools.end());
  return document_.value();
}

std::vector<CamToolDefinition> DocumentManager::cam_tool_list() const {
  if (!document_.has_value()) return {};
  return document_->tool_library;
}

// ── CAM Operations ─────────────────────────────────────────────

DocumentState DocumentManager::cam_operation_add(const CamOperationEntry& op) {
  require_document();
  document_->cam_operations.push_back(op);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_update(
    const std::string& operation_id, const CamOperationEntry& updated) {
  require_document();
  for (auto& op : document_->cam_operations) {
    if (op.id == operation_id) {
      // Preserve TNP witness data, update user-editable fields.
      op.name = updated.name;
      op.toolId = updated.toolId;
      op.faceMilling = updated.faceMilling;
      bump_geometry_revision();
      return document_.value();
    }
  }
  return document_.value();
}

DocumentState DocumentManager::cam_operation_delete(const std::string& operation_id) {
  require_document();
  auto& ops = document_->cam_operations;
  ops.erase(
      std::remove_if(ops.begin(), ops.end(),
                     [&](const CamOperationEntry& op) { return op.id == operation_id; }),
      ops.end());
  bump_geometry_revision();
  return document_.value();
}

