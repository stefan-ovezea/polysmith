polysmith::core::ExtrudeFeatureParameters
extrude_parameters_from_payload(const json& payload) {
  return parse_extrude_parameters_from_payload(payload);
}

json plane_frame_to_payload(const polysmith::core::PlaneFrame& frame) {
  return {
      {"origin",
       {
           {"x", frame.origin_x},
           {"y", frame.origin_y},
           {"z", frame.origin_z},
       }},
      {"x_axis",
       {
           {"x", frame.x_axis_x},
           {"y", frame.x_axis_y},
           {"z", frame.x_axis_z},
       }},
      {"y_axis",
       {
           {"x", frame.y_axis_x},
           {"y", frame.y_axis_y},
           {"z", frame.y_axis_z},
       }},
      {"normal",
       {
           {"x", frame.normal_x},
           {"y", frame.normal_y},
           {"z", frame.normal_z},
       }},
  };
}

json profile_points_to_payload(
    const std::vector<polysmith::core::SketchProfilePoint>& points) {
  json payload = json::array();
  for (const auto& point : points) {
    payload.push_back({{"x", point.x}, {"y", point.y}});
  }
  return payload;
}

// ── CAM Stock & Setup ───────────────────────────────────────────

json to_payload(const polysmith::core::CamStockDefinition& stock) {
  return {
      {"width", stock.width},
      {"height", stock.height},
      {"depth", stock.depth},
      {"offset_x", stock.offsetX},
      {"offset_y", stock.offsetY},
      {"offset_z", stock.offsetZ},
  };
}

json to_payload(const polysmith::core::CamSetup& setup) {
  return {
      {"setup_id", setup.id},
      {"stock", to_payload(setup.stock)},
      {"wcs_origin",
       {
           {"x", setup.wcsOriginX},
           {"y", setup.wcsOriginY},
           {"z", setup.wcsOriginZ},
       }},
      {"safety_plane_z", setup.safetyPlaneZ},
      {"axis_count", setup.axisCount},
      {"wcs_angle", setup.wcsAngle},
  };
}

polysmith::core::CamStockDefinition stock_from_payload(const json& payload) {
  polysmith::core::CamStockDefinition stock;
  if (payload.contains("width")) stock.width = payload["width"].get<double>();
  if (payload.contains("height")) stock.height = payload["height"].get<double>();
  if (payload.contains("depth")) stock.depth = payload["depth"].get<double>();
  if (payload.contains("offset_x")) stock.offsetX = payload["offset_x"].get<double>();
  if (payload.contains("offset_y")) stock.offsetY = payload["offset_y"].get<double>();
  if (payload.contains("offset_z")) stock.offsetZ = payload["offset_z"].get<double>();
  return stock;
}

json to_payload(const polysmith::core::CamToolDefinition& tool) {
  return {
      {"tool_id", tool.id},
      {"name", tool.name},
      {"tool_number", tool.toolNumber},
      {"diameter", tool.diameter},
      {"flute_length", tool.fluteLength},
      {"overall_length", tool.overallLength},
      {"corner_radius", tool.cornerRadius},
      {"spindle_speed", tool.spindleSpeed},
      {"feed_rate", tool.feedRate},
      {"stepover", tool.stepover},
      {"stepdown", tool.stepdown},
      {"type", static_cast<int>(tool.type)},
  };
}

polysmith::core::CamSetup setup_from_payload(const json& payload) {
  polysmith::core::CamSetup setup;
  if (payload.contains("setup_id")) setup.id = payload["setup_id"].get<std::string>();
  if (payload.contains("stock")) setup.stock = stock_from_payload(payload["stock"]);
  if (payload.contains("wcs_origin")) {
    const auto& wcs = payload["wcs_origin"];
    if (wcs.contains("x")) setup.wcsOriginX = wcs["x"].get<double>();
    if (wcs.contains("y")) setup.wcsOriginY = wcs["y"].get<double>();
    if (wcs.contains("z")) setup.wcsOriginZ = wcs["z"].get<double>();
  }
  if (payload.contains("safety_plane_z")) setup.safetyPlaneZ = payload["safety_plane_z"].get<double>();
  if (payload.contains("axis_count")) setup.axisCount = payload["axis_count"].get<int>();
  if (payload.contains("wcs_angle")) setup.wcsAngle = payload["wcs_angle"].get<double>();
  return setup;
}

polysmith::core::CamToolDefinition tool_from_payload(const json& payload) {
  polysmith::core::CamToolDefinition tool;
  if (payload.contains("tool_id")) tool.id = payload["tool_id"].get<std::string>();
  if (payload.contains("name")) tool.name = payload["name"].get<std::string>();
  if (payload.contains("tool_number")) tool.toolNumber = payload["tool_number"].get<int>();
  if (payload.contains("diameter")) tool.diameter = payload["diameter"].get<double>();
  if (payload.contains("flute_length")) tool.fluteLength = payload["flute_length"].get<double>();
  if (payload.contains("overall_length")) tool.overallLength = payload["overall_length"].get<double>();
  if (payload.contains("corner_radius")) tool.cornerRadius = payload["corner_radius"].get<double>();
  if (payload.contains("spindle_speed")) tool.spindleSpeed = payload["spindle_speed"].get<double>();
  if (payload.contains("feed_rate")) tool.feedRate = payload["feed_rate"].get<double>();
  if (payload.contains("stepover")) tool.stepover = payload["stepover"].get<double>();
  if (payload.contains("stepdown")) tool.stepdown = payload["stepdown"].get<double>();
  if (payload.contains("type")) {
    tool.type = static_cast<polysmith::core::CamToolDefinition::Type>(payload["type"].get<int>());
  }
  return tool;
}
