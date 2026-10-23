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
      // Arc-slot fields (T2.3). Absent in saves that predate arc slots; those
      // saves can only contain straight slots, so the defaults are harmless.
      if (slot_payload.contains("arc_radius") &&
          slot_payload.at("arc_radius").is_number()) {
        slot.arc_radius = slot_payload.at("arc_radius").get<double>();
      }
      if (slot_payload.contains("start_angle") &&
          slot_payload.at("start_angle").is_number()) {
        slot.start_angle = slot_payload.at("start_angle").get<double>();
      }
      if (slot_payload.contains("sweep_angle") &&
          slot_payload.at("sweep_angle").is_number()) {
        slot.sweep_angle = slot_payload.at("sweep_angle").get<double>();
      }
      if (slot_payload.contains("is_construction") &&
          slot_payload.at("is_construction").is_boolean()) {
        slot.is_construction = slot_payload.at("is_construction").get<bool>();
      }
      params.slots.push_back(std::move(slot));
    }
  }
