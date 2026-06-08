  if (command.type == "create_helix") {
    const auto document = document_manager().create_helix(
        read_string(command.payload, "axis_source_id"),
        read_helix_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_helix_parameters") {
    const auto document = document_manager().update_helix_parameters(
        read_string(command.payload, "feature_id"),
        read_helix_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_thread") {
    const auto document = document_manager().create_thread(
        read_thread_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_thread_parameters") {
    const auto document = document_manager().update_thread_parameters(
        read_string(command.payload, "feature_id"),
        read_thread_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_thread") {
    const auto document = document_manager().confirm_thread(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_fastener") {
    const auto document = document_manager().create_fastener(
        read_fastener_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fastener_parameters") {
    const auto document = document_manager().update_fastener_parameters(
        read_string(command.payload, "feature_id"),
        read_fastener_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
