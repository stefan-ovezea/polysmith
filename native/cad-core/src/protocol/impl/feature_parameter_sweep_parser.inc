polysmith::core::SweepFeatureParameters
sweep_parameters_from_payload(const json& payload) {
  polysmith::core::SweepFeatureParameters params{};
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
  params.path_sketch_feature_id =
      read_string(payload, "path_sketch_feature_id");
  params.path_entity_id = read_string(payload, "path_entity_id");
  params.path_start_x = read_number(payload, "path_start_x");
  params.path_start_y = read_number(payload, "path_start_y");
  params.path_start_z = read_number(payload, "path_start_z");
  params.path_end_x = read_number(payload, "path_end_x");
  params.path_end_y = read_number(payload, "path_end_y");
  params.path_end_z = read_number(payload, "path_end_z");
  if (payload.contains("path_segments") &&
      payload.at("path_segments").is_array()) {
    for (const auto& segment_payload : payload.at("path_segments")) {
      polysmith::core::SweepFeatureParameters::PathSegment segment{};
      segment.entity_id = read_string(segment_payload, "entity_id");
      segment.kind = read_string(segment_payload, "kind");
      segment.start_x = read_number(segment_payload, "start_x");
      segment.start_y = read_number(segment_payload, "start_y");
      segment.start_z = read_number(segment_payload, "start_z");
      segment.end_x = read_number(segment_payload, "end_x");
      segment.end_y = read_number(segment_payload, "end_y");
      segment.end_z = read_number(segment_payload, "end_z");
      segment.center_x = read_number(segment_payload, "center_x");
      segment.center_y = read_number(segment_payload, "center_y");
      segment.center_z = read_number(segment_payload, "center_z");
      segment.mid_x = read_number(segment_payload, "mid_x");
      segment.mid_y = read_number(segment_payload, "mid_y");
      segment.mid_z = read_number(segment_payload, "mid_z");
      segment.radius = read_number(segment_payload, "radius");
      segment.ccw = segment_payload.contains("ccw") &&
                    segment_payload.at("ccw").is_boolean()
                        ? segment_payload.at("ccw").get<bool>()
                        : true;
      params.path_segments.push_back(segment);
    }
  }
  if (params.path_segments.empty()) {
    params.path_segments.push_back(
        polysmith::core::SweepFeatureParameters::PathSegment{
            .entity_id = params.path_entity_id,
            .kind = "line",
            .start_x = params.path_start_x,
            .start_y = params.path_start_y,
            .start_z = params.path_start_z,
            .end_x = params.path_end_x,
            .end_y = params.path_end_y,
            .end_z = params.path_end_z,
        });
  }
  return params;
}
