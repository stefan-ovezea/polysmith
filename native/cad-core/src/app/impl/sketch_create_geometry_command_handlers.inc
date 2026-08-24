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
        read_string(command.payload, "vertex_id"), host_line_id);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_vertex_line_anchor") {
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
    const auto document = document_manager().set_sketch_vertex_line_anchor(
        read_string(command.payload, "vertex_id"), host_line_id, t);

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
    // Optional creation-mode fields (two_point / three_point /
    // tangent_two_lines / tangent_three_lines). Absent fields fall
    // back to the wrapper defaults so the plain center+radius flow
    // is unchanged.
    const auto optional_string = [&](const char* key) {
      if (command.payload.contains(key) &&
          command.payload.at(key).is_string()) {
        return command.payload.at(key).get<std::string>();
      }
      return std::string{};
    };
    const auto optional_dimension = [&](const char* key) {
      if (command.payload.contains(key) &&
          command.payload.at(key).is_number()) {
        return command.payload.at(key).get<double>();
      }
      return 0.0;
    };
    const auto document = document_manager().add_sketch_circle(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"),
        is_construction,
        optional_string("mode"),
        optional_dimension("p1_x"),
        optional_dimension("p1_y"),
        optional_dimension("p2_x"),
        optional_dimension("p2_y"),
        optional_dimension("p3_x"),
        optional_dimension("p3_y"),
        optional_string("line_a_id"),
        optional_string("line_b_id"),
        optional_string("line_c_id"),
        optional_dimension("hint_x"),
        optional_dimension("hint_y"));

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

  if (command.type == "add_sketch_ellipse") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_ellipse(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "axis_a_x"),
        read_dimension(command.payload, "axis_a_y"),
        read_dimension(command.payload, "axis_b_x"),
        read_dimension(command.payload, "axis_b_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_spline") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    std::vector<std::pair<double, double>> points;
    if (command.payload.contains("points") &&
        command.payload.at("points").is_array()) {
      for (const auto& point : command.payload.at("points")) {
        points.push_back({read_dimension(point, "x"),
                          read_dimension(point, "y")});
      }
    }
    const auto document = document_manager().add_sketch_spline(
        points, is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_slot") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_slot(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "length"),
        read_dimension(command.payload, "radius"),
        read_dimension(command.payload, "rotation"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_slot") {
    const auto document = document_manager().update_sketch_slot(
        read_string(command.payload, "slot_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "length"),
        read_dimension(command.payload, "radius"),
        read_dimension(command.payload, "rotation"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
