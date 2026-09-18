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
