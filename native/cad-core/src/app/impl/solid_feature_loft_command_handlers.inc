  if (command.type == "loft_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    bool ruled = false;
    if (command.payload.contains("ruled") &&
        command.payload.at("ruled").is_boolean()) {
      ruled = command.payload.at("ruled").get<bool>();
    }
    const auto document = document_manager().loft_profiles(profile_ids, ruled);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_loft_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document = document_manager().update_loft_profiles(
        read_string(command.payload, "feature_id"), profile_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_loft_ruled") {
    if (!command.payload.contains("ruled") ||
        !command.payload.at("ruled").is_boolean()) {
      throw std::runtime_error("Command payload is missing bool field 'ruled'");
    }
    const auto document = document_manager().update_loft_ruled(
        read_string(command.payload, "feature_id"),
        command.payload.at("ruled").get<bool>());

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
