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
      if (profile_payload.contains("ordered_edge_ids") &&
          profile_payload.at("ordered_edge_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("ordered_edge_ids")) {
          profile.ordered_edge_ids.push_back(id_value.get<std::string>());
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
      if (profile_payload.contains("boundary_edges") &&
          profile_payload.at("boundary_edges").is_array()) {
        for (const auto& edge_payload : profile_payload.at("boundary_edges")) {
          profile.boundary_edges.push_back(
              polysmith::core::ProfileBoundaryEdge{
                  .entity_id =
                      read_string(edge_payload, "entity_id"),
                  .entity_kind =
                      read_string(edge_payload, "entity_kind"),
                  .param_start = read_number(edge_payload, "param_start"),
                  .param_end = read_number(edge_payload, "param_end"),
                  .start_x = read_number(edge_payload, "start_x"),
                  .start_y = read_number(edge_payload, "start_y"),
                  .end_x = read_number(edge_payload, "end_x"),
                  .end_y = read_number(edge_payload, "end_y"),
                  .center_x = read_number(edge_payload, "center_x"),
                  .center_y = read_number(edge_payload, "center_y"),
                  .radius = read_number(edge_payload, "radius"),
                  .ccw = read_bool(edge_payload, "ccw"),
              });
        }
      }
      if (profile_payload.contains("arc_descriptors") &&
          profile_payload.at("arc_descriptors").is_array()) {
        for (const auto& desc_payload : profile_payload.at("arc_descriptors")) {
          profile.arc_descriptors.push_back(
              polysmith::core::SketchArcDescriptor{
                  .circle_id = desc_payload.at("circle_id").get<std::string>(),
                  .center_x = desc_payload.at("center_x").get<double>(),
                  .center_y = desc_payload.at("center_y").get<double>(),
                  .radius = desc_payload.at("radius").get<double>(),
                  .start_angle_rad = desc_payload.at("start_angle_rad").get<double>(),
                  .end_angle_rad = desc_payload.at("end_angle_rad").get<double>(),
                  .ccw = desc_payload.at("ccw").get<bool>(),
              });
        }
      }
      params.profiles.push_back(profile);
    }
  }
