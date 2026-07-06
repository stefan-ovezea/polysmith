  if (payload.contains("lines") && payload.at("lines").is_array()) {
    for (const auto& line_payload : payload.at("lines")) {
      polysmith::core::SketchLine line{};
      line.id = read_string(line_payload, "line_id");
      line.start_point_id = read_string(line_payload, "start_point_id");
      line.end_point_id = read_string(line_payload, "end_point_id");
      line.start_x = read_number(line_payload, "start_x");
      line.start_y = read_number(line_payload, "start_y");
      line.end_x = read_number(line_payload, "end_x");
      line.end_y = read_number(line_payload, "end_y");
      line.constraint = read_optional_string(line_payload, "constraint");
      // Older saves predate construction lines; default to solid.
      if (line_payload.contains("is_construction") &&
          line_payload.at("is_construction").is_boolean()) {
        line.is_construction = line_payload.at("is_construction").get<bool>();
      }
      params.lines.push_back(line);
    }
  }
  if (payload.contains("circles") && payload.at("circles").is_array()) {
    for (const auto& circle_payload : payload.at("circles")) {
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
      params.circles.push_back(circle);
    }
  }
  // Older saves predate arcs; absence of the key just means no arcs
  // exist, mirroring how `circles` and `lines` degrade gracefully.
  if (payload.contains("arcs") && payload.at("arcs").is_array()) {
    for (const auto& arc_payload : payload.at("arcs")) {
      polysmith::core::SketchArc arc{};
      arc.id = read_string(arc_payload, "arc_id");
      arc.start_point_id = read_string(arc_payload, "start_point_id");
      arc.end_point_id = read_string(arc_payload, "end_point_id");
      arc.center_x = read_number(arc_payload, "center_x");
      arc.center_y = read_number(arc_payload, "center_y");
      arc.radius = read_number(arc_payload, "radius");
      arc.start_x = read_number(arc_payload, "start_x");
      arc.start_y = read_number(arc_payload, "start_y");
      arc.end_x = read_number(arc_payload, "end_x");
      arc.end_y = read_number(arc_payload, "end_y");
      arc.ccw = read_bool(arc_payload, "ccw");
      if (arc_payload.contains("is_construction") &&
          arc_payload.at("is_construction").is_boolean()) {
        arc.is_construction = arc_payload.at("is_construction").get<bool>();
      }
      params.arcs.push_back(arc);
    }
  }
  // Older saves predate fillets; absence of the key just means no
  // fillets exist (lines/arcs that originated as fillet outputs are
  // already round-tripped above as plain entities).
  if (payload.contains("fillets") && payload.at("fillets").is_array()) {
    for (const auto& fillet_payload : payload.at("fillets")) {
      polysmith::core::SketchFillet fillet{};
      fillet.id = read_string(fillet_payload, "fillet_id");
      fillet.corner_point_id = read_string(fillet_payload, "corner_point_id");
      fillet.corner_x = read_number(fillet_payload, "corner_x");
      fillet.corner_y = read_number(fillet_payload, "corner_y");
      fillet.line_a_id = read_string(fillet_payload, "line_a_id");
      fillet.line_b_id = read_string(fillet_payload, "line_b_id");
      fillet.trim_a_point_id = read_string(fillet_payload, "trim_a_point_id");
      fillet.trim_b_point_id = read_string(fillet_payload, "trim_b_point_id");
      fillet.arc_id = read_string(fillet_payload, "arc_id");
      fillet.radius = read_number(fillet_payload, "radius");
      params.fillets.push_back(fillet);
    }
  }
  // Older saves predate polygons; absence of the key just means none exist.
  if (payload.contains("polygons") && payload.at("polygons").is_array()) {
    for (const auto& polygon_payload : payload.at("polygons")) {
      polysmith::core::SketchPolygon polygon{};
      polygon.id = read_string(polygon_payload, "polygon_id");
      polygon.center_x = read_number(polygon_payload, "center_x");
      polygon.center_y = read_number(polygon_payload, "center_y");
      polygon.radius = read_number(polygon_payload, "radius");
      polygon.sides = static_cast<int>(read_number(polygon_payload, "sides"));
      polygon.mode = read_string(polygon_payload, "mode");
      polygon.start_x = read_number(polygon_payload, "start_x");
      polygon.start_y = read_number(polygon_payload, "start_y");
      polygon.end_x = read_number(polygon_payload, "end_x");
      polygon.end_y = read_number(polygon_payload, "end_y");
      if (polygon_payload.contains("is_construction") &&
          polygon_payload.at("is_construction").is_boolean()) {
        polygon.is_construction =
            polygon_payload.at("is_construction").get<bool>();
      }
      params.polygons.push_back(polygon);
    }
  }
  if (payload.contains("points") && payload.at("points").is_array()) {
    for (const auto& point_payload : payload.at("points")) {
      polysmith::core::SketchVertex point{};
      point.id = read_string(point_payload, "point_id");
      point.kind = read_string(point_payload, "kind");
      point.x = read_number(point_payload, "x");
      point.y = read_number(point_payload, "y");
      point.is_fixed = read_bool(point_payload, "is_fixed");
      params.points.push_back(point);
    }
  }
