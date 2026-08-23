  if (command.type == "create_linear_array" ||
      command.type == "create_circular_array") {
    std::vector<std::string> entity_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& entry : command.payload.at("entity_ids")) {
        if (entry.is_string()) {
          entity_ids.push_back(entry.get<std::string>());
        }
      }
    }
    if (command.type == "create_linear_array") {
      const auto document = document_manager().create_linear_array(
          entity_ids,
          read_dimension(command.payload, "dx"),
          read_dimension(command.payload, "dy"),
          read_int(command.payload, "count"));

      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document)));
      return;
    }
    const auto document = document_manager().create_circular_array(
        entity_ids,
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_int(command.payload, "count"),
        read_dimension(command.payload, "total_angle_deg"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
