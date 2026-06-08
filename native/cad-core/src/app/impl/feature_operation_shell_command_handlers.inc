  if (command.type == "create_shell") {
    const auto document = document_manager().create_shell(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "thickness"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_shell_thickness") {
    const auto document = document_manager().update_shell_thickness(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "thickness"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_shell") {
    const auto document = document_manager().confirm_shell(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
