  if (command.type == "add_sketch_text") {
    polysmith::core::SketchText text{};
    text.text = command.payload.contains("text") &&
                        command.payload.at("text").is_string()
                    ? command.payload.at("text").get<std::string>()
                    : std::string("Text");
    text.font_path =
        read_optional_string(command.payload, "font_path").value_or("");
    text.height_mm =
        command.payload.contains("height_mm") &&
                command.payload.at("height_mm").is_number()
            ? command.payload.at("height_mm").get<double>()
            : 10.0;
    text.angle_deg =
        command.payload.contains("angle_deg") &&
                command.payload.at("angle_deg").is_number()
            ? command.payload.at("angle_deg").get<double>()
            : 0.0;
    text.anchor_x = read_dimension(command.payload, "anchor_x");
    text.anchor_y = read_dimension(command.payload, "anchor_y");
    text.h_align =
        command.payload.contains("h_align") &&
                command.payload.at("h_align").is_string()
            ? command.payload.at("h_align").get<std::string>()
            : std::string("center");
    text.v_align =
        command.payload.contains("v_align") &&
                command.payload.at("v_align").is_string()
            ? command.payload.at("v_align").get<std::string>()
            : std::string("middle");
    text.char_spacing =
        command.payload.contains("char_spacing") &&
                command.payload.at("char_spacing").is_number()
            ? command.payload.at("char_spacing").get<double>()
            : 0.0;

    const auto document = document_manager().add_sketch_text(text);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_text") {
    const std::string text_id = read_string(command.payload, "text_id");

    // Merge the partial patch over the stored parameters (the same
    // optional-field semantics as the mirror preview handlers).
    polysmith::core::SketchText merged{};
    const auto current = document_manager().get_document();
    if (current.has_value() &&
        current->active_sketch_feature_id.has_value()) {
      const auto feature_it = std::find_if(
          current->feature_history.begin(),
          current->feature_history.end(),
          [&](const polysmith::core::FeatureEntry& feature) {
            return feature.id ==
                   current->active_sketch_feature_id.value();
          });
      if (feature_it != current->feature_history.end() &&
          feature_it->sketch_parameters.has_value()) {
        const auto text_it = std::find_if(
            feature_it->sketch_parameters->texts.begin(),
            feature_it->sketch_parameters->texts.end(),
            [&](const polysmith::core::SketchText& entry) {
              return entry.id == text_id;
            });
        if (text_it != feature_it->sketch_parameters->texts.end()) {
          merged = *text_it;
        }
      }
    }
    if (merged.id.empty()) {
      throw std::runtime_error("Sketch text not found: " + text_id);
    }
    if (command.payload.contains("text") &&
        command.payload.at("text").is_string()) {
      merged.text = command.payload.at("text").get<std::string>();
    }
    if (command.payload.contains("font_path")) {
      merged.font_path =
          read_optional_string(command.payload, "font_path").value_or("");
    }
    if (command.payload.contains("height_mm") &&
        command.payload.at("height_mm").is_number()) {
      merged.height_mm = command.payload.at("height_mm").get<double>();
    }
    if (command.payload.contains("angle_deg") &&
        command.payload.at("angle_deg").is_number()) {
      merged.angle_deg = command.payload.at("angle_deg").get<double>();
    }
    if (command.payload.contains("anchor_x") &&
        command.payload.at("anchor_x").is_number()) {
      merged.anchor_x = command.payload.at("anchor_x").get<double>();
    }
    if (command.payload.contains("anchor_y") &&
        command.payload.at("anchor_y").is_number()) {
      merged.anchor_y = command.payload.at("anchor_y").get<double>();
    }
    if (command.payload.contains("h_align") &&
        command.payload.at("h_align").is_string()) {
      merged.h_align = command.payload.at("h_align").get<std::string>();
    }
    if (command.payload.contains("v_align") &&
        command.payload.at("v_align").is_string()) {
      merged.v_align = command.payload.at("v_align").get<std::string>();
    }
    if (command.payload.contains("char_spacing") &&
        command.payload.at("char_spacing").is_number()) {
      merged.char_spacing =
          command.payload.at("char_spacing").get<double>();
    }

    const auto document =
        document_manager().update_sketch_text(text_id, merged);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_text") {
    const auto document = document_manager().delete_sketch_text(
        read_string(command.payload, "text_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
