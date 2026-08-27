  if (command.type == "add_parameter") {
    std::string kind = "length";
    if (command.payload.contains("kind") &&
        command.payload.at("kind").is_string()) {
      kind = command.payload.at("kind").get<std::string>();
    }
    const auto document = document_manager().add_parameter(
        read_string(command.payload, "name"),
        read_string(command.payload, "expression"),
        kind);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_parameter") {
    std::string kind = "length";
    if (command.payload.contains("kind") &&
        command.payload.at("kind").is_string()) {
      kind = command.payload.at("kind").get<std::string>();
    }
    const auto document = document_manager().update_parameter(
        read_string(command.payload, "name"),
        read_string(command.payload, "expression"),
        kind);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_selection_filter") {
    auto filter = polysmith::core::SelectionFilter{};
    // Read booleans from payload with defaults from the struct
    if (command.payload.contains("select_curves") &&
        command.payload.at("select_curves").is_boolean()) {
      filter.select_curves = command.payload.at("select_curves").get<bool>();
    }
    if (command.payload.contains("select_points") &&
        command.payload.at("select_points").is_boolean()) {
      filter.select_points = command.payload.at("select_points").get<bool>();
    }
    if (command.payload.contains("select_construction") &&
        command.payload.at("select_construction").is_boolean()) {
      filter.select_construction = command.payload.at("select_construction").get<bool>();
    }
    if (command.payload.contains("select_constraints") &&
        command.payload.at("select_constraints").is_boolean()) {
      filter.select_constraints = command.payload.at("select_constraints").get<bool>();
    }
    if (command.payload.contains("snap_endpoint") &&
        command.payload.at("snap_endpoint").is_boolean()) {
      filter.snap_endpoint = command.payload.at("snap_endpoint").get<bool>();
    }
    if (command.payload.contains("snap_midpoint") &&
        command.payload.at("snap_midpoint").is_boolean()) {
      filter.snap_midpoint = command.payload.at("snap_midpoint").get<bool>();
    }
    if (command.payload.contains("snap_center") &&
        command.payload.at("snap_center").is_boolean()) {
      filter.snap_center = command.payload.at("snap_center").get<bool>();
    }
    if (command.payload.contains("snap_intersection") &&
        command.payload.at("snap_intersection").is_boolean()) {
      filter.snap_intersection = command.payload.at("snap_intersection").get<bool>();
    }
    if (command.payload.contains("snap_nearest") &&
        command.payload.at("snap_nearest").is_boolean()) {
      filter.snap_nearest = command.payload.at("snap_nearest").get<bool>();
    }
    if (command.payload.contains("snap_quadrant") &&
        command.payload.at("snap_quadrant").is_boolean()) {
      filter.snap_quadrant = command.payload.at("snap_quadrant").get<bool>();
    }
    if (command.payload.contains("snap_perpendicular") &&
        command.payload.at("snap_perpendicular").is_boolean()) {
      filter.snap_perpendicular = command.payload.at("snap_perpendicular").get<bool>();
    }
    if (command.payload.contains("snap_parallel") &&
        command.payload.at("snap_parallel").is_boolean()) {
      filter.snap_parallel = command.payload.at("snap_parallel").get<bool>();
    }
    if (command.payload.contains("snap_tangent") &&
        command.payload.at("snap_tangent").is_boolean()) {
      filter.snap_tangent = command.payload.at("snap_tangent").get<bool>();
    }
    if (command.payload.contains("snap_grid") &&
        command.payload.at("snap_grid").is_boolean()) {
      filter.snap_grid = command.payload.at("snap_grid").get<bool>();
    }
    if (command.payload.contains("snap_grid_line") &&
        command.payload.at("snap_grid_line").is_boolean()) {
      filter.snap_grid_line = command.payload.at("snap_grid_line").get<bool>();
    }
    if (command.payload.contains("snap_polar") &&
        command.payload.at("snap_polar").is_boolean()) {
      filter.snap_polar = command.payload.at("snap_polar").get<bool>();
    }
    if (command.payload.contains("magnetic_pull") &&
        command.payload.at("magnetic_pull").is_boolean()) {
      filter.magnetic_pull = command.payload.at("magnetic_pull").get<bool>();
    }
    if (command.payload.contains("tolerance_px") &&
        command.payload.at("tolerance_px").is_number()) {
      filter.tolerance_px = command.payload.at("tolerance_px").get<int>();
    }

    const auto document = document_manager().update_selection_filter(filter);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_parameter") {
    const auto document = document_manager().delete_parameter(
        read_string(command.payload, "name"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "trim_sketch_entity") {
    // Optional segment_index: the hovered segment from the trim_preview
    // result. When present the core trims exactly the highlighted
    // segment instead of re-deriving it from the click point.
    std::optional<int> segment_index = std::nullopt;
    if (command.payload.contains("segment_index") &&
        command.payload["segment_index"].is_number()) {
      segment_index = command.payload["segment_index"].get<int>();
    }
    const auto document = document_manager().trim_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"),
        segment_index);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
