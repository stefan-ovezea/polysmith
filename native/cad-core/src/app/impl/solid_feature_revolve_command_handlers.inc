  if (command.type == "revolve_profile") {
    double angle_degrees = 360.0;
    if (command.payload.contains("angle_degrees") &&
        command.payload.at("angle_degrees").is_number()) {
      angle_degrees = command.payload.at("angle_degrees").get<double>();
    }
    const auto document = document_manager().revolve_profile(
        read_string(command.payload, "profile_id"),
        read_string(command.payload, "axis_entity_id"),
        angle_degrees);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_revolve_profile") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_revolve_profile(
        feature_id, read_string(command.payload, "profile_id"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_revolve_axis") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_revolve_axis(
        feature_id, read_string(command.payload, "axis_entity_id"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_revolve_angle") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_revolve_angle(
        feature_id, read_dimension(command.payload, "angle_degrees"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
