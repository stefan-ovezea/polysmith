  if (command.type == "project_face_into_sketch") {
    const auto document = document_manager().project_face_into_sketch(
        read_string(command.payload, "face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_profile_into_sketch") {
    const auto document = document_manager().project_profile_into_sketch(
        read_string(command.payload, "profile_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_edge_into_sketch") {
    const auto document = document_manager().project_edge_into_sketch(
        read_string(command.payload, "edge_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_vertex_into_sketch") {
    const auto document = document_manager().project_vertex_into_sketch(
        read_string(command.payload, "vertex_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_body_into_sketch") {
    const auto document = document_manager().project_body_into_sketch(
        read_string(command.payload, "body_id"),
        read_string(command.payload, "mode"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "remove_sketch_projections") {
    const auto document = document_manager().remove_sketch_projections(
        read_string(command.payload, "feature_id"),
        read_bool(command.payload, "keep_geometry"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
