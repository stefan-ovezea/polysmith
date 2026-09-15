  if (command.type == "add_sketch_fillet") {
    // Arc operands are optional (old clients send line ids only);
    // the kind dispatch in the create path validates the pairs.
    const auto document = document_manager().add_sketch_fillet(
        read_string(command.payload, "corner_vertex_id"),
        read_string(command.payload, "line_a_id"),
        read_string(command.payload, "line_b_id"),
        read_dimension(command.payload, "radius"),
        read_optional_string(command.payload, "arc_a_id").value_or(""),
        read_optional_string(command.payload, "arc_b_id").value_or(""));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_fillet_radius") {
    const auto document = document_manager().update_sketch_fillet_radius(
        read_string(command.payload, "fillet_id"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_fillet") {
    const auto document = document_manager().delete_sketch_fillet(
        read_string(command.payload, "fillet_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "merge_coincident_sketch_points") {
    const auto document = document_manager().merge_coincident_sketch_points(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
