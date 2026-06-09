  if (payload.contains("plugin_feature_parameters") &&
      !payload.at("plugin_feature_parameters").is_null()) {
    const json& plugin_payload = payload.at("plugin_feature_parameters");
    polysmith::core::PluginFeatureParameters parameters{};
    parameters.plugin_id = read_string(plugin_payload, "plugin_id");
    parameters.feature_type = read_string(plugin_payload, "feature_type");
    if (plugin_payload.contains("schema_version") &&
        !plugin_payload.at("schema_version").is_null()) {
      parameters.schema_version = read_int(plugin_payload, "schema_version");
    }
    if (plugin_payload.contains("is_pending") &&
        plugin_payload.at("is_pending").is_boolean()) {
      parameters.is_pending = read_bool(plugin_payload, "is_pending");
    }
    parameters.display_name =
        read_optional_string_value(plugin_payload,
                                   "display_name",
                                   parameters.display_name);
    parameters.parameters_summary =
        read_optional_string_value(plugin_payload,
                                   "parameters_summary",
                                   parameters.parameters_summary);
    if (plugin_payload.contains("parameters") &&
        !plugin_payload.at("parameters").is_null()) {
      parameters.parameters_json = plugin_payload.at("parameters").dump();
    }
    if (plugin_payload.contains("geometry") &&
        plugin_payload.at("geometry").is_array()) {
      for (const auto& operation_payload : plugin_payload.at("geometry")) {
        polysmith::core::PluginGeometryOperation operation{};
        operation.operation =
            read_optional_string_value(operation_payload,
                                       "operation",
                                       operation.operation);
        operation.primitive =
            read_optional_string_value(operation_payload,
                                       "primitive",
                                       operation.primitive);
        operation.x = read_optional_number(operation_payload,
                                           "x",
                                           operation.x);
        operation.y = read_optional_number(operation_payload,
                                           "y",
                                           operation.y);
        operation.z = read_optional_number(operation_payload,
                                           "z",
                                           operation.z);
        operation.width = read_optional_number(operation_payload,
                                               "width",
                                               operation.width);
        operation.depth = read_optional_number(operation_payload,
                                               "depth",
                                               operation.depth);
        operation.height = read_optional_number(operation_payload,
                                                "height",
                                                operation.height);
        operation.radius = read_optional_number(operation_payload,
                                                "radius",
                                                operation.radius);
        operation.top_width = read_optional_number(operation_payload,
                                                   "top_width",
                                                   operation.top_width);
        operation.top_depth = read_optional_number(operation_payload,
                                                   "top_depth",
                                                   operation.top_depth);
        operation.top_radius = read_optional_number(operation_payload,
                                                    "top_radius",
                                                    operation.top_radius);
        parameters.geometry.push_back(operation);
      }
    }
    feature.plugin_parameters = parameters;
  }
