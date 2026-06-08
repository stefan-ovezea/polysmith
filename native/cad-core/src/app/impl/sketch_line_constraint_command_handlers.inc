  if (command.type == "set_sketch_line_constraint") {
    const std::string constraint = read_string(command.payload, "constraint");
    const auto document = document_manager().set_sketch_line_constraint(
        read_string(command.payload, "line_id"),
        constraint == "none" ? std::nullopt
                              : std::make_optional(constraint));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_sketch_line_constraints") {
    const auto document = document_manager().clear_sketch_line_constraints(
        read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_equal_length_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_equal_length_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_perpendicular_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_perpendicular_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
