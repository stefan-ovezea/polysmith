  if (command.type == "create_fillet") {
    // Accept either a single `edge_id` (legacy single-edge payload)
    // or an `edge_ids` array (multi-select). Reading both so old
    // clients that haven't migrated still work.
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
    const auto document = document_manager().create_fillet(
        edge_ids, read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fillet_edges") {
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_fillet_edges(
        feature_id, edge_ids);

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fillet_radius") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = document_manager().update_fillet_radius(
        feature_id, read_dimension(command.payload, "radius"));

    if (emit_feature_changed(command.id, feature_id, document)) {
      return;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_fillet") {
    const auto document = document_manager().confirm_fillet(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
