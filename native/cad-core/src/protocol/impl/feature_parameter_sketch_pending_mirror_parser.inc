  // Pending mirror state. Older saves (and most live document
  // states outside an in-progress mirror) won't have this key —
  // that's fine, the field stays as `nullopt`.
  if (payload.contains("pending_mirror") &&
      payload.at("pending_mirror").is_object()) {
    polysmith::core::SketchFeatureParameters::PendingMirror pending{};
    const auto& pm = payload.at("pending_mirror");
    pending.axis_line_id = read_optional_string(pm, "axis_line_id");
    if (pm.contains("object_ids") && pm.at("object_ids").is_array()) {
      for (const auto& id_value : pm.at("object_ids")) {
        if (id_value.is_string()) {
          pending.object_ids.push_back(id_value.get<std::string>());
        }
      }
    }
    // Generated geometry is regenerated on every parameter
    // change, so it doesn't need to round-trip through saves —
    // but we still rebuild it from the payload for the live
    // viewport/document round-trip during a session.
    if (pm.contains("generated_lines") &&
        pm.at("generated_lines").is_array()) {
      for (const auto& line_payload : pm.at("generated_lines")) {
        polysmith::core::SketchLine line{};
        line.id = read_string(line_payload, "line_id");
        line.start_vertex_id = read_string(line_payload, "start_vertex_id");
        line.end_vertex_id = read_string(line_payload, "end_vertex_id");
        line.start_x = read_number(line_payload, "start_x");
        line.start_y = read_number(line_payload, "start_y");
        line.end_x = read_number(line_payload, "end_x");
        line.end_y = read_number(line_payload, "end_y");
        if (line_payload.contains("is_construction") &&
            line_payload.at("is_construction").is_boolean()) {
          line.is_construction = line_payload.at("is_construction").get<bool>();
        }
        pending.generated_lines.push_back(line);
      }
    }
    if (pm.contains("generated_circles") &&
        pm.at("generated_circles").is_array()) {
      for (const auto& circle_payload : pm.at("generated_circles")) {
        polysmith::core::SketchCircle circle{};
        circle.id = read_string(circle_payload, "circle_id");
        circle.center_x = read_number(circle_payload, "center_x");
        circle.center_y = read_number(circle_payload, "center_y");
        circle.radius = read_number(circle_payload, "radius");
        if (circle_payload.contains("is_construction") &&
            circle_payload.at("is_construction").is_boolean()) {
          circle.is_construction =
              circle_payload.at("is_construction").get<bool>();
        }
        pending.generated_circles.push_back(circle);
      }
    }
    params.pending_mirror = pending;
  }
