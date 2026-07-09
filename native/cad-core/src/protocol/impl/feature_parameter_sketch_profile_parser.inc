  if (payload.contains("profiles") && payload.at("profiles").is_array()) {
    for (const auto& profile_payload : payload.at("profiles")) {
      polysmith::core::SketchProfileRegion profile{};
      profile.id = read_string(profile_payload, "profile_id");
      profile.kind = read_string(profile_payload, "kind");
      if (profile_payload.contains("vertex_ids") &&
          profile_payload.at("vertex_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("vertex_ids")) {
          profile.vertex_ids.push_back(id_value.get<std::string>());
        }
      }
      if (profile_payload.contains("line_ids") &&
          profile_payload.at("line_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("line_ids")) {
          profile.line_ids.push_back(id_value.get<std::string>());
        }
      }
      if (profile_payload.contains("points") &&
          profile_payload.at("points").is_array()) {
        for (const auto& pt_payload : profile_payload.at("points")) {
          profile.points.push_back(polysmith::core::SketchProfilePoint{
              .x = pt_payload.at("x").get<double>(),
              .y = pt_payload.at("y").get<double>(),
          });
        }
      }
      if (profile_payload.contains("inner_loops") &&
          profile_payload.at("inner_loops").is_array()) {
        for (const auto& loop_payload : profile_payload.at("inner_loops")) {
          std::vector<polysmith::core::SketchProfilePoint> loop;
          for (const auto& pt_payload : loop_payload) {
            loop.push_back(polysmith::core::SketchProfilePoint{
                .x = pt_payload.at("x").get<double>(),
                .y = pt_payload.at("y").get<double>(),
            });
          }
          profile.inner_loops.push_back(std::move(loop));
        }
      }
      profile.source_circle_id =
          read_optional_string(profile_payload, "source_circle_id");
      profile.center_x = read_number(profile_payload, "center_x");
      profile.center_y = read_number(profile_payload, "center_y");
      profile.radius = read_number(profile_payload, "radius");
      params.profiles.push_back(profile);
    }
  }
