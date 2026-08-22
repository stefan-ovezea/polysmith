  // Older saves predate sketch chamfers; absence of the key means no
  // chamfers (any orphaned chamfer lines in such a save round-trip as
  // plain entities).
  if (payload.contains("chamfers") && payload.at("chamfers").is_array()) {
    for (const auto& chamfer_payload : payload.at("chamfers")) {
      polysmith::core::SketchChamfer chamfer{};
      chamfer.id = read_string(chamfer_payload, "chamfer_id");
      chamfer.corner_vertex_id = read_string(chamfer_payload, "corner_vertex_id");
      chamfer.corner_x = read_number(chamfer_payload, "corner_x");
      chamfer.corner_y = read_number(chamfer_payload, "corner_y");
      chamfer.line_a_id = read_string(chamfer_payload, "line_a_id");
      chamfer.line_b_id = read_string(chamfer_payload, "line_b_id");
      chamfer.trim_a_vertex_id = read_string(chamfer_payload, "trim_a_vertex_id");
      chamfer.trim_b_vertex_id = read_string(chamfer_payload, "trim_b_vertex_id");
      chamfer.chamfer_line_id = read_string(chamfer_payload, "chamfer_line_id");
      chamfer.distance_a = read_number(chamfer_payload, "distance_a");
      chamfer.distance_b = read_number(chamfer_payload, "distance_b");
      params.chamfers.push_back(std::move(chamfer));
    }
  }
