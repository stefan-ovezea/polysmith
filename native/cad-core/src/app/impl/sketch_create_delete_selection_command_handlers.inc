  if (command.type == "delete_sketch_selection") {
    std::vector<std::string> entity_ids;
    std::vector<std::string> point_ids;
    std::vector<std::string> profile_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& id : command.payload.at("entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("point_ids") &&
        command.payload.at("point_ids").is_array()) {
      for (const auto& id : command.payload.at("point_ids")) {
        if (id.is_string()) {
          point_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document =
        document_manager().delete_sketch_selection(
            entity_ids, point_ids, profile_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
