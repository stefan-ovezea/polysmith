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
    const auto document = document_manager().update_fillet_edges(
        read_string(command.payload, "feature_id"), edge_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fillet_radius") {
    const auto document = document_manager().update_fillet_radius(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "radius"));

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

  if (command.type == "create_shell") {
    const auto document = document_manager().create_shell(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "thickness"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_shell_thickness") {
    const auto document = document_manager().update_shell_thickness(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "thickness"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_shell") {
    const auto document = document_manager().confirm_shell(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_offset_plane") {
    const auto document = document_manager().create_offset_plane(
        read_string(command.payload, "source_plane_id"),
        read_dimension(command.payload, "offset"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_midplane") {
    std::vector<std::string> source_plane_ids;
    if (command.payload.contains("source_plane_ids") &&
        command.payload.at("source_plane_ids").is_array()) {
      for (const auto& id : command.payload.at("source_plane_ids")) {
        if (id.is_string()) {
          source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (source_plane_ids.size() != 2) {
      throw std::runtime_error("create_midplane requires two source_plane_ids");
    }
    const auto document = document_manager().create_midplane(
        source_plane_ids[0], source_plane_ids[1]);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_tangent_plane") {
    const auto document = document_manager().create_tangent_plane(
        read_string(command.payload, "source_face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_angle_plane") {
    const auto document = document_manager().create_angle_plane(
        read_string(command.payload, "source_plane_id"),
        read_string(command.payload, "source_axis_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_construction_axis") {
    const auto document = document_manager().create_construction_axis(
        read_string(command.payload, "source_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_construction_point") {
    const auto document = document_manager().create_construction_point(
        read_string(command.payload, "source_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_hole") {
    const auto document = document_manager().create_hole(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "center_z"),
        read_hole_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_hole_parameters") {
    const auto document = document_manager().update_hole_parameters(
        read_string(command.payload, "feature_id"),
        read_hole_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_hole") {
    const auto document = document_manager().confirm_hole(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_helix") {
    const auto document = document_manager().create_helix(
        read_string(command.payload, "axis_source_id"),
        read_helix_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_helix_parameters") {
    const auto document = document_manager().update_helix_parameters(
        read_string(command.payload, "feature_id"),
        read_helix_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_thread") {
    const auto document = document_manager().create_thread(
        read_thread_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_thread_parameters") {
    const auto document = document_manager().update_thread_parameters(
        read_string(command.payload, "feature_id"),
        read_thread_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_thread") {
    const auto document = document_manager().confirm_thread(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_fastener") {
    const auto document = document_manager().create_fastener(
        read_fastener_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fastener_parameters") {
    const auto document = document_manager().update_fastener_parameters(
        read_string(command.payload, "feature_id"),
        read_fastener_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_move") {
    const auto document = document_manager().create_move(
        read_string(command.payload, "target_body_id"),
        read_move_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_move_parameters") {
    const auto document = document_manager().update_move_parameters(
        read_string(command.payload, "feature_id"),
        read_move_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_move") {
    const auto document = document_manager().confirm_move(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_body_copy") {
    const auto document = document_manager().create_body_copy(
        read_string(command.payload, "source_body_id"),
        read_optional_string(command.payload, "copy_mode").value_or("linked"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "unlink_body_copy") {
    const auto document = document_manager().unlink_body_copy(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_offset_plane") {
    const auto document = document_manager().update_offset_plane(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "offset"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_angle_plane") {
    const auto document = document_manager().update_angle_plane(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
