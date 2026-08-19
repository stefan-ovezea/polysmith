  // Older saves predate sketch text; absence of the key means no texts
  // (any generated glyph lines in such a save are orphans — the first
  // refresh strips them via the generated_by flag).
  if (payload.contains("texts") && payload.at("texts").is_array()) {
    for (const auto& text_payload : payload.at("texts")) {
      polysmith::core::SketchText entry{};
      entry.id = read_string(text_payload, "text_id");
      entry.text = read_string(text_payload, "text");
      entry.font_path = read_optional_string(text_payload, "font_path").value_or("");
      entry.height_mm = read_number(text_payload, "height_mm");
      entry.angle_deg = read_number(text_payload, "angle_deg");
      entry.anchor_x = read_number(text_payload, "anchor_x");
      entry.anchor_y = read_number(text_payload, "anchor_y");
      entry.h_align = read_optional_string(text_payload, "h_align").value_or("center");
      entry.v_align = read_optional_string(text_payload, "v_align").value_or("middle");
      entry.char_spacing = read_optional_number(text_payload, "char_spacing", 0.0);
      entry.path_entity_id = read_optional_string(text_payload, "path_entity_id");
      if (text_payload.contains("path_offset") &&
          text_payload.at("path_offset").is_number()) {
        entry.path_offset = text_payload.at("path_offset").get<double>();
      }
      params.texts.push_back(std::move(entry));
    }
  }
