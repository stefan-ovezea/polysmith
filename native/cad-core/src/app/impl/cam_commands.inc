  // ── CAM commands ──────────────────────────────────────────────
  //
  // All CAM state lives in DocumentState::cam (cam_types.h).  Payloads
  // are the serialized target-schema structs; replies are document_state
  // events like every other mutator.  Validation errors from the
  // DocumentManager bubble to the run() catch and reply with an error
  // event — handlers stay thin.

  if (command.type == "cam_setup_create") {
    const auto setup =
        polysmith::protocol::cam_setup_from_payload(command.payload);
    const auto document = document_manager().cam_setup_create(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_setup_get") {
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return;
  }

  if (command.type == "cam_setup_update") {
    const auto setup =
        polysmith::protocol::cam_setup_from_payload(command.payload);
    const auto document = document_manager().cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_setup_delete") {
    const auto document = document_manager().cam_setup_delete(
        read_string(command.payload, "setup_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_machine_settings_set") {
    const auto settings =
        polysmith::protocol::laser_machine_settings_from_payload(
            command.payload);
    const auto document =
        document_manager().cam_machine_settings_set(settings);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_capture_face_reference") {
    // The UI asks the core to capture a TNP-safe face witness from a
    // selected body face ("<body_id>:face:<index>") — the witness
    // math lives in the core, never in TypeScript.
    const std::string face_id = command.payload.value("face_id", "");
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    const auto separator = face_id.rfind(":face:");
    if (separator == std::string::npos) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return;
    }
    const std::string body_id = face_id.substr(0, separator);
    int face_index = 0;
    try {
      face_index = std::stoi(face_id.substr(separator + 6));
    } catch (const std::exception&) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return;
    }

    const auto bodies =
        polysmith::core::compile_bodies(document.value(),
                                 /*include_meshes=*/false);
    const polysmith::core::CompiledBody* body = nullptr;
    for (const auto& candidate : bodies.bodies) {
      if (candidate.id == body_id) {
        body = &candidate;
        break;
      }
    }
    if (body == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_NOT_FOUND",
          "The body of the selected face no longer exists."));
      return;
    }
    const auto captured =
        polysmith::core::capture_face_reference(body_id, body->shape,
                                                  face_index);
    if (!captured.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_CAPTURE_FAILED",
          "The face witness could not be captured."));
      return;
    }
    polysmith::core::FaceAttestation attestation;
    attestation.area = captured->capturedArea;
    attestation.normal = captured->capturedNormal;
    attestation.sample_points = captured->samplePoints;
    // Bounds: the sample-point bbox (resolution does not consume
    // bounds; they round-trip for debugging).
    for (const auto& point : captured->samplePoints) {
      attestation.bounds.min_x = std::min(attestation.bounds.min_x, point[0]);
      attestation.bounds.min_y = std::min(attestation.bounds.min_y, point[1]);
      attestation.bounds.min_z = std::min(attestation.bounds.min_z, point[2]);
      attestation.bounds.max_x = std::max(attestation.bounds.max_x, point[0]);
      attestation.bounds.max_y = std::max(attestation.bounds.max_y, point[1]);
      attestation.bounds.max_z = std::max(attestation.bounds.max_z, point[2]);
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_face_attestation_event(
            command.id,
            {{"persistent_id", face_id},
             {"attestation", polysmith::protocol::to_payload(attestation)}}));
    return;
  }

  if (command.type == "cam_wcs_set_face") {
    // Anchor the WCS origin to a body face: capture the TNP-safe
    // witness, store it on the first setup's WCS, and let the refresh
    // pass resolve the machine origin from the live face.
    const std::string face_id = command.payload.value("face_id", "");
    const auto document = document_manager().get_document();
    if (!document.has_value() || document->cam.setups.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_CAM_SETUP", "Create a CAM setup first"));
      return;
    }
    const auto separator = face_id.rfind(":face:");
    if (separator == std::string::npos) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return;
    }
    const std::string body_id = face_id.substr(0, separator);
    int face_index = 0;
    try {
      face_index = std::stoi(face_id.substr(separator + 6));
    } catch (const std::exception&) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return;
    }

    const auto bodies =
        polysmith::core::compile_bodies(document.value(),
                                 /*include_meshes=*/false);
    const polysmith::core::CompiledBody* body = nullptr;
    for (const auto& candidate : bodies.bodies) {
      if (candidate.id == body_id) {
        body = &candidate;
        break;
      }
    }
    if (body == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_NOT_FOUND",
          "The body of the selected face no longer exists."));
      return;
    }
    const auto captured =
        polysmith::core::capture_face_reference(body_id, body->shape,
                                                  face_index);
    if (!captured.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_CAPTURE_FAILED",
          "The WCS face witness could not be captured."));
      return;
    }

    polysmith::core::FaceAttestation attestation;
    attestation.area = captured->capturedArea;
    attestation.normal = captured->capturedNormal;
    attestation.sample_points = captured->samplePoints;

    auto setup = document->cam.setups[0];
    setup.wcs_origin.feature_id = body_id;
    setup.wcs_origin.face_reference.persistent_id = face_id;
    setup.wcs_origin.face_reference.attestation = attestation;
    const auto updated = document_manager().cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated)));
    return;
  }

  if (command.type == "cam_stock_set") {
    // The stock is part of the setup; this command is a thin wrapper
    // over setups[0].stock for the setup panel's stock tab.
    const auto document = document_manager().get_document();
    if (!document.has_value() || document->cam.setups.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_CAM_SETUP", "Create a CAM setup first"));
      return;
    }
    auto setup = document->cam.setups[0];
    setup.stock = polysmith::protocol::stock_definition_from_payload(
        command.payload);
    const auto updated = document_manager().cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated)));
    return;
  }

  if (command.type == "cam_stock_get") {
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return;
  }

  if (command.type == "cam_tool_add") {
    const auto tool =
        polysmith::protocol::tool_entry_from_payload(command.payload);
    const auto document = document_manager().cam_tool_add(tool);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_tool_update") {
    const auto tool =
        polysmith::protocol::tool_entry_from_payload(command.payload);
    const auto document = document_manager().cam_tool_update(
        read_string(command.payload, "tool_id"), tool);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_tool_delete") {
    const auto document = document_manager().cam_tool_delete(
        read_string(command.payload, "tool_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_tool_list") {
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return;
  }

  if (command.type == "cam_operation_create") {
    auto op = polysmith::protocol::cam_operation_from_payload(command.payload);
    // When the payload carries no geometry references, capture witness
    // references from the selected sketch profiles (the contextual
    // workflow: select profiles → invoke the operation).
    if (op.geometry_references.machining_regions.empty()) {
      const auto document = document_manager().get_document();
      if (!document.has_value()) {
        polysmith::protocol::write_message(
            polysmith::protocol::make_error_event(
                command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
        return;
      }
      if (!polysmith::core::capture_profile_references_from_selection(
              document.value(), op)) {
        // A laser operation without a pre-selection is created with an
        // EMPTY scope — the panel's reference-sketch dropdown fills it
        // in afterwards.  Every other operation type still requires
        // geometry up front.
        if (op.type != "laser_cut") {
          polysmith::protocol::write_message(
              polysmith::protocol::make_error_event(
                  command.id, "NO_PROFILE_SELECTION",
                  "Select sketch profiles for the CAM operation first"));
          return;
        }
      }
    }
    const auto document = document_manager().cam_operation_add(op);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_operation_update") {
    const std::string op_id = read_string(command.payload, "op_id");
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    const polysmith::core::CamOperation* existing = nullptr;
    for (const auto& op : document->cam.operations) {
      if (op.op_id == op_id) {
        existing = &op;
        break;
      }
    }
    if (existing == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_OPERATION_NOT_FOUND",
          "CAM operation not found: " + op_id));
      return;
    }
    // Merge patch: only fields present in the payload overwrite the
    // stored operation (a full from_payload would reset absent fields
    // to struct defaults).
    auto merged = *existing;
    const auto& p = command.payload;
    if (p.contains("name") && !p.at("name").is_null()) {
      merged.name = p.at("name").get<std::string>();
    }
    if (p.contains("type") && !p.at("type").is_null()) {
      merged.type = p.at("type").get<std::string>();
    }
    if (p.contains("enabled") && p.at("enabled").is_boolean()) {
      merged.enabled = p.at("enabled").get<bool>();
    }
    if (p.contains("tool_id") && !p.at("tool_id").is_null()) {
      merged.tool_id = p.at("tool_id").get<std::string>();
    }
    if (p.contains("geometry_references")) {
      merged.geometry_references =
          polysmith::protocol::cam_geometry_references_from_payload(
              p.at("geometry_references"));
      // An explicitly EMPTY region set is the "re-select geometry"
      // gesture: capture fresh TNP-safe witnesses from the current
      // sketch profile selection.  Unlike create, this honors ONLY the
      // explicit profile selection — an empty selection is an error,
      // never a silent fallback to the whole selected sketch.
      if (merged.geometry_references.machining_regions.empty() &&
          merged.type == "laser_cut") {
        if (document->selected_sketch_profile_ids.empty() ||
            !polysmith::core::capture_profile_references_from_profile_ids(
                document.value(), document->selected_sketch_profile_ids,
                merged)) {
          polysmith::protocol::write_message(
              polysmith::protocol::make_error_event(
                  command.id, "NO_PROFILE_SELECTION",
                  "Select sketch profiles for the CAM operation first"));
          return;
        }
      }
    }
    if (p.contains("parameters")) {
      merged.parameters =
          polysmith::protocol::cam_operation_parameters_from_payload(
              p.at("parameters"));
    }
    if (p.contains("dependencies")) {
      merged.dependencies =
          polysmith::protocol::cam_operation_dependencies_from_payload(
              p.at("dependencies"));
    }
    const auto updated = document_manager().cam_operation_update(op_id, merged);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated)));
    return;
  }

  if (command.type == "cam_operation_delete") {
    const auto document = document_manager().cam_operation_delete(
        read_string(command.payload, "op_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_operation_set_scope") {
    // Re-target an operation at a sketch: the core captures fresh
    // TNP-safe witnesses for every profile of that sketch.  Errors
    // (unknown op / unknown sketch) bubble to the run() catch.
    const auto document = document_manager().cam_operation_set_scope(
        read_string(command.payload, "op_id"),
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_post_processor_set") {
    const auto post =
        polysmith::protocol::post_processor_from_payload(command.payload);
    const auto document = document_manager().cam_post_processor_set(post);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_post_list" || command.type == "cam_post_import") {
    std::string error;
    if (command.type == "cam_post_import") {
      const std::string imported = polysmith::core::import_post_processor(
          read_string(command.payload, "source_path"), error);
      if (!error.empty()) {
        polysmith::core::log_warn(
            "cam", "post import failed: " + error);
        polysmith::protocol::write_message(
            polysmith::protocol::make_error_event(
                command.id, "CAM_POST_IMPORT_FAILED", error));
        return;
      }
      polysmith::core::log_info("cam",
                                "post processor imported: " + imported);
    }
    const auto posts = polysmith::core::list_post_processors();
    polysmith::protocol::json payload = polysmith::protocol::json::array();
    for (const auto& entry : posts) {
      payload.push_back({{"name", entry.name}, {"path", entry.path}});
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_post_list_event(command.id, payload));
    return;
  }

  if (command.type == "cam_export_gcode") {
    const auto export_result = document_manager().export_cam_gcode(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id,
            polysmith::protocol::json{
                {"file_path", export_result.file_path},
                {"format", export_result.format},
                {"exported_feature_count",
                 export_result.exported_feature_count},
            }));
    return;
  }

  if (command.type == "cam_operation_generate" ||
      command.type == "cam_operation_preview") {
    const bool preview = command.type == "cam_operation_preview";
    const std::string op_id = read_string(command.payload, "op_id");
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }

    const auto outcome = polysmith::core::generate_operation_toolpath(
        document.value(), op_id, preview, [&](int percent) {
          polysmith::protocol::write_message(
              polysmith::protocol::make_cam_generation_progress_event(
                  command.id, op_id, percent));
        });

    if (!outcome.found) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_OPERATION_NOT_FOUND", outcome.result.error_message));
      return;
    }
    if (!outcome.result.ok) {
      polysmith::core::log_warn("cam",
                                "generation failed for '" + op_id +
                                    "': " + outcome.result.error_message);
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_GENERATION_FAILED", outcome.result.error_message));
      return;
    }

    // Generator warnings (skipped loops, double-cut hints, tessellated
    // contours) surface through the structured logger → Logs panel.
    for (const auto& warning : outcome.result.warnings) {
      polysmith::core::log_warn("cam", warning);
    }

    if (preview) {
      polysmith::core::cam_runtime::store_preview(
          document.value(), op_id, outcome.result.toolpath);
    } else {
      polysmith::core::cam_runtime::store_generated(
          document.value(), op_id, outcome.result.toolpath);
      // Record the serialized metadata on the operation (the path
      // itself stays memory-only) and mark it generated.
      const auto current = document_manager().get_document();
      for (const auto& op : current->cam.operations) {
        if (op.op_id == op_id) {
          auto updated = op;
          polysmith::core::ToolpathCache cache;
          cache.toolpath_id = "tp:" + op_id;
          cache.status = "up_to_date";
          cache.bounds = outcome.result.toolpath.bounds;
          cache.total_length_mm = outcome.result.toolpath.total_length_mm;
          cache.estimated_time_seconds =
              outcome.result.toolpath.estimated_time_seconds;
          cache.num_moves =
              static_cast<int>(outcome.result.toolpath.moves.size());
          cache.num_rapids = outcome.result.toolpath.num_rapids;
          cache.num_feeds = outcome.result.toolpath.num_feeds;
          updated.toolpath_cache = cache;
          document_manager().cam_operation_set_generated(op_id, updated);
          break;
        }
      }
    }

    const auto updated = document_manager().get_document();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated.value())));
    return;
  }
