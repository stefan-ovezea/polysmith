  if (command.type == "create_hole") {
    const auto document = document_manager().create_hole(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "center_z"),
        read_hole_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_hole_parameters") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_hole_parameters(
        feature_id, read_hole_parameters(command.payload));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_hole") {
    const auto document = document_manager().confirm_hole(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
