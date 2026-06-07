DocumentManager& document_manager() {
  static DocumentManager manager;
  return manager;
}

double read_dimension(const polysmith::protocol::json& payload,
                      const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Command payload is missing numeric field '") +
                             key + "'");
  }

  return payload.at(key).get<double>();
}

int read_int(const polysmith::protocol::json& payload, const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number_integer()) {
    throw std::runtime_error(std::string("Command payload is missing integer field '") +
                             key + "'");
  }

  return payload.at(key).get<int>();
}

std::string read_string(const polysmith::protocol::json& payload,
                        const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload is missing string field '") +
                             key + "'");
  }

  return payload.at(key).get<std::string>();
}

std::optional<std::string> read_optional_string(
    const polysmith::protocol::json& payload,
    const char* key) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return std::nullopt;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload field is not a string '") +
                             key + "'");
  }
  return payload.at(key).get<std::string>();
}

double read_optional_dimension(const polysmith::protocol::json& payload,
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

HoleFeatureParameters read_hole_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  HoleFeatureParameters params{};
  params.hole_type =
      read_optional_string(source, "hole_type").value_or(params.hole_type);
  params.extent_type =
      read_optional_string(source, "extent_type").value_or(params.extent_type);
  params.diameter =
      read_optional_dimension(source, "diameter", params.diameter);
  params.depth = read_optional_dimension(source, "depth", params.depth);
  params.counterbore_diameter = read_optional_dimension(
      source, "counterbore_diameter", params.counterbore_diameter);
  params.counterbore_depth = read_optional_dimension(
      source, "counterbore_depth", params.counterbore_depth);
  params.countersink_diameter = read_optional_dimension(
      source, "countersink_diameter", params.countersink_diameter);
  params.countersink_angle_degrees = read_optional_dimension(
      source, "countersink_angle_degrees", params.countersink_angle_degrees);
  params.standard =
      read_optional_string(source, "standard").value_or(params.standard);
  params.standard_size =
      read_optional_string(source, "standard_size").value_or(params.standard_size);
  params.hole_fit =
      read_optional_string(source, "hole_fit").value_or(params.hole_fit);
  if (source.contains("thread_enabled") && source.at("thread_enabled").is_boolean()) {
    params.thread_enabled = source.at("thread_enabled").get<bool>();
  }
  params.thread_spec =
      read_optional_string(source, "thread_spec").value_or(params.thread_spec);
  params.thread_pitch =
      read_optional_dimension(source, "thread_pitch", params.thread_pitch);
  params.major_diameter =
      read_optional_dimension(source, "major_diameter", params.major_diameter);
  params.minor_diameter =
      read_optional_dimension(source, "minor_diameter", params.minor_diameter);
  params.thread_depth =
      read_optional_dimension(source, "thread_depth", params.thread_depth);
  params.thread_representation =
      read_optional_string(source, "thread_representation")
          .value_or(params.thread_representation);
  return params;
}

HelixFeatureParameters read_helix_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  HelixFeatureParameters params{};
  params.radius = read_optional_dimension(source, "radius", params.radius);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.height = read_optional_dimension(source, "height", params.height);
  params.handedness =
      read_optional_string(source, "handedness").value_or(params.handedness);
  params.start_angle_degrees = read_optional_dimension(
      source, "start_angle_degrees", params.start_angle_degrees);
  return params;
}

ThreadFeatureParameters read_thread_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  ThreadFeatureParameters params{};
  params.target_body_id =
      read_optional_string(source, "target_body_id").value_or(params.target_body_id);
  params.axis_source_id =
      read_optional_string(source, "axis_source_id").value_or(params.axis_source_id);
  params.mode = read_optional_string(source, "mode").value_or(params.mode);
  params.standard =
      read_optional_string(source, "standard").value_or(params.standard);
  params.size = read_optional_string(source, "size").value_or(params.size);
  params.major_diameter =
      read_optional_dimension(source, "major_diameter", params.major_diameter);
  params.minor_diameter =
      read_optional_dimension(source, "minor_diameter", params.minor_diameter);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.length = read_optional_dimension(source, "length", params.length);
  params.thread_angle_degrees =
      read_optional_dimension(source, "thread_angle_degrees", params.thread_angle_degrees);
  params.start_offset =
      read_optional_dimension(source, "start_offset", params.start_offset);
  params.handedness =
      read_optional_string(source, "handedness").value_or(params.handedness);
  params.representation =
      read_optional_string(source, "representation").value_or(params.representation);
  return params;
}

