  if (command.type == "extend_sketch_entity") {
    const auto document = document_manager().extend_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
