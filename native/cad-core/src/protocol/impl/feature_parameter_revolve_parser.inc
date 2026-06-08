polysmith::core::RevolveFeatureParameters
revolve_parameters_from_payload(const json& payload) {
  polysmith::core::RevolveFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.profile_kind = read_string(payload, "profile_kind");
  if (payload.contains("profile_points") &&
      payload.at("profile_points").is_array()) {
    for (const auto& point_payload : payload.at("profile_points")) {
      params.profile_points.push_back(polysmith::core::SketchProfilePoint{
          .x = point_payload.at("x").get<double>(),
          .y = point_payload.at("y").get<double>(),
      });
    }
  }
  if (payload.contains("inner_loops") && payload.at("inner_loops").is_array()) {
    for (const auto& loop_payload : payload.at("inner_loops")) {
      std::vector<polysmith::core::SketchProfilePoint> loop;
      for (const auto& point_payload : loop_payload) {
        loop.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.inner_loops.push_back(std::move(loop));
    }
  }
  params.axis_sketch_feature_id =
      read_string(payload, "axis_sketch_feature_id");
  params.axis_entity_id = read_string(payload, "axis_entity_id");
  params.axis_start_x = read_number(payload, "axis_start_x");
  params.axis_start_y = read_number(payload, "axis_start_y");
  params.axis_start_z = read_number(payload, "axis_start_z");
  params.axis_end_x = read_number(payload, "axis_end_x");
  params.axis_end_y = read_number(payload, "axis_end_y");
  params.axis_end_z = read_number(payload, "axis_end_z");
  params.angle_degrees = read_number(payload, "angle_degrees");
  return params;
}
