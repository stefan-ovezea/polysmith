  if (command.type == "corner_trim_sketch_entities") {
    const auto document = document_manager().corner_trim_sketch_entities(
        read_string(command.payload, "entity_a_id"),
        read_string(command.payload, "entity_b_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "corner_trim_preview") {
    const std::string entity_a_id =
        read_string(command.payload, "entity_a_id");
    const std::string entity_b_id =
        read_string(command.payload, "entity_b_id");
    const double cursor_x = read_dimension(command.payload, "cursor_x");
    const double cursor_y = read_dimension(command.payload, "cursor_y");

    const auto doc_opt = document_manager().get_document();
    if (!doc_opt.has_value() ||
        !doc_opt->active_sketch_feature_id.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "corner_trim_preview_result" },
            { "payload", nullptr } });
      return;
    }

    const auto feature_it = std::find_if(
        doc_opt->feature_history.begin(),
        doc_opt->feature_history.end(),
        [&](const auto& f) {
          return f.id == doc_opt->active_sketch_feature_id.value();
        });

    if (feature_it == doc_opt->feature_history.end() ||
        !feature_it->sketch_parameters.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "corner_trim_preview_result" },
            { "payload", nullptr } });
      return;
    }

    const auto preview = polysmith::core::corner_trim_preview(
        *feature_it, entity_a_id, entity_b_id, cursor_x, cursor_y);

    nlohmann::json result;
    result["entity_a_id"] = entity_a_id;
    result["entity_b_id"] = entity_b_id;
    result["valid"] = preview.valid;
    result["revision"] = doc_opt->revision;
    if (preview.valid) {
      result["corner"] = {preview.corner_x, preview.corner_y};
      auto segment_json = [](const polysmith::core::CornerTrimPreviewSegment& seg) {
        nlohmann::json j;
        j["kind"] = seg.kind;
        j["start"] = {seg.sx, seg.sy};
        j["end"] = {seg.ex, seg.ey};
        if (seg.kind == "arc") {
          j["center"] = {seg.center_x, seg.center_y};
          j["radius"] = seg.radius;
          j["ccw"] = seg.ccw;
        }
        return j;
      };
      result["a"] = segment_json(preview.a);
      result["b"] = segment_json(preview.b);
    }
    polysmith::protocol::write_message(
        { { "id", command.id },
          { "type", "corner_trim_preview_result" },
          { "payload", result } });
    return;
  }
