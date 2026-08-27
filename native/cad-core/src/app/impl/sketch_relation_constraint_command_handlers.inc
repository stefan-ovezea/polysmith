  if (command.type == "set_sketch_tangent_constraint") {
    const auto document = document_manager().set_sketch_tangent_constraint(
        read_string(command.payload, "line_id"),
        read_string(command.payload, "circle_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_parallel_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_parallel_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_coincident_constraint") {
    const auto document = document_manager().set_sketch_coincident_constraint(
        read_string(command.payload, "vertex_id"),
        read_string(command.payload, "other_vertex_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_coincident_constraint") {
    const auto document =
        document_manager().delete_sketch_coincident_constraint(
            read_string(command.payload, "constraint_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_symmetric_constraint") {
    const auto document = document_manager().set_sketch_symmetric_constraint(
        read_string(command.payload, "point_id"),
        read_string(command.payload, "other_point_id"),
        read_string(command.payload, "axis_line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_midpoint_constraint") {
    const auto document = document_manager().set_sketch_midpoint_constraint(
        read_string(command.payload, "vertex_id"),
        read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_collinear_constraint") {
    const auto document = document_manager().set_sketch_collinear_constraint(
        read_string(command.payload, "line_id"),
        read_string(command.payload, "other_line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_tangent_pair_constraint") {
    const auto document = document_manager().set_sketch_tangent_pair_constraint(
        read_string(command.payload, "first_id"),
        read_string(command.payload, "second_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
