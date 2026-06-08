  if (command.type == "create_chamfer") {
    // Same dual-payload handling as create_fillet — see comment there.
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    } else {
      edge_ids.push_back(read_string(command.payload, "edge_id"));
    }
    const auto document = document_manager().create_chamfer(
        edge_ids, read_dimension(command.payload, "distance"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_chamfer_edges") {
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const auto document = document_manager().update_chamfer_edges(
        read_string(command.payload, "feature_id"), edge_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_chamfer_distance") {
    const auto document = document_manager().update_chamfer_distance(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "distance"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_chamfer") {
    const auto document = document_manager().confirm_chamfer(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
