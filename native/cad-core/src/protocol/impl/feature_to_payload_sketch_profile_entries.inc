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

                      json ordered_edge_ids = json::array();
                      for (const auto& edge_id : profile.ordered_edge_ids) {
                        ordered_edge_ids.push_back(edge_id);
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

                      json arc_descriptors = json::array();
                      for (const auto& desc : profile.arc_descriptors) {
                        arc_descriptors.push_back({
                            {"circle_id", desc.circle_id},
                            {"center_x", desc.center_x},
                            {"center_y", desc.center_y},
                            {"radius", desc.radius},
                            {"start_angle_rad", desc.start_angle_rad},
                            {"end_angle_rad", desc.end_angle_rad},
                            {"ccw", desc.ccw},
                        });
                      }

                      json boundary_edges = json::array();
                      for (const auto& edge : profile.boundary_edges) {
                        boundary_edges.push_back({
                            {"entity_id", edge.entity_id},
                            {"entity_kind", edge.entity_kind},
                            {"param_start", edge.param_start},
                            {"param_end", edge.param_end},
                            {"start_x", edge.start_x},
                            {"start_y", edge.start_y},
                            {"end_x", edge.end_x},
                            {"end_y", edge.end_y},
                            {"center_x", edge.center_x},
                            {"center_y", edge.center_y},
                            {"radius", edge.radius},
                            {"ccw", edge.ccw},
                        });
                      }

                      profiles.push_back({
                          {"profile_id", profile.id},
                          {"kind", profile.kind},
                          {"vertex_ids", point_ids},
                          {"line_ids", line_ids},
                          {"ordered_edge_ids", ordered_edge_ids},
                          {"points", points},
                          {"inner_loops", inner_loops},
                          {"arc_descriptors", arc_descriptors},
                          {"boundary_edges", boundary_edges},
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
