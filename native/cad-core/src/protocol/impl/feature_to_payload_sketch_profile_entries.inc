                 {"profiles",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile : feature.sketch_parameters->profiles) {
                      json point_ids = json::array();
                      for (const auto& point_id : profile.vertex_ids) {
                        point_ids.push_back(point_id);
                      }

                      json line_ids = json::array();
                      for (const auto& line_id : profile.line_ids) {
                        line_ids.push_back(line_id);
                      }

                      json points = json::array();
                      for (const auto& point : profile.points) {
                        points.push_back({
                            {"x", point.x},
                            {"y", point.y},
                        });
                      }
                      json inner_loops = json::array();
                      for (const auto& loop : profile.inner_loops) {
                        json loop_payload = json::array();
                        for (const auto& point : loop) {
                          loop_payload.push_back({
                              {"x", point.x},
                              {"y", point.y},
                          });
                        }
                        inner_loops.push_back(loop_payload);
                      }

                      profiles.push_back({
                          {"profile_id", profile.id},
                          {"kind", profile.kind},
                          {"vertex_ids", point_ids},
                          {"line_ids", line_ids},
                          {"points", points},
                          {"inner_loops", inner_loops},
                          {"source_circle_id",
                           profile.source_circle_id.has_value()
                               ? json(profile.source_circle_id.value())
                               : json(nullptr)},
                          {"center_x", profile.center_x},
                          {"center_y", profile.center_y},
                          {"radius", profile.radius},
                      });
                    }
                    return profiles;
                  }()},
