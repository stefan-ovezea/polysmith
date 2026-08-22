                 {"chamfers",
                  [&feature]() {
                    json chamfers = json::array();
                    for (const auto& chamfer : feature.sketch_parameters->chamfers) {
                      chamfers.push_back({
                          {"chamfer_id", chamfer.id},
                          {"corner_vertex_id", chamfer.corner_vertex_id},
                          {"corner_x", chamfer.corner_x},
                          {"corner_y", chamfer.corner_y},
                          {"line_a_id", chamfer.line_a_id},
                          {"line_b_id", chamfer.line_b_id},
                          {"trim_a_vertex_id", chamfer.trim_a_vertex_id},
                          {"trim_b_vertex_id", chamfer.trim_b_vertex_id},
                          {"chamfer_line_id", chamfer.chamfer_line_id},
                          {"distance_a", chamfer.distance_a},
                          {"distance_b", chamfer.distance_b},
                      });
                    }
                    return chamfers;
                  }()},
