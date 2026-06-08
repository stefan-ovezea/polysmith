  if (payload.contains("construction_plane_parameters") &&
      !payload.at("construction_plane_parameters").is_null()) {
    const json& cpp = payload.at("construction_plane_parameters");
    polysmith::core::ConstructionPlaneFeatureParameters params{};
    if (cpp.contains("plane_type") && cpp.at("plane_type").is_string()) {
      params.plane_type = cpp.at("plane_type").get<std::string>();
    }
    params.source_plane_id = read_string(cpp, "source_plane_id");
    if (cpp.contains("source_plane_ids") && cpp.at("source_plane_ids").is_array()) {
      for (const auto& id : cpp.at("source_plane_ids")) {
        if (id.is_string()) {
          params.source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (params.source_plane_ids.empty() && !params.source_plane_id.empty()) {
      params.source_plane_ids.push_back(params.source_plane_id);
    }
    if (cpp.contains("source_axis_id") && cpp.at("source_axis_id").is_string()) {
      params.source_axis_id = cpp.at("source_axis_id").get<std::string>();
    }
    params.offset = read_number(cpp, "offset");
    if (cpp.contains("angle_degrees") && cpp.at("angle_degrees").is_number()) {
      params.angle_degrees = cpp.at("angle_degrees").get<double>();
    }
    if (cpp.contains("plane_frame") && cpp.at("plane_frame").is_object()) {
      params.plane_frame = plane_frame_from_payload(cpp.at("plane_frame"));
    }
    feature.construction_plane_parameters = params;
  }

  if (payload.contains("construction_axis_parameters") &&
      !payload.at("construction_axis_parameters").is_null()) {
    const json& cap = payload.at("construction_axis_parameters");
    polysmith::core::ConstructionAxisFeatureParameters params{};
    params.source_id = read_string(cap, "source_id");
    params.source_kind =
        read_optional_string_value(cap, "source_kind", "edge");
    params.start_x = require(cap, "start").at("x").get<double>();
    params.start_y = require(cap, "start").at("y").get<double>();
    params.start_z = require(cap, "start").at("z").get<double>();
    params.end_x = require(cap, "end").at("x").get<double>();
    params.end_y = require(cap, "end").at("y").get<double>();
    params.end_z = require(cap, "end").at("z").get<double>();
    feature.construction_axis_parameters = params;
  }

  if (payload.contains("construction_point_parameters") &&
      !payload.at("construction_point_parameters").is_null()) {
    const json& cpp = payload.at("construction_point_parameters");
    polysmith::core::ConstructionPointFeatureParameters params{};
    params.source_id = read_string(cpp, "source_id");
    params.source_kind =
        read_optional_string_value(cpp, "source_kind", "vertex");
    params.x = require(cpp, "position").at("x").get<double>();
    params.y = require(cpp, "position").at("y").get<double>();
    params.z = require(cpp, "position").at("z").get<double>();
    feature.construction_point_parameters = params;
  }
