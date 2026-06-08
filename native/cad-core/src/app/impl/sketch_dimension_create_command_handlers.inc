  if (command.type == "add_sketch_angle_dimension") {
    const auto document = document_manager().add_sketch_angle_dimension(
        read_string(command.payload, "first_line_id"),
        read_string(command.payload, "second_line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_distance_dimension") {
    const auto document = document_manager().add_sketch_distance_dimension(
        read_string(command.payload, "first_entity_id"),
        read_string(command.payload, "second_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension_label_position") {
    const auto document =
        document_manager().update_sketch_dimension_label_position(
            read_string(command.payload, "dimension_id"),
            read_dimension(command.payload, "label_x"),
            read_dimension(command.payload, "label_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_line_length_dimension") {
    const auto document =
        document_manager().add_sketch_line_length_dimension(
            read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_circle_radius_dimension") {
    std::optional<std::string> display_as;
    if (command.payload.contains("display_as") &&
        command.payload.at("display_as").is_string()) {
      display_as = command.payload.at("display_as").get<std::string>();
    }
    const auto document =
        document_manager().add_sketch_circle_radius_dimension(
            read_string(command.payload, "circle_id"), display_as);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_point_distance_dimension") {
    const auto document =
        document_manager().add_sketch_point_distance_dimension(
            read_string(command.payload, "point_a_id"),
            read_string(command.payload, "point_b_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_polygon_radius_dimension") {
    const auto document =
        document_manager().add_sketch_polygon_radius_dimension(
            read_string(command.payload, "polygon_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
