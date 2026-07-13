                 {"lines",
                  [&feature]() {
                    json lines = json::array();
                    for (const auto& line : feature.sketch_parameters->lines) {
                      lines.push_back({
                          {"line_id", line.id},
                          {"start_vertex_id", line.start_vertex_id},
                          {"end_vertex_id", line.end_vertex_id},
                          {"start_x", line.start_x},
                          {"start_y", line.start_y},
                          {"end_x", line.end_x},
                          {"end_y", line.end_y},
                          {"constraint",
                           line.constraint.has_value()
                               ? json(line.constraint.value())
                               : json(nullptr)},
                          {"is_construction", line.is_construction},
                      });
                    }
                    return lines;
                  }()},
                 {"circles",
                 [&feature]() {
                    json circles = json::array();
                    for (const auto& circle : feature.sketch_parameters->circles) {
                      circles.push_back({
                          {"circle_id", circle.id},
                          {"center_vertex_id", circle.center_vertex_id},
                          {"center_x", circle.center_x},
                          {"center_y", circle.center_y},
                          {"radius", circle.radius},
                          {"is_construction", circle.is_construction},
                      });
                    }
                    return circles;
                  }()},
                 {"arcs",
                 [&feature]() {
                    json arcs = json::array();
                    for (const auto& arc : feature.sketch_parameters->arcs) {
                      arcs.push_back({
                          {"arc_id", arc.id},
                          {"start_vertex_id", arc.start_vertex_id},
                          {"end_vertex_id", arc.end_vertex_id},
                          {"center_vertex_id", arc.center_vertex_id},
                          {"center_x", arc.center_x},
                          {"center_y", arc.center_y},
                          {"radius", arc.radius},
                          {"start_x", arc.start_x},
                          {"start_y", arc.start_y},
                          {"end_x", arc.end_x},
                          {"end_y", arc.end_y},
                          {"ccw", arc.ccw},
                          {"is_construction", arc.is_construction},
                      });
                    }
                    return arcs;
                  }()},
                 {"fillets",
                  [&feature]() {
                    json fillets = json::array();
                    for (const auto& fillet : feature.sketch_parameters->fillets) {
                      fillets.push_back({
                          {"fillet_id", fillet.id},
                          {"corner_vertex_id", fillet.corner_vertex_id},
                          {"corner_x", fillet.corner_x},
                          {"corner_y", fillet.corner_y},
                          {"line_a_id", fillet.line_a_id},
                          {"line_b_id", fillet.line_b_id},
                          {"trim_a_vertex_id", fillet.trim_a_vertex_id},
                          {"trim_b_vertex_id", fillet.trim_b_vertex_id},
                          {"arc_id", fillet.arc_id},
                          {"radius", fillet.radius},
                      });
                    }
                    return fillets;
                  }()},
                  {"polygons",
                   [&feature]() {
                     json polygons = json::array();
                     for (const auto& polygon : feature.sketch_parameters->polygons) {
                       polygons.push_back({
                           {"polygon_id", polygon.id},
                           {"center_x", polygon.center_x},
                           {"center_y", polygon.center_y},
                           {"radius", polygon.radius},
                           {"sides", polygon.sides},
                           {"mode", polygon.mode},
                           {"start_x", polygon.start_x},
                           {"start_y", polygon.start_y},
                           {"end_x", polygon.end_x},
                           {"end_y", polygon.end_y},
                           {"is_construction", polygon.is_construction},
                       });
                     }
                     return polygons;
                   }()},
                  {"points",
                  [&feature]() {
                    json points = json::array();
                    for (const auto& point : feature.sketch_parameters->vertices) {
                      json pt{
                          {"vertex_id", point.id},
                          {"kind", point.kind},
                          {"x", point.x},
                          {"y", point.y},
                          {"is_fixed", point.is_fixed},
                          {"geometry_owner_ids", point.geometry_owner_ids},
                          {"is_projected", point.is_projected},
                      };
                      if (point.source_type.has_value())
                        pt["source_type"] = point.source_type.value();
                      if (point.source_feature_id.has_value())
                        pt["source_feature_id"] = point.source_feature_id.value();
                      if (point.source_edge_id.has_value())
                        pt["source_edge_id"] = point.source_edge_id.value();
                      points.push_back(std::move(pt));
                    }
                    return points;
                  }()},
