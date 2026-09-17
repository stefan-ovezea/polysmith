  if (command.type == "add_box_feature") {
    const auto document = document_manager().add_box_feature(BoxFeatureParameters{
        .width = read_dimension(command.payload, "width"),
        .height = read_dimension(command.payload, "height"),
        .depth = read_dimension(command.payload, "depth"),
    });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_cylinder_feature") {
    const auto document =
        document_manager().add_cylinder_feature(CylinderFeatureParameters{
            .radius = read_dimension(command.payload, "radius"),
            .height = read_dimension(command.payload, "height"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_box_feature") {
    const auto document = document_manager().update_box_feature(
        read_string(command.payload, "feature_id"),
        BoxFeatureParameters{
            .width = read_dimension(command.payload, "width"),
            .height = read_dimension(command.payload, "height"),
            .depth = read_dimension(command.payload, "depth"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_cylinder_feature") {
    const auto document = document_manager().update_cylinder_feature(
        read_string(command.payload, "feature_id"),
        CylinderFeatureParameters{
            .radius = read_dimension(command.payload, "radius"),
            .height = read_dimension(command.payload, "height"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_plugin_feature") {
    const auto document = document_manager().create_plugin_feature(
        read_plugin_feature_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_plugin_feature") {
    const auto document = document_manager().update_plugin_feature(
        read_string(command.payload, "feature_id"),
        read_plugin_feature_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_plugin_feature") {
    const auto document = document_manager().confirm_plugin_feature(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_extrude_depth") {
    const auto document = document_manager().update_extrude_depth(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "depth"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "rename_feature") {
    const auto document = document_manager().rename_feature(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "name"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_feature_suppressed") {
    const auto document = document_manager().set_feature_suppressed(
        read_string(command.payload, "feature_id"),
        command.payload.at("suppressed").get<bool>());

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_feature") {
    const auto document =
        document_manager().delete_feature(read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "undo") {
    // Empty-stack undo is a protocol-level no-op with its own error
    // code (D6) — not a throw surfacing as INVALID_COMMAND, which is
    // what the UI used to special-case by message string.
    if (!document_manager().get_session_state().can_undo) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "EMPTY_UNDO_STACK", "Nothing to undo"));
      return;
    }
    const auto document = document_manager().undo();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "redo") {
    if (!document_manager().get_session_state().can_redo) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "EMPTY_REDO_STACK", "Nothing to redo"));
      return;
    }
    const auto document = document_manager().redo();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "undo_many") {
    // History-dropdown multi-undo (D8): the UI confirms when count > 1.
    const auto document = document_manager().undo_many(
        read_int(command.payload, "count"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "undo_begin_group") {
    document_manager().begin_undo_group(
        read_string(command.payload, "name"));
    const auto document = document_manager().get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return;
  }

  if (command.type == "undo_end_group") {
    document_manager().end_undo_group();
    const auto document = document_manager().get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return;
  }

  if (command.type == "undo_abort_group") {
    document_manager().abort_undo_group();
    const auto document = document_manager().get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return;
  }

  if (command.type == "undo_abort_all_groups") {
    // "Cancel Sketch" (D4): roll back the entire edit session — nested
    // draft groups included — to the session's start snapshot.
    document_manager().abort_all_undo_groups();
    const auto document = document_manager().get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return;
  }

  if (command.type == "set_undo_limit") {
    document_manager().set_undo_limit(read_int(command.payload, "limit"));
    const auto document = document_manager().get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return;
  }

  if (command.type == "set_timeline_cursor") {
    const auto document = document_manager().set_timeline_cursor(
        read_int(command.payload, "included_action_count"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_feature") {
    const auto document =
        document_manager().select_feature(read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_reference") {
    const auto document = document_manager().select_reference(
        read_string(command.payload, "reference_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_face") {
    const auto document =
        document_manager().select_face(read_string(command.payload, "face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_edge") {
    // `additive` toggles the edge into / out of the multi-select set
    // (shift-click). It defaults to false so old payloads (which only
    // sent `edge_id`) keep behaving as a plain replace-style select.
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_edge(
        read_string(command.payload, "edge_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_vertex") {
    // `additive` toggles the vertex into the multi-select set
    // (shift-click). Defaults to false for back-compat with payloads
    // that only carry `vertex_id`.
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_vertex(
        read_string(command.payload, "vertex_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_body_color") {
    const auto document = document_manager().set_body_color(
        read_string(command.payload, "body_id"),
        read_string(command.payload, "color"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_face_color") {
    const auto document = document_manager().set_face_color(
        read_string(command.payload, "face_id"),
        read_string(command.payload, "color"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_body_color") {
    const auto document = document_manager().clear_body_color(
        read_string(command.payload, "body_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_face_color") {
    const auto document = document_manager().clear_face_color(
        read_string(command.payload, "face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_appearance_overrides") {
    const auto document = document_manager().clear_appearance_overrides();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
