  if (command.type == "create_move") {
    const auto document = document_manager().create_move(
        read_string(command.payload, "target_body_id"),
        read_move_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_move_parameters") {
    const auto document = document_manager().update_move_parameters(
        read_string(command.payload, "feature_id"),
        read_move_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_move") {
    const auto document = document_manager().confirm_move(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_body_copy") {
    const auto document = document_manager().create_body_copy(
        read_string(command.payload, "source_body_id"),
        read_optional_string(command.payload, "copy_mode").value_or("linked"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "unlink_body_copy") {
    const auto document = document_manager().unlink_body_copy(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
