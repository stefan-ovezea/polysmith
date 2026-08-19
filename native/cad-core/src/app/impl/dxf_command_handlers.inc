  if (command.type == "import_dxf") {
    const auto document = document_manager().import_dxf(
        read_string(command.payload, "file_path"),
        read_optional_string(command.payload, "plane_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "export_document_dxf") {
    const auto export_result = document_manager().export_document_as_dxf(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", export_result.file_path},
                            {"format", export_result.format},
                            {"exported_feature_count",
                             export_result.exported_feature_count},
                        }));
    return;
  }
