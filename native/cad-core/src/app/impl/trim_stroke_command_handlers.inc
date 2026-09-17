  if (command.type == "trim_sketch_stroke") {
    std::vector<polysmith::core::TrimStrokeEntry> entries;
    if (command.payload.contains("entries") &&
        command.payload["entries"].is_array()) {
      entries.reserve(command.payload["entries"].size());
      for (const auto& item : command.payload["entries"]) {
        entries.push_back(polysmith::core::TrimStrokeEntry{
            .entity_id = read_string(item, "entity_id"),
            .click_x = read_dimension(item, "click_x"),
            .click_y = read_dimension(item, "click_y"),
        });
      }
    }
    const auto document = document_manager().trim_sketch_stroke(entries);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
