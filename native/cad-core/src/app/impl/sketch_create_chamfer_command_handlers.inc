  if (command.type == "add_sketch_chamfer") {
    const auto document = document_manager().add_sketch_chamfer(
        read_string(command.payload, "corner_vertex_id"),
        read_string(command.payload, "line_a_id"),
        read_string(command.payload, "line_b_id"),
        read_dimension(command.payload, "distance_a"),
        read_dimension(command.payload, "distance_b"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_chamfer") {
    const auto document = document_manager().update_sketch_chamfer(
        read_string(command.payload, "chamfer_id"),
        read_dimension(command.payload, "distance_a"),
        read_dimension(command.payload, "distance_b"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_chamfer") {
    const auto before_document = document_manager().get_document();
    const auto document = document_manager().delete_sketch_chamfer(
        read_string(command.payload, "chamfer_id"));

    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
