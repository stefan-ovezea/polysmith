                 {"projected_points",
                  [&feature]() {
                    json projected = json::array();
                    for (const auto& point :
                         feature.sketch_parameters->projected_points) {
                      projected.push_back({
                          {"vertex_id", point.id},
                          {"source_id", point.source_id},
                          {"x", point.x},
                          {"y", point.y},
                      });
                    }
                    return projected;
                  }()},
                 {"projected_sources",
                  [&feature]() {
                    json sources = json::array();
                    for (const auto& source :
                         feature.sketch_parameters->projected_sources) {
                      sources.push_back(source);
                    }
                    return sources;
                  }()},
                 {"projections",
                  [&feature]() {
                    json projections_payload = json::array();
                    for (const auto& projection :
                         feature.sketch_parameters->projections) {
                      json line_ids = json::array();
                      for (const auto& id : projection.generated_line_ids) {
                        line_ids.push_back(id);
                      }
                      json circle_ids = json::array();
                      for (const auto& id : projection.generated_circle_ids) {
                        circle_ids.push_back(id);
                      }
                      json arc_ids = json::array();
                      for (const auto& id : projection.generated_arc_ids) {
                        arc_ids.push_back(id);
                      }
                      projections_payload.push_back({
                          {"projection_id", projection.id},
                          {"source_id", projection.source_id},
                          {"source_kind", projection.source_kind},
                          {"generated_line_ids", line_ids},
                          {"generated_circle_ids", circle_ids},
                          {"generated_arc_ids", arc_ids},
                          {"generated_vertex_id",
                           projection.generated_vertex_id},
                          {"dependency_broken", projection.dependency_broken},
                          {"dependency_warning",
                           projection.dependency_warning},
                      });
                    }
                    return projections_payload;
                  }()},
