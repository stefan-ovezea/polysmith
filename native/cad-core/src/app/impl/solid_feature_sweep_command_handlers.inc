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
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_sweep_profile(
        feature_id, read_string(command.payload, "profile_id"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sweep_path") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_sweep_path(
        feature_id, read_string(command.payload, "path_entity_id"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
