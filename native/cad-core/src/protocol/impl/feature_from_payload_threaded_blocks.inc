  if (payload.contains("hole_parameters") &&
      !payload.at("hole_parameters").is_null()) {
    const json& hp = payload.at("hole_parameters");
    polysmith::core::HoleFeatureParameters params{};
    params.target_body_id = read_string(hp, "target_body_id");
    params.source_face_id = read_string(hp, "source_face_id");
    params.plane_frame = plane_frame_from_payload(require(hp, "plane_frame"));
    params.center_x = read_number(hp, "center_x");
    params.center_y = read_number(hp, "center_y");
    params.hole_type = read_optional_string_value(hp, "hole_type", "simple");
    params.extent_type = read_optional_string_value(hp, "extent_type", "blind");
    params.diameter = read_optional_number(hp, "diameter", params.diameter);
    params.depth = read_optional_number(hp, "depth", params.depth);
    params.counterbore_diameter = read_optional_number(
        hp, "counterbore_diameter", params.counterbore_diameter);
    params.counterbore_depth = read_optional_number(
        hp, "counterbore_depth", params.counterbore_depth);
    params.countersink_diameter = read_optional_number(
        hp, "countersink_diameter", params.countersink_diameter);
    params.countersink_angle_degrees = read_optional_number(
        hp, "countersink_angle_degrees", params.countersink_angle_degrees);
    params.standard =
        read_optional_string_value(hp, "standard", params.standard);
    params.standard_size =
        read_optional_string_value(hp, "standard_size", params.standard_size);
    params.hole_fit =
        read_optional_string_value(hp, "hole_fit", params.hole_fit);
    if (hp.contains("thread_enabled") && hp.at("thread_enabled").is_boolean()) {
      params.thread_enabled = hp.at("thread_enabled").get<bool>();
    }
    params.thread_spec =
        read_optional_string_value(hp, "thread_spec", params.thread_spec);
    params.thread_pitch =
        read_optional_number(hp, "thread_pitch", params.thread_pitch);
    params.major_diameter =
        read_optional_number(hp, "major_diameter", params.major_diameter);
    params.minor_diameter =
        read_optional_number(hp, "minor_diameter", params.minor_diameter);
    params.thread_depth =
        read_optional_number(hp, "thread_depth", params.thread_depth);
    params.thread_representation = read_optional_string_value(
        hp, "thread_representation", params.thread_representation);
    if (hp.contains("is_pending") && hp.at("is_pending").is_boolean()) {
      params.is_pending = hp.at("is_pending").get<bool>();
    }
    feature.hole_parameters = params;
  }

  if (payload.contains("helix_parameters") &&
      !payload.at("helix_parameters").is_null()) {
    const json& hp = payload.at("helix_parameters");
    polysmith::core::HelixFeatureParameters params{};
    params.axis_source_id = read_string(hp, "axis_source_id");
    params.axis_start_x = require(hp, "axis_start").at("x").get<double>();
    params.axis_start_y = require(hp, "axis_start").at("y").get<double>();
    params.axis_start_z = require(hp, "axis_start").at("z").get<double>();
    params.axis_end_x = require(hp, "axis_end").at("x").get<double>();
    params.axis_end_y = require(hp, "axis_end").at("y").get<double>();
    params.axis_end_z = require(hp, "axis_end").at("z").get<double>();
    params.radius = read_optional_number(hp, "radius", params.radius);
    params.pitch = read_optional_number(hp, "pitch", params.pitch);
    params.height = read_optional_number(hp, "height", params.height);
    params.turns = read_optional_number(hp, "turns", params.turns);
    params.handedness =
        read_optional_string_value(hp, "handedness", params.handedness);
    params.start_angle_degrees = read_optional_number(
        hp, "start_angle_degrees", params.start_angle_degrees);
    if (hp.contains("points") && hp.at("points").is_array()) {
      for (const auto& point_value : hp.at("points")) {
        if (point_value.is_number()) {
          params.points.push_back(point_value.get<double>());
        }
      }
    }
    feature.helix_parameters = params;
  }

  if (payload.contains("thread_parameters") &&
      !payload.at("thread_parameters").is_null()) {
    const json& tp = payload.at("thread_parameters");
    polysmith::core::ThreadFeatureParameters params{};
    params.target_body_id =
        read_optional_string_value(tp, "target_body_id", params.target_body_id);
    params.axis_source_id =
        read_optional_string_value(tp, "axis_source_id", params.axis_source_id);
    params.mode = read_optional_string_value(tp, "mode", params.mode);
    params.standard =
        read_optional_string_value(tp, "standard", params.standard);
    params.size = read_optional_string_value(tp, "size", params.size);
    params.major_diameter =
        read_optional_number(tp, "major_diameter", params.major_diameter);
    params.minor_diameter =
        read_optional_number(tp, "minor_diameter", params.minor_diameter);
    params.pitch = read_optional_number(tp, "pitch", params.pitch);
    params.length = read_optional_number(tp, "length", params.length);
    params.thread_angle_degrees = read_optional_number(
        tp, "thread_angle_degrees", params.thread_angle_degrees);
    params.start_offset =
        read_optional_number(tp, "start_offset", params.start_offset);
    params.handedness =
        read_optional_string_value(tp, "handedness", params.handedness);
    params.representation =
        read_optional_string_value(tp, "representation", params.representation);
    if (tp.contains("is_pending") && tp.at("is_pending").is_boolean()) {
      params.is_pending = tp.at("is_pending").get<bool>();
    }
    feature.thread_parameters = params;
  }

  if (payload.contains("fastener_parameters") &&
      !payload.at("fastener_parameters").is_null()) {
    const json& fp = payload.at("fastener_parameters");
    polysmith::core::FastenerFeatureParameters params{};
    params.standard =
        read_optional_string_value(fp, "standard", params.standard);
    params.size = read_optional_string_value(fp, "size", params.size);
    params.diameter = read_optional_number(fp, "diameter", params.diameter);
    params.minor_diameter =
        read_optional_number(fp, "minor_diameter", params.minor_diameter);
    params.pitch = read_optional_number(fp, "pitch", params.pitch);
    params.length = read_optional_number(fp, "length", params.length);
    params.thread_length =
        read_optional_number(fp, "thread_length", params.thread_length);
    params.head_type =
        read_optional_string_value(fp, "head_type", params.head_type);
    params.drive_type =
        read_optional_string_value(fp, "drive_type", params.drive_type);
    params.thread_representation = read_optional_string_value(
        fp, "thread_representation", params.thread_representation);
    feature.fastener_parameters = params;
  }
