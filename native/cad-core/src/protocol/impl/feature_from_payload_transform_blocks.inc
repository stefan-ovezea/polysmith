  if (payload.contains("move_parameters") &&
      !payload.at("move_parameters").is_null()) {
    const json& mp = payload.at("move_parameters");
    polysmith::core::MoveFeatureParameters params{};
    params.target_body_id =
        read_optional_string_value(mp, "target_body_id", params.target_body_id);
    params.translation_x =
        read_optional_number(mp, "translation_x", params.translation_x);
    params.translation_y =
        read_optional_number(mp, "translation_y", params.translation_y);
    params.translation_z =
        read_optional_number(mp, "translation_z", params.translation_z);
    params.rotation_x_degrees =
        read_optional_number(mp, "rotation_x_degrees", params.rotation_x_degrees);
    params.rotation_y_degrees =
        read_optional_number(mp, "rotation_y_degrees", params.rotation_y_degrees);
    params.rotation_z_degrees =
        read_optional_number(mp, "rotation_z_degrees", params.rotation_z_degrees);
    if (mp.contains("is_pending") && mp.at("is_pending").is_boolean()) {
      params.is_pending = mp.at("is_pending").get<bool>();
    }
    feature.move_parameters = params;
  }

  if (payload.contains("body_copy_parameters") &&
      !payload.at("body_copy_parameters").is_null()) {
    const json& bp = payload.at("body_copy_parameters");
    polysmith::core::BodyCopyFeatureParameters params{};
    params.source_body_id =
        read_optional_string_value(bp, "source_body_id", params.source_body_id);
    params.copy_mode =
        read_optional_string_value(bp, "copy_mode", params.copy_mode);
    params.source_body_name =
        read_optional_string_value(bp, "source_body_name", params.source_body_name);
    params.serialized_shape =
        read_optional_string_value(bp, "serialized_shape", params.serialized_shape);
    params.local_x_axis_x =
        read_optional_number(bp, "local_x_axis_x", params.local_x_axis_x);
    params.local_x_axis_y =
        read_optional_number(bp, "local_x_axis_y", params.local_x_axis_y);
    params.local_x_axis_z =
        read_optional_number(bp, "local_x_axis_z", params.local_x_axis_z);
    params.local_y_axis_x =
        read_optional_number(bp, "local_y_axis_x", params.local_y_axis_x);
    params.local_y_axis_y =
        read_optional_number(bp, "local_y_axis_y", params.local_y_axis_y);
    params.local_y_axis_z =
        read_optional_number(bp, "local_y_axis_z", params.local_y_axis_z);
    params.local_z_axis_x =
        read_optional_number(bp, "local_z_axis_x", params.local_z_axis_x);
    params.local_z_axis_y =
        read_optional_number(bp, "local_z_axis_y", params.local_z_axis_y);
    params.local_z_axis_z =
        read_optional_number(bp, "local_z_axis_z", params.local_z_axis_z);
    feature.body_copy_parameters = params;
  }
