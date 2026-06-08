ExtrudeFeatureParameters::SideParameters read_extrude_side(
    const polysmith::protocol::json& payload,
    const char* key,
    const ExtrudeFeatureParameters::SideParameters& fallback) {
  ExtrudeFeatureParameters::SideParameters side = fallback;
  if (!payload.contains(key) || !payload.at(key).is_object()) {
    return side;
  }
  const auto& side_payload = payload.at(key);
  side.extent_type =
      read_optional_string(side_payload, "extent_type").value_or(side.extent_type);
  side.distance =
      read_optional_dimension(side_payload, "distance", side.distance);
  side.start_offset =
      read_optional_dimension(side_payload, "start_offset", side.start_offset);
  side.taper_angle_degrees =
      read_optional_dimension(side_payload,
                              "taper_angle_degrees",
                              side.taper_angle_degrees);
  side.target_reference_id =
      read_optional_string(side_payload, "target_reference_id");
  return side;
}

std::optional<ExtrudeFeatureParameters> read_optional_extrude_parameters(
    const polysmith::protocol::json& payload,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id) {
  const bool has_parameter_fields =
      payload.contains("extent_mode") || payload.contains("side1") ||
      payload.contains("side2") || payload.contains("thin") ||
      payload.contains("operation") || payload.contains("intersect_result");
  if (!has_parameter_fields && !payload.contains("parameters")) {
    return std::nullopt;
  }

  if (payload.contains("parameters") && payload.at("parameters").is_object() &&
      payload.at("parameters").contains("sketch_feature_id")) {
    return polysmith::protocol::extrude_parameters_from_payload(
        payload.at("parameters"));
  }

  const auto& parameter_payload =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  ExtrudeFeatureParameters params{};
  params.depth = depth;
  params.mode = mode;
  params.operation =
      read_optional_string(parameter_payload, "operation")
          .value_or(mode.empty() ? "new_body" : mode);
  params.target_body_id = target_body_id;
  params.extent_mode =
      read_optional_string(parameter_payload, "extent_mode").value_or("one_side");
  params.side1.distance = std::abs(depth);
  params.side1 = read_extrude_side(parameter_payload, "side1", params.side1);
  if (parameter_payload.contains("side2") &&
      parameter_payload.at("side2").is_object()) {
    params.side2 =
        read_extrude_side(parameter_payload, "side2", params.side1);
  }
  if (parameter_payload.contains("thin") &&
      parameter_payload.at("thin").is_object()) {
    const auto& thin = parameter_payload.at("thin");
    if (thin.contains("enabled") && thin.at("enabled").is_boolean()) {
      params.thin.enabled = thin.at("enabled").get<bool>();
    }
    params.thin.thickness =
        read_optional_dimension(thin, "thickness", params.thin.thickness);
    params.thin.placement =
        read_optional_string(thin, "placement").value_or(params.thin.placement);
  }
  params.intersect_result =
      read_optional_string(parameter_payload, "intersect_result")
          .value_or(params.intersect_result);
  return params;
}
