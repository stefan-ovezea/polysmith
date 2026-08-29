void DocumentManager::save_document_to_path(const std::string& file_path) const {
  require_document();
  if (file_path.empty()) {
    throw std::runtime_error("Save path cannot be empty");
  }

  const nlohmann::json payload =
      polysmith::protocol::to_payload(document_.value(),
                                      /*include_opaque=*/true);

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

  // Restore CAM id counters so subsequent CAM additions don't collide
  // with ids already present in the loaded document.
  for (const auto& setup : loaded.cam.setups) {
    next_cam_setup_id_ =
        std::max(next_cam_setup_id_, trailing_integer(setup.setup_id) + 1);
  }
  for (const auto& tool : loaded.cam.tool_library) {
    next_cam_tool_id_ =
        std::max(next_cam_tool_id_, trailing_integer(tool.tool_id) + 1);
  }
  for (const auto& op : loaded.cam.operations) {
    next_cam_op_id_ = std::max(next_cam_op_id_, trailing_integer(op.op_id) + 1);
  }

  // Rebuild sketch derived state (solver + profile regions) for every
  // sketch feature.  Saved profiles are restored verbatim by the
  // payload parser and may predate geometry edits or detection fixes —
  // without a refresh a loaded sketch keeps its stale stored profiles
  // and the extrudable regions never regenerate (user-reported: the
  // tangent-line wedge surface missing after reload).  Safe now that
  // the parser restores center_vertex_id and next_vertex_index —
  // vertex ids can no longer collide during the rebuild.
  for (auto& feature : document_->feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      polysmith::core::refresh_sketch_derived_state(feature);
    }
  }
  // The refresh replaces every profile with fresh ids — the saved
  // selection would otherwise dangle and surface as
  // "Sketch profile not found" on the next command.
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();

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
// CAM state lives in DocumentState::cam (target-schema types from
// cam_types.h).  Mutators follow the canonical document shape:
// require → validate (throw std::runtime_error with a human message,
// document untouched) → push undo → mutate → bump revision → return.

namespace {

bool is_supported_machine_type(const std::string& type) {
  static const std::set<std::string> kSupported = {
      "3_axis_mill", "4_axis_mill", "5_axis_mill", "lathe_2_axis",
      "lathe_live_tooling", "laser", "plasma", "printer"};
  return kSupported.count(type) > 0;
}

bool is_supported_tool_type(const std::string& type) {
  static const std::set<std::string> kSupported = {
      "endmill_flat", "endmill_ball", "endmill_bull", "drill",
      "facemill", "chamfer", "threadmill", "turning_insert",
      "laser", "plasma"};
  return kSupported.count(type) > 0;
}

}  // namespace

