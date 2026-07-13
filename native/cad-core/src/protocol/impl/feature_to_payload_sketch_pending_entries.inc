                 {"active_tool", feature.sketch_parameters->active_tool},
                 // Pending mirror tool state. Emits `null` when no
                 // mirror is in progress; the UI uses presence of
                 // this object as the signal to show the floating
                 // panel.
                 {"pending_mirror",
                  feature.sketch_parameters->pending_mirror.has_value()
                      ? [&feature]() {
                          const auto& pm =
                              *feature.sketch_parameters->pending_mirror;
                          json object_ids = json::array();
                          for (const auto& id : pm.object_ids) {
                            object_ids.push_back(id);
                          }
                          json generated_lines = json::array();
                          for (const auto& line : pm.generated_lines) {
                            generated_lines.push_back({
                                {"line_id", line.id},
                                {"start_vertex_id", line.start_vertex_id},
                                {"end_vertex_id", line.end_vertex_id},
                                {"start_x", line.start_x},
                                {"start_y", line.start_y},
                                {"end_x", line.end_x},
                                {"end_y", line.end_y},
                                {"is_construction", line.is_construction},
                            });
                          }
                          json generated_circles = json::array();
                          for (const auto& circle : pm.generated_circles) {
                            generated_circles.push_back({
                                {"circle_id", circle.id},
                                {"center_x", circle.center_x},
                                {"center_y", circle.center_y},
                                {"radius", circle.radius},
                                {"is_construction", circle.is_construction},
                            });
                          }
                          return json{
                              {"axis_line_id",
                               pm.axis_line_id.has_value()
                                   ? json(*pm.axis_line_id)
                                   : json(nullptr)},
                              {"object_ids", object_ids},
                              {"generated_lines", generated_lines},
                              {"generated_circles", generated_circles},
                          };
                        }()
                      : json(nullptr)},
