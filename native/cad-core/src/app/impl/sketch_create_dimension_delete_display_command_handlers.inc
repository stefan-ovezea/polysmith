  if (command.type == "delete_sketch_dimension") {
    const auto document = document_manager().delete_sketch_dimension(
        read_string(command.payload, "dimension_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension_display") {
    const auto document = document_manager().update_sketch_dimension_display(
        read_string(command.payload, "dimension_id"),
        read_string(command.payload, "display_as"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
