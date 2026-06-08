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
