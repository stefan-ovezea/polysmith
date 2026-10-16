  if (command.type == "pcb_import_layer") {
    // The payload's optional `role` is the layer table's explicit override;
    // absent means "resolve from the file's own attributes, then its name".
    const std::optional<std::string> role =
        command.payload.contains("role") && command.payload.at("role").is_string()
            ? std::optional<std::string>(command.payload.at("role").get<std::string>())
            : std::nullopt;
    const auto document = document_manager().import_pcb_layer(
        read_string(command.payload, "file_path"), role);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "pcb_update_layer") {
    const std::optional<std::string> role =
        command.payload.contains("role") && command.payload.at("role").is_string()
            ? std::optional<std::string>(command.payload.at("role").get<std::string>())
            : std::nullopt;
    const std::optional<bool> mirror_x =
        command.payload.contains("mirror_x") && command.payload.at("mirror_x").is_boolean()
            ? std::optional<bool>(command.payload.at("mirror_x").get<bool>())
            : std::nullopt;
    const auto document = document_manager().update_pcb_layer(
        read_string(command.payload, "feature_id"), role, mirror_x);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "pcb_remove_layer") {
    const auto document = document_manager().remove_pcb_layer(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
