std::string read_optional_string_value(
    const polysmith::protocol::json& payload,
    const char* key,
    const std::string& fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload field is not string '") +
                             key + "'");
  }
  return payload.at(key).get<std::string>();
}

double read_optional_number_value(
    const polysmith::protocol::json& payload,
    const char* key,
    double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Command payload field is not numeric '") +
                             key + "'");
  }
  return payload.at(key).get<double>();
}

int read_optional_int_value(const polysmith::protocol::json& payload,
                            const char* key,
                            int fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number_integer()) {
    throw std::runtime_error(std::string("Command payload field is not integer '") +
                             key + "'");
  }
  return payload.at(key).get<int>();
}

std::vector<polysmith::core::PluginProfilePoint> read_optional_plugin_profile_points(
    const polysmith::protocol::json& payload,
    const char* key) {
  std::vector<polysmith::core::PluginProfilePoint> points;
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return points;
  }
  if (!payload.at(key).is_array()) {
    throw std::runtime_error(
        std::string("Command payload field is not array '") + key + "'");
  }
  for (const auto& point_payload : payload.at(key)) {
    if (!point_payload.is_object()) {
      throw std::runtime_error("Plugin profile point must be an object");
    }
    points.push_back(polysmith::core::PluginProfilePoint{
        .u = read_optional_number_value(point_payload, "u", 0.0),
        .v = read_optional_number_value(point_payload, "v", 0.0),
    });
  }
  return points;
}

PluginGeometryOperation read_plugin_geometry_operation(
    const polysmith::protocol::json& payload) {
  if (!payload.is_object()) {
    throw std::runtime_error("Plugin geometry operation must be an object");
  }

  PluginGeometryOperation operation{};
  operation.operation = read_optional_string_value(payload, "operation", "add");
  operation.primitive = read_optional_string_value(payload, "primitive", "box");
  operation.x = read_optional_number_value(payload, "x", operation.x);
  operation.y = read_optional_number_value(payload, "y", operation.y);
  operation.z = read_optional_number_value(payload, "z", operation.z);
  operation.width = read_optional_number_value(payload, "width", operation.width);
  operation.depth = read_optional_number_value(payload, "depth", operation.depth);
  operation.height = read_optional_number_value(payload, "height", operation.height);
  operation.radius = read_optional_number_value(payload, "radius", operation.radius);
  operation.top_width =
      read_optional_number_value(payload, "top_width", operation.top_width);
  operation.top_depth =
      read_optional_number_value(payload, "top_depth", operation.top_depth);
  operation.top_radius =
      read_optional_number_value(payload, "top_radius", operation.top_radius);
  operation.top_offset_x =
      read_optional_number_value(payload, "top_offset_x", operation.top_offset_x);
  operation.top_offset_y =
      read_optional_number_value(payload, "top_offset_y", operation.top_offset_y);
  operation.profile_plane =
      read_optional_string_value(payload, "profile_plane", operation.profile_plane);
  operation.extrude_x =
      read_optional_number_value(payload, "extrude_x", operation.extrude_x);
  operation.extrude_y =
      read_optional_number_value(payload, "extrude_y", operation.extrude_y);
  operation.extrude_z =
      read_optional_number_value(payload, "extrude_z", operation.extrude_z);
  operation.path_width =
      read_optional_number_value(payload, "path_width", operation.path_width);
  operation.path_depth =
      read_optional_number_value(payload, "path_depth", operation.path_depth);
  operation.path_radius =
      read_optional_number_value(payload, "path_radius", operation.path_radius);
  operation.profile_points =
      read_optional_plugin_profile_points(payload, "profile_points");
  return operation;
}

PluginFeatureParameters read_plugin_feature_parameters(
    const polysmith::protocol::json& payload) {
  PluginFeatureParameters parameters{};
  parameters.plugin_id = read_string(payload, "plugin_id");
  parameters.feature_type = read_string(payload, "feature_type");
  parameters.schema_version =
      read_optional_int_value(payload, "schema_version", parameters.schema_version);
  parameters.display_name =
      read_optional_string_value(payload, "display_name", parameters.display_name);
  parameters.parameters_summary =
      read_optional_string_value(payload, "parameters_summary", parameters.parameters_summary);

  if (payload.contains("parameters") && !payload.at("parameters").is_null()) {
    parameters.parameters_json = payload.at("parameters").dump();
  }

  if (!payload.contains("geometry") || !payload.at("geometry").is_array()) {
    throw std::runtime_error("Plugin feature command is missing geometry");
  }
  for (const auto& operation_payload : payload.at("geometry")) {
    parameters.geometry.push_back(
        read_plugin_geometry_operation(operation_payload));
  }
  return parameters;
}
