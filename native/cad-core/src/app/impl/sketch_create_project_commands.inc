  if (command.type == "add_sketch_line") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_line(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_midpoint_anchor") {
    std::string host_line_id;
    if (command.payload.contains("host_line_id") &&
        command.payload.at("host_line_id").is_string()) {
      host_line_id = command.payload.at("host_line_id").get<std::string>();
    }
    const auto document = document_manager().set_sketch_midpoint_anchor(
        read_string(command.payload, "point_id"), host_line_id);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_point_line_anchor") {
    std::string host_line_id;
    if (command.payload.contains("host_line_id") &&
        command.payload.at("host_line_id").is_string()) {
      host_line_id = command.payload.at("host_line_id").get<std::string>();
    }
    double t = 0.5;
    if (command.payload.contains("t") &&
        command.payload.at("t").is_number()) {
      t = command.payload.at("t").get<double>();
    }
    const auto document = document_manager().set_sketch_point_line_anchor(
        read_string(command.payload, "point_id"), host_line_id, t);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_line_construction") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().set_sketch_line_construction(
        read_string(command.payload, "line_id"), is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_rectangle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_rectangle(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_arc") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_arc(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        read_dimension(command.payload, "anchor_x"),
        read_dimension(command.payload, "anchor_y"),
        read_string(command.payload, "mode"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_circle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_circle(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_polygon") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_polygon(
        static_cast<int>(read_dimension(command.payload, "sides")),
        read_string(command.payload, "mode"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_fillet") {
    const auto document = document_manager().add_sketch_fillet(
        read_string(command.payload, "corner_point_id"),
        read_string(command.payload, "line_a_id"),
        read_string(command.payload, "line_b_id"),
        read_dimension(command.payload, "radius"));

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

  if (command.type == "delete_sketch_dimension") {
    const auto document = document_manager().delete_sketch_dimension(
        read_string(command.payload, "dimension_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension_display") {
    const auto document = document_manager().update_sketch_dimension_display(
        read_string(command.payload, "dimension_id"),
        read_string(command.payload, "display_as"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_selection") {
    std::vector<std::string> entity_ids;
    std::vector<std::string> point_ids;
    std::vector<std::string> profile_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& id : command.payload.at("entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("point_ids") &&
        command.payload.at("point_ids").is_array()) {
      for (const auto& id : command.payload.at("point_ids")) {
        if (id.is_string()) {
          point_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document =
        document_manager().delete_sketch_selection(
            entity_ids, point_ids, profile_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_point") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_sketch_point(
        read_string(command.payload, "point_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_point_fixed") {
    const auto document = document_manager().set_sketch_point_fixed(
        read_string(command.payload, "point_id"),
        command.payload.at("is_fixed").get<bool>());

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_entity") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_sketch_entity(
        read_string(command.payload, "entity_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_dimension") {
    const auto document = document_manager().select_sketch_dimension(
        read_string(command.payload, "dimension_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "finish_sketch") {
    const auto document = document_manager().finish_sketch();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "reenter_sketch") {
    const auto document = document_manager().reenter_sketch(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

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

  if (command.type == "clear_selection") {
    const auto document = document_manager().clear_selection();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
