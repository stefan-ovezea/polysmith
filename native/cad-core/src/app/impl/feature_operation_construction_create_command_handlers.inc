  if (command.type == "create_offset_plane") {
    const auto document = document_manager().create_offset_plane(
        read_string(command.payload, "source_plane_id"),
        read_dimension(command.payload, "offset"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_midplane") {
    std::vector<std::string> source_plane_ids;
    if (command.payload.contains("source_plane_ids") &&
        command.payload.at("source_plane_ids").is_array()) {
      for (const auto& id : command.payload.at("source_plane_ids")) {
        if (id.is_string()) {
          source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (source_plane_ids.size() != 2) {
      throw std::runtime_error("create_midplane requires two source_plane_ids");
    }
    const auto document = document_manager().create_midplane(
        source_plane_ids[0], source_plane_ids[1]);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_tangent_plane") {
    const auto document = document_manager().create_tangent_plane(
        read_string(command.payload, "source_face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_angle_plane") {
    const auto document = document_manager().create_angle_plane(
        read_string(command.payload, "source_plane_id"),
        read_string(command.payload, "source_axis_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_construction_axis") {
    const auto document = document_manager().create_construction_axis(
        read_string(command.payload, "source_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_construction_point") {
    const auto document = document_manager().create_construction_point(
        read_string(command.payload, "source_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
