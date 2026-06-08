  if (command.type == "sweep_profile") {
    const auto document = document_manager().sweep_profile(
        read_string(command.payload, "profile_id"),
        read_string(command.payload, "path_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sweep_profile") {
    const auto document = document_manager().update_sweep_profile(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "profile_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sweep_path") {
    const auto document = document_manager().update_sweep_path(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "path_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
