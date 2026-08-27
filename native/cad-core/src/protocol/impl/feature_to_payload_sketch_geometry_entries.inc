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
                          {"generated_by",
                           line.generated_by.has_value()
                               ? json(line.generated_by.value())
                               : json(nullptr)},
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
                          {"generated_by",
                           circle.generated_by.has_value()
                               ? json(circle.generated_by.value())
                               : json(nullptr)},
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
                          {"generated_by",
                           arc.generated_by.has_value()
                               ? json(arc.generated_by.value())
                               : json(nullptr)},
                      });
                    }
                    return arcs;
                  }()},
                 {"ellipses",
                  [&feature]() {
                    json ellipses = json::array();
                    for (const auto& e : feature.sketch_parameters->ellipses) {
                      ellipses.push_back({
                          {"ellipse_id", e.id},
                          {"center_vertex_id", e.center_vertex_id},
                          {"axis_a_vertex_id", e.axis_a_vertex_id},
                          {"axis_b_vertex_id", e.axis_b_vertex_id},
                          {"center_x", e.center_x},
                          {"center_y", e.center_y},
                          {"a", e.a},
                          {"b", e.b},
                          {"rotation", e.rotation},
                          {"is_construction", e.is_construction},
                          {"generated_by",
                           e.generated_by.has_value()
                               ? json(e.generated_by.value())
                               : json(nullptr)},
                          {"has_sweep", e.has_sweep},
                          {"sweep_start_angle", e.sweep_start_angle},
                          {"sweep_end_angle", e.sweep_end_angle},
                          {"ccw", e.ccw},
                          {"start_vertex_id", e.start_vertex_id},
                          {"end_vertex_id", e.end_vertex_id},
                      });
                    }
                    return ellipses;
                  }()},
                 {"splines",
                  [&feature]() {
                    json splines = json::array();
                    for (const auto& s : feature.sketch_parameters->splines) {
                      json pole_vertex_ids = json::array();
                      for (const auto& vid : s.pole_vertex_ids) {
                        pole_vertex_ids.push_back(vid);
                      }
                      splines.push_back({
                          {"spline_id", s.id},
                          {"pole_vertex_ids", pole_vertex_ids},
                          {"pole_xs", s.pole_xs},
                          {"pole_ys", s.pole_ys},
                          {"degree", s.degree},
                          {"is_construction", s.is_construction},
                          {"generated_by",
                           s.generated_by.has_value()
                               ? json(s.generated_by.value())
                               : json(nullptr)},
                      });
                    }
                    return splines;
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
                  {"vertices",
                  [&feature]() {
                    json vertices = json::array();
                    for (const auto& vtx : feature.sketch_parameters->vertices) {
                      json pt{
                          {"vertex_id", vtx.id},
                          {"kind", vtx.kind},
                          {"x", vtx.x},
                          {"y", vtx.y},
                          {"is_fixed", vtx.is_fixed},
                          {"geometry_owner_ids", vtx.geometry_owner_ids},
                          {"is_projected", vtx.is_projected},
                      };
                      if (vtx.source_type.has_value())
                        pt["source_type"] = vtx.source_type.value();
                      if (vtx.source_feature_id.has_value())
                        pt["source_feature_id"] = vtx.source_feature_id.value();
                      if (vtx.source_edge_id.has_value())
                        pt["source_edge_id"] = vtx.source_edge_id.value();
                      vertices.push_back(std::move(pt));
                    }
                    return vertices;
                  }()},
