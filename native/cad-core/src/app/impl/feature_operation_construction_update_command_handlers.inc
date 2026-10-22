  if (command.type == "update_offset_plane") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_offset_plane(
        feature_id, read_dimension(command.payload, "offset"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_angle_plane") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_angle_plane(
        feature_id, read_dimension(command.payload, "angle_degrees"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
