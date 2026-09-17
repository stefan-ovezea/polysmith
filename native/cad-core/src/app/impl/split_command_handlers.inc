  if (command.type == "split_sketch_entity") {
    const auto document = document_manager().split_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"),
        read_dimension(command.payload, "split2_x"),
        read_dimension(command.payload, "split2_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
