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