DocumentState DocumentManager::cam_setup_create(const CamSetup& setup) {
  require_document();
  if (setup.name.empty()) {
    throw std::runtime_error("The CAM setup needs a name");
  }
  if (!is_supported_machine_type(setup.machine_type)) {
    throw std::runtime_error("Unknown machine type: " + setup.machine_type);
  }
  if (setup.units != "mm" && setup.units != "inch") {
    throw std::runtime_error("CAM units must be 'mm' or 'inch'");
  }
  if (!setup.setup_id.empty()) {
    for (const auto& existing : document_->cam.setups) {
      if (existing.setup_id == setup.setup_id) {
        throw std::runtime_error("A CAM setup with this id already exists");
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  CamSetup stored = setup;
  if (stored.setup_id.empty()) {
    stored.setup_id = "cam-setup-" + std::to_string(next_cam_setup_id_++);
  }
  document_->cam.setups.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_machine_settings_set(
    const LaserMachineSettings& machine_settings) {
  require_document();
  if (machine_settings.work_area_x_mm <= 0.0 ||
      machine_settings.work_area_y_mm <= 0.0) {
    throw std::runtime_error("The machine work area must be positive");
  }
  push_undo_state();
  clear_redo_stack();
  document_->cam.machine_settings = machine_settings;
  bump_geometry_revision();
  return document_.value();
}

std::optional<CamSetup> DocumentManager::cam_setup_get() const {
  if (!document_.has_value() || document_->cam.setups.empty())
    return std::nullopt;
  return document_->cam.setups[0];
}

DocumentState DocumentManager::cam_setup_update(const CamSetup& setup) {
  require_document();
  if (setup.name.empty()) {
    throw std::runtime_error("The CAM setup needs a name");
  }
  if (!is_supported_machine_type(setup.machine_type)) {
    throw std::runtime_error("Unknown machine type: " + setup.machine_type);
  }
  for (auto& existing : document_->cam.setups) {
    if (existing.setup_id == setup.setup_id) {
      push_undo_state();
      clear_redo_stack();
      existing = setup;
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM setup not found: " + setup.setup_id);
}

DocumentState DocumentManager::cam_tool_add(const ToolEntry& tool) {
  require_document();
  if (tool.name.empty()) {
    throw std::runtime_error("The tool needs a name");
  }
  if (!is_supported_tool_type(tool.type)) {
    throw std::runtime_error("Unknown tool type: " + tool.type);
  }
  if (!tool.tool_id.empty()) {
    for (const auto& existing : document_->cam.tool_library) {
      if (existing.tool_id == tool.tool_id) {
        throw std::runtime_error("A tool with this id already exists");
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  ToolEntry stored = tool;
  if (stored.tool_id.empty()) {
    stored.tool_id = "tool-" + std::to_string(next_cam_tool_id_++);
  }
  document_->cam.tool_library.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_tool_update(const std::string& toolId,
                                               const ToolEntry& tool) {
  require_document();
  if (tool.name.empty()) {
    throw std::runtime_error("The tool needs a name");
  }
  if (!is_supported_tool_type(tool.type)) {
    throw std::runtime_error("Unknown tool type: " + tool.type);
  }
  for (auto& existing : document_->cam.tool_library) {
    if (existing.tool_id == toolId) {
      push_undo_state();
      clear_redo_stack();
      existing = tool;
      existing.tool_id = toolId;  // the id is the lookup key, not the payload
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("Tool not found: " + toolId);
}

DocumentState DocumentManager::cam_tool_delete(const std::string& toolId) {
  require_document();
  bool found = false;
  for (auto it = document_->cam.tool_library.begin();
       it != document_->cam.tool_library.end(); ++it) {
    if (it->tool_id == toolId) {
      push_undo_state();
      clear_redo_stack();
      document_->cam.tool_library.erase(it);
      // Operations referencing the deleted tool can no longer generate —
      // degrade them with a human-readable message rather than leaving a
      // dangling reference.
      for (auto& op : document_->cam.operations) {
        if (op.tool_id == toolId) {
          op.status = "error";
          op.status_message = "The tool used by this operation was deleted.";
          op.toolpath_cache.reset();
        }
      }
      bump_geometry_revision();
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Tool not found: " + toolId);
  }
  return document_.value();
}

std::vector<ToolEntry> DocumentManager::cam_tool_list() const {
  if (!document_.has_value()) return {};
  return document_->cam.tool_library;
}

DocumentState DocumentManager::cam_operation_add(const CamOperation& op) {
  require_document();
  if (op.type.empty()) {
    throw std::runtime_error("The CAM operation needs a type");
  }
  // A laser operation makes no sense on a milling machine — catch the
  // mismatch at creation time instead of at export time.
  if (op.type == "laser_cut" && !document_->cam.setups.empty() &&
      document_->cam.setups[0].machine_type != "laser") {
    throw std::runtime_error(
        "A laser cut operation requires a laser machine setup");
  }
  if (!op.op_id.empty()) {
    for (const auto& existing : document_->cam.operations) {
      if (existing.op_id == op.op_id) {
        throw std::runtime_error("A CAM operation with this id already exists");
      }
    }
  }

  CamOperation stored = op;
  if (stored.op_id.empty()) {
    stored.op_id = "cam-op-" + std::to_string(next_cam_op_id_++);
  }
  // An operation without an explicit setup joins the first setup
  // (legacy payloads keep working; multi-setup ops carry setup_id).
  if (stored.setup_id.empty() && !document_->cam.setups.empty()) {
    stored.setup_id = document_->cam.setups[0].setup_id;
  }

  // Tool resolution.  An explicit tool must exist; an EMPTY tool gets
  // a sensible default — a matching tool from the library when one
  // exists, otherwise a default tool created on the spot (the very
  // first operation must not require a manual tool-library step).
  bool createDefaultTool = false;
  ToolEntry defaultTool;
  if (stored.tool_id.empty()) {
    const bool wantsLaser = stored.type == "laser_cut";
    const ToolEntry* match = nullptr;
    for (const auto& tool : document_->cam.tool_library) {
      if (wantsLaser ? tool.type == "laser" : tool.type == "endmill_flat") {
        match = &tool;
        break;
      }
    }
    if (match != nullptr) {
      stored.tool_id = match->tool_id;
    } else {
      createDefaultTool = true;
      defaultTool.tool_id = "tool-" + std::to_string(next_cam_tool_id_++);
      if (wantsLaser) {
        // Identity fields only — the struct defaults carry the
        // sensible cutting parameters.
        defaultTool.name = "CO2 laser (default)";
        defaultTool.type = "laser";
      } else {
        defaultTool.name = "6mm flat endmill (default)";
        defaultTool.type = "endmill_flat";
        defaultTool.diameter_mm = 6.0;
      }
      stored.tool_id = defaultTool.tool_id;
    }
  } else {
    bool toolExists = false;
    for (const auto& tool : document_->cam.tool_library) {
      if (tool.tool_id == stored.tool_id) {
        toolExists = true;
        break;
      }
    }
    if (!toolExists) {
      throw std::runtime_error("Unknown tool: " + stored.tool_id);
    }
  }

  // One undo step covers the operation and the default tool (if any).
  push_undo_state();
  clear_redo_stack();
  if (createDefaultTool) {
    document_->cam.tool_library.push_back(defaultTool);
  }
  stored.status = "pending";
  stored.status_message.clear();
  stored.toolpath_cache.reset();
  document_->cam.operations.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_update(const std::string& operation_id,
                                                    const CamOperation& op) {
  require_document();
  bool toolExists = false;
  for (const auto& tool : document_->cam.tool_library) {
    if (tool.tool_id == op.tool_id) {
      toolExists = true;
      break;
    }
  }
  if (!toolExists) {
    throw std::runtime_error("Unknown tool: " + op.tool_id);
  }
  for (auto& existing : document_->cam.operations) {
    if (existing.op_id == operation_id) {
      push_undo_state();
      clear_redo_stack();
      existing = op;
      existing.op_id = operation_id;  // the id is the lookup key, not the payload
      // Any parameter/geometry/tool change invalidates a generated path.
      existing.status = "needs_regenerate";
      existing.status_message.clear();
      existing.toolpath_cache.reset();
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}

DocumentState DocumentManager::cam_operation_delete(const std::string& operation_id) {
  require_document();
  for (auto it = document_->cam.operations.begin();
       it != document_->cam.operations.end(); ++it) {
    if (it->op_id == operation_id) {
      push_undo_state();
      clear_redo_stack();
      document_->cam.operations.erase(it);
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}

DocumentState DocumentManager::cam_post_processor_set(
    const PostProcessor& post_processor) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  document_->cam.post_processor = post_processor;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_set_generated(
    const std::string& operation_id, const CamOperation& op) {
  require_document();
  for (auto& existing : document_->cam.operations) {
    if (existing.op_id == operation_id) {
      existing = op;
      existing.op_id = operation_id;  // the id is the lookup key, not the payload
      existing.status = "generated";
      existing.status_message.clear();
      // Deliberately no bump_geometry_revision: generation is derived
      // data.  A revision bump would invalidate the cache entry this
      // very call is recording.
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}