FastenerFeatureParameters read_fastener_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  FastenerFeatureParameters params{};
  params.standard =
      read_optional_string(source, "standard").value_or(params.standard);
  params.size = read_optional_string(source, "size").value_or(params.size);
  params.diameter =
      read_optional_dimension(source, "diameter", params.diameter);
  params.minor_diameter =
      read_optional_dimension(source, "minor_diameter", params.minor_diameter);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.length = read_optional_dimension(source, "length", params.length);
  params.thread_length =
      read_optional_dimension(source, "thread_length", params.thread_length);
  params.head_type =
      read_optional_string(source, "head_type").value_or(params.head_type);
  params.drive_type =
      read_optional_string(source, "drive_type").value_or(params.drive_type);
  params.thread_representation =
      read_optional_string(source, "thread_representation")
          .value_or(params.thread_representation);
  return params;
}

MoveFeatureParameters read_move_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  MoveFeatureParameters params{};
  params.target_body_id =
      read_optional_string(source, "target_body_id").value_or(params.target_body_id);
  params.translation_x =
      read_optional_dimension(source, "translation_x", params.translation_x);
  params.translation_y =
      read_optional_dimension(source, "translation_y", params.translation_y);
  params.translation_z =
      read_optional_dimension(source, "translation_z", params.translation_z);
  params.rotation_x_degrees =
      read_optional_dimension(source, "rotation_x_degrees", params.rotation_x_degrees);
  params.rotation_y_degrees =
      read_optional_dimension(source, "rotation_y_degrees", params.rotation_y_degrees);
  params.rotation_z_degrees =
      read_optional_dimension(source, "rotation_z_degrees", params.rotation_z_degrees);
  if (source.contains("is_pending") && source.at("is_pending").is_boolean()) {
    params.is_pending = source.at("is_pending").get<bool>();
  }
  return params;
}

polysmith::core::SketchFeatureParameters::SketchPlaneFrame read_plane_frame(
    const polysmith::protocol::json& payload,
    const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_object()) {
    throw std::runtime_error(std::string("Command payload is missing object field '") +
                             key + "'");
  }

  const auto& frame = payload.at(key);
  const auto read_vector = [&](const char* vector_key) {
    if (!frame.contains(vector_key) || !frame.at(vector_key).is_object()) {
      throw std::runtime_error(std::string("Command payload is missing plane frame field '") +
                               vector_key + "'");
    }

    const auto& vector = frame.at(vector_key);
    return polysmith::protocol::json{
        {"x", vector.at("x").get<double>()},
        {"y", vector.at("y").get<double>()},
        {"z", vector.at("z").get<double>()},
    };
  };

  const auto origin = read_vector("origin");
  const auto x_axis = read_vector("x_axis");
  const auto y_axis = read_vector("y_axis");
  const auto normal = read_vector("normal");

  return polysmith::core::SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = origin.at("x").get<double>(),
      .origin_y = origin.at("y").get<double>(),
      .origin_z = origin.at("z").get<double>(),
      .x_axis_x = x_axis.at("x").get<double>(),
      .x_axis_y = x_axis.at("y").get<double>(),
      .x_axis_z = x_axis.at("z").get<double>(),
      .y_axis_x = y_axis.at("x").get<double>(),
      .y_axis_y = y_axis.at("y").get<double>(),
      .y_axis_z = y_axis.at("z").get<double>(),
      .normal_x = normal.at("x").get<double>(),
      .normal_y = normal.at("y").get<double>(),
      .normal_z = normal.at("z").get<double>(),
  };
}
