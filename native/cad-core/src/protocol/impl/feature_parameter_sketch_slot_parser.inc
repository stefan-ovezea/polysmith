  // Older saves predate sketch slots; absence of the key means no slots
  // (any generated slot lines/arcs in such a save are orphans — the
  // first refresh strips them via the generated_by flag).
  if (payload.contains("slots") && payload.at("slots").is_array()) {
    for (const auto& slot_payload : payload.at("slots")) {
      polysmith::core::SketchSlot slot{};
      slot.id = read_string(slot_payload, "slot_id");
      slot.center_vertex_id = read_string(slot_payload, "center_vertex_id");
      slot.center_x = read_number(slot_payload, "center_x");
      slot.center_y = read_number(slot_payload, "center_y");
      slot.length = read_number(slot_payload, "length");
      slot.radius = read_number(slot_payload, "radius");
      slot.rotation = read_number(slot_payload, "rotation");
      slot.mode = read_optional_string(slot_payload, "mode").value_or("straight");
      if (slot_payload.contains("is_construction") &&
          slot_payload.at("is_construction").is_boolean()) {
        slot.is_construction = slot_payload.at("is_construction").get<bool>();
      }
      params.slots.push_back(std::move(slot));
    }
  }
