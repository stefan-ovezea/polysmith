  if (command.type == "import_stl") {
    const auto document = document_manager().import_stl(
        read_string(command.payload, "file_path"),
        read_optional_dimension(command.payload, "scale", /*fallback=*/1.0));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "convert_mesh_to_body") {
    const auto document = document_manager().convert_mesh_to_body(
        read_string(command.payload, "body_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "detach_body_projections") {
    const auto document = document_manager().detach_body_projections(
        read_string(command.payload, "body_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
