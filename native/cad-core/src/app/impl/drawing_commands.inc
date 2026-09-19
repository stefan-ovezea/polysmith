  // ── DRAWING commands ───────────────────────────────────────────
  //
  // All drawing state lives in DocumentState::drawing
  // (drawing_types.h).  Payloads are the serialized target-schema
  // structs; replies are document_state events like every other
  // mutator.  Validation errors from the DocumentManager bubble to
  // the run() catch and reply with an error event — handlers stay
  // thin.

  if (command.type == "drawing_create") {
    const auto drawing =
        polysmith::protocol::drawing_from_payload(command.payload);
    const auto document = document_manager().drawing_create(drawing);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_delete") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto document = document_manager().drawing_delete(drawing_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_set_active") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto document = document_manager().drawing_set_active(drawing_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_sheet_create") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet = polysmith::protocol::drawing_sheet_from_payload(
        command.payload.at("sheet"));
    const auto document =
        document_manager().drawing_sheet_create(drawing_id, sheet);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_sheet_delete") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet_id = read_string(
        command.payload, "sheet_id");
    const auto document =
        document_manager().drawing_sheet_delete(drawing_id, sheet_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_create") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet_id = read_string(
        command.payload, "sheet_id");
    const auto view = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));
    const auto document =
        document_manager().drawing_view_create(drawing_id, sheet_id, view);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_update") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));
    const auto document =
        document_manager().drawing_view_update(drawing_id, view);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_delete") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view_id = read_string(
        command.payload, "view_id");
    const auto document =
        document_manager().drawing_view_delete(drawing_id, view_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_move") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view_id = read_string(
        command.payload, "view_id");
    const auto position = command.payload.at("sheet_position");
    if (!position.is_array() || position.size() < 2 ||
        !position.at(0).is_number() || !position.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_view_move needs sheet_position [x, y]"));
      return;
    }
    const std::array<double, 2> sheet_position = {
        position.at(0).get<double>(), position.at(1).get<double>()};
    const auto document = document_manager().drawing_view_move(
        drawing_id, view_id, sheet_position);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_section_update") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view_id = read_string(
        command.payload, "view_id");
    const auto section =
        polysmith::protocol::section_definition_from_payload(
            command.payload.at("section"));
    const auto document = document_manager().drawing_section_update(
        drawing_id, view_id, section);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
