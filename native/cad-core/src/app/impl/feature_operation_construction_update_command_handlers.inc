  if (command.type == "update_offset_plane") {
    const auto document = document_manager().update_offset_plane(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "offset"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_angle_plane") {
    const auto document = document_manager().update_angle_plane(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
