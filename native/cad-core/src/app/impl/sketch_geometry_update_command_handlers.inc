  if (command.type == "update_sketch_line") {
    const auto document = document_manager().update_sketch_line(
        read_string(command.payload, "line_id"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_vertex") {
    const auto before_document = document_manager().get_document();
    const auto document = document_manager().update_sketch_vertex(
        read_string(command.payload, "vertex_id"),
        read_dimension(command.payload, "x"),
        read_dimension(command.payload, "y"));

    // Phase D1 (wiki/IPC-Payload-Plan.md): reply with only the entities this
    // command changed.  Vertices and lines store their own coordinates, so the
    // diff needs no serialisation, and `document_state` stays the fallback for
    // anything the diff cannot describe.
    const std::string active_id =
        document.active_sketch_feature_id.value_or("");
    const polysmith::core::FeatureEntry* before_feature = nullptr;
    const polysmith::core::FeatureEntry* after_feature = nullptr;
    if (before_document.has_value() && !active_id.empty()) {
      for (const auto& feature : before_document->feature_history) {
        if (feature.id == active_id) {
          before_feature = &feature;
        }
      }
      for (const auto& feature : document.feature_history) {
        if (feature.id == active_id) {
          after_feature = &feature;
        }
      }
    }

    if (before_feature != nullptr && after_feature != nullptr &&
        before_feature->sketch_parameters.has_value() &&
        after_feature->sketch_parameters.has_value()) {
      const auto& before_params = before_feature->sketch_parameters.value();
      const auto& after_params = after_feature->sketch_parameters.value();
      polysmith::protocol::json changed_vertices = polysmith::protocol::json::array();
      polysmith::protocol::json changed_lines = polysmith::protocol::json::array();
      polysmith::protocol::json removed_ids = polysmith::protocol::json::array();

      for (const auto& vertex : after_params.vertices) {
        const polysmith::core::SketchVertex* previous = nullptr;
        for (const auto& candidate : before_params.vertices) {
          if (candidate.vertex_id == vertex.vertex_id) {
            previous = &candidate;
            break;
          }
        }
        if (previous == nullptr || previous->x != vertex.x ||
            previous->y != vertex.y) {
          changed_vertices.push_back({
              {"vertex_id", vertex.vertex_id},
              {"x", vertex.x},
              {"y", vertex.y},
          });
        }
      }
      for (const auto& vertex : before_params.vertices) {
        bool still_there = false;
        for (const auto& candidate : after_params.vertices) {
          if (candidate.vertex_id == vertex.vertex_id) {
            still_there = true;
            break;
          }
        }
        if (!still_there) {
          removed_ids.push_back(vertex.vertex_id);
        }
      }
      for (const auto& line : after_params.lines) {
        const polysmith::core::SketchLine* previous = nullptr;
        for (const auto& candidate : before_params.lines) {
          if (candidate.id == line.id) {
            previous = &candidate;
            break;
          }
        }
        if (previous == nullptr || previous->start_x != line.start_x ||
            previous->start_y != line.start_y || previous->end_x != line.end_x ||
            previous->end_y != line.end_y) {
          changed_lines.push_back({
              {"line_id", line.id},
              {"start_x", line.start_x},
              {"start_y", line.start_y},
              {"end_x", line.end_x},
              {"end_y", line.end_y},
          });
        }
      }
      for (const auto& line : before_params.lines) {
        bool still_there = false;
        for (const auto& candidate : after_params.lines) {
          if (candidate.id == line.id) {
            still_there = true;
            break;
          }
        }
        if (!still_there) {
          removed_ids.push_back(line.id);
        }
      }

      if (!changed_vertices.empty() || !changed_lines.empty() ||
          !removed_ids.empty()) {
        polysmith::protocol::write_message(
            polysmith::protocol::make_sketch_entities_changed_event(
                command.id,
                polysmith::protocol::json{
                    {"revision", document.revision},
                    {"feature_id", active_id},
                    {"lines", changed_lines},
                    {"vertices", changed_vertices},
                    {"removed", removed_ids},
                    {"derived",
                     polysmith::protocol::json{
                         {"profile_count", after_params.profiles.size()},
                         {"vertex_count", after_params.vertices.size()},
                     }},
                }));
        return;
      }
    }

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "move_sketch_entities" ||
      command.type == "transform_sketch_entities") {
    std::vector<std::string> entity_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& entry : command.payload.at("entity_ids")) {
        if (entry.is_string()) {
          entity_ids.push_back(entry.get<std::string>());
        }
      }
    }
    if (command.type == "transform_sketch_entities") {
      bool copy = false;
      if (command.payload.contains("copy") &&
          command.payload.at("copy").is_boolean()) {
        copy = command.payload.at("copy").get<bool>();
      }
      const auto document = document_manager().transform_sketch_entities(
          entity_ids,
          read_dimension(command.payload, "dx"),
          read_dimension(command.payload, "dy"),
          read_dimension(command.payload, "center_x"),
          read_dimension(command.payload, "center_y"),
          read_dimension(command.payload, "angle_deg"),
          read_dimension(command.payload, "scale"),
          copy);

      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document)));
      return;
    }
    const auto document = document_manager().move_sketch_entities(
        entity_ids,
        read_dimension(command.payload, "dx"),
        read_dimension(command.payload, "dy"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "angle_deg"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
