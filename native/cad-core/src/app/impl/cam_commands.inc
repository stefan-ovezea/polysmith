  // ── CAM Operation Create ───────────────────────────────────────
  if (command.type == "cam_operation_create") {
    core::CamOperationEntry op;
    op.id = "cam-op-" + command.id;
    op.name = read_string(command.payload, "name");
    const auto opType = read_string(command.payload, "operation_type");
    if (opType == "face_milling") {
      op.type = core::CamOperationType::FaceMilling;
      core::FaceMillingParams params;
      if (command.payload.contains("params")) {
        const auto& p = command.payload["params"];
        if (p.contains("depth")) params.depth = p["depth"].get<double>();
        if (p.contains("stepover")) params.stepover = p["stepover"].get<double>();
        if (p.contains("angle_deg")) params.angleDeg = p["angle_deg"].get<double>();
      }
      op.faceMilling = params;
    }
    op.toolId = read_string(command.payload, "tool_id");
    // Read geometry reference.
    if (command.payload.contains("face_reference")) {
      const auto& ref = command.payload["face_reference"];
      if (ref.contains("body_id")) op.bodyId = ref["body_id"].get<std::string>();
      if (ref.contains("face_index")) op.faceIndex = ref["face_index"].get<int>();
    }

    // For face milling, auto-select the face whose normal is most aligned
    // with Z (the tool axis) instead of trusting an arbitrary index.
    if (op.type == core::CamOperationType::FaceMilling && !op.bodyId.empty()) {
      const auto doc = document_manager().get_document();
      if (doc.has_value()) {
        const auto compiled = compile_bodies(doc.value());
        for (const auto& body : compiled.bodies) {
          if (body.id == op.bodyId && !body.shape.IsNull()) {
            TopTools_IndexedMapOfShape faceMap;
            TopExp::MapShapes(body.shape, TopAbs_FACE, faceMap);
            int bestIdx = 0;
            double bestDot = -1.0;
            for (int i = 1; i <= faceMap.Extent(); ++i) {
              const TopoDS_Face& face = TopoDS::Face(faceMap(i));
              BRepAdaptor_Surface surface(face);
              const double uMid = 0.5 * (surface.FirstUParameter() + surface.LastUParameter());
              const double vMid = 0.5 * (surface.FirstVParameter() + surface.LastVParameter());
              gp_Pnt pt;
              gp_Vec d1u, d1v;
              surface.D1(uMid, vMid, pt, d1u, d1v);
              gp_Vec normal = d1u.Crossed(d1v);
              if (normal.Magnitude() > 1e-12) {
                normal.Normalize();
                if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
                const double dotZ = std::abs(normal.Z());
                if (dotZ > bestDot) {
                  bestDot = dotZ;
                  bestIdx = i - 1;  // 0-based
                }
              }
            }
            if (bestDot > 0.5) {
              op.faceIndex = bestIdx;
            }
            break;
          }
        }
      }
    }

    const auto document = document_manager().cam_operation_add(op);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  // ── CAM Operation Update / Delete ───────────────────────────────
  if (command.type == "cam_operation_update") {
    const auto opId = read_string(command.payload, "operation_id");

    // Start from the existing operation so fields not in the payload
    // are preserved (e.g. name, tool_id, TNP witness data).
    core::CamOperationEntry updated;
    bool found = false;
    {
      const auto doc = document_manager().get_document();
      if (doc.has_value()) {
        for (const auto& op : doc->cam_operations) {
          if (op.id == opId) {
            updated = op;
            found = true;
            break;
          }
        }
      }
    }
    if (!found) {
      updated.id = opId;
    }

    if (command.payload.contains("name")) updated.name = read_string(command.payload, "name");
    if (command.payload.contains("tool_id")) updated.toolId = read_string(command.payload, "tool_id");
    if (command.payload.contains("params")) {
      const auto& p = command.payload["params"];
      core::FaceMillingParams fm;
      if (p.contains("depth")) fm.depth = p["depth"].get<double>();
      if (p.contains("stepover")) fm.stepover = p["stepover"].get<double>();
      if (p.contains("angle_deg")) fm.angleDeg = p["angle_deg"].get<double>();
      updated.faceMilling = fm;
    }
    const auto document = document_manager().cam_operation_update(opId, updated);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_operation_delete") {
    const auto opId = read_string(command.payload, "operation_id");
    const auto document = document_manager().cam_operation_delete(opId);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  // ── CAM Setup ──────────────────────────────────────────────────
  if (command.type == "cam_setup_create") {
    const auto setup = polysmith::protocol::setup_from_payload(command.payload);
    const auto document = document_manager().cam_setup_create(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_setup_get") {
    const auto setup = document_manager().cam_setup_get();
    nlohmann::json payload;
    if (setup.has_value()) {
      payload = polysmith::protocol::to_payload(setup.value());
    } else {
      payload = nullptr;
    }
    polysmith::protocol::write_message(
        {{"id", command.id}, {"type", "cam_setup_state"}, {"payload", payload}});
    return;
  }

  if (command.type == "cam_setup_update") {
    const auto setup = polysmith::protocol::setup_from_payload(command.payload);
    const auto document = document_manager().cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  // ── CAM Tool Library ───────────────────────────────────────────
  if (command.type == "cam_tool_add") {
    const auto tool = polysmith::protocol::tool_from_payload(command.payload);
    const auto document = document_manager().cam_tool_add(tool);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_tool_list") {
    const auto tools = document_manager().cam_tool_list();
    nlohmann::json tools_json = nlohmann::json::array();
    for (const auto& tool : tools) {
      tools_json.push_back(polysmith::protocol::to_payload(tool));
    }
    polysmith::protocol::write_message(
        {{"id", command.id}, {"type", "cam_tool_list"}, {"payload", {{"tools", tools_json}}}});
    return;
  }

  if (command.type == "cam_tool_delete") {
    const auto toolId = read_string(command.payload, "tool_id");
    const auto document = document_manager().cam_tool_delete(toolId);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  // ── CAM Stock ──────────────────────────────────────────────────
  if (command.type == "cam_stock_set") {
    const auto stock = polysmith::protocol::stock_from_payload(command.payload);
    auto setup = document_manager().cam_setup_get().value_or(core::CamSetup{});
    setup.stock = stock;
    const auto document = document_manager().cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cam_stock_get") {
    const auto setup = document_manager().cam_setup_get();
    nlohmann::json payload;
    if (setup.has_value()) {
      payload = polysmith::protocol::to_payload(setup.value().stock);
    } else {
      payload = nullptr;
    }
    polysmith::protocol::write_message(
        {{"id", command.id}, {"type", "cam_stock_state"}, {"payload", payload}});
    return;
  }

  if (command.type == "shutdown") {
    throw std::runtime_error("__POLYSMITH_SHUTDOWN__");
  }

  polysmith::core::log_error("cad_core", "Unknown command: " + command.type);
  polysmith::protocol::write_message(polysmith::protocol::make_error_event(
      command.id, "UNKNOWN_COMMAND", "Unknown command: " + command.type));
