  if (command.type == "start_mirror_preview") {
    const auto document = document_manager().start_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_mirror_preview_axis") {
    // Empty axis_line_id is a valid clear. read_string allows
    // empty strings; the core treats empty as "no axis".
    const auto document = document_manager().update_mirror_preview_axis(
        read_string(command.payload, "axis_line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_mirror_preview_objects") {
    std::vector<std::string> object_ids;
    if (command.payload.contains("object_ids") &&
        command.payload.at("object_ids").is_array()) {
      for (const auto& entry : command.payload.at("object_ids")) {
        if (entry.is_string()) {
          object_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const auto document =
        document_manager().update_mirror_preview_objects(object_ids);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "commit_mirror_preview") {
    bool persistent = false;
    if (command.payload.contains("persistent")) {
      persistent = command.payload["persistent"].get<bool>();
    }
    const auto document = document_manager().commit_mirror_preview(persistent);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cancel_mirror_preview") {
    const auto document = document_manager().cancel_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
