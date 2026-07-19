      {"extrude_parameters",
       feature.extrude_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.extrude_parameters->sketch_feature_id},
                 {"profile_id", feature.extrude_parameters->profile_id},
                 {"profile_ids",
                  [&feature]() {
                    json ids = json::array();
                    for (const auto& id : feature.extrude_parameters->profile_ids) {
                      ids.push_back(id);
                    }
                    return ids;
                  }()},
                 {"open_entity_ids",
                  [&feature]() {
                    json ids = json::array();
                    for (const auto& id :
                         feature.extrude_parameters->open_entity_ids) {
                      ids.push_back(id);
                    }
                    return ids;
                  }()},
                 {"plane_id", feature.extrude_parameters->plane_id},
                 {"plane_frame",
                  feature.extrude_parameters->plane_frame.has_value()
                      ? json{
                            {"origin",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->origin_x},
                                 {"y", feature.extrude_parameters->plane_frame->origin_y},
                                 {"z", feature.extrude_parameters->plane_frame->origin_z},
                             }},
                            {"x_axis",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->x_axis_x},
                                 {"y", feature.extrude_parameters->plane_frame->x_axis_y},
                                 {"z", feature.extrude_parameters->plane_frame->x_axis_z},
                             }},
                            {"y_axis",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->y_axis_x},
                                 {"y", feature.extrude_parameters->plane_frame->y_axis_y},
                                 {"z", feature.extrude_parameters->plane_frame->y_axis_z},
                             }},
                            {"normal",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->normal_x},
                                 {"y", feature.extrude_parameters->plane_frame->normal_y},
                                 {"z", feature.extrude_parameters->plane_frame->normal_z},
                             }},
                        }
                      : json(nullptr)},
                  {"sketch_edge_ids",
                   [&feature]() {
                     json ids = json::array();
                     for (const auto& id :
                          feature.extrude_parameters->sketch_edge_ids) {
                       ids.push_back(id);
                     }
                     return ids;
                   }()},
                  {"sketch_lines",
                   [&feature]() {
                     json lines = json::array();
                     for (const auto& line :
                          feature.extrude_parameters->sketch_lines) {
                       lines.push_back({
                           {"line_id", line.id},
                           {"start_x", line.start_x},
                           {"start_y", line.start_y},
                           {"end_x", line.end_x},
                           {"end_y", line.end_y},
                           {"start_vertex_id", line.start_vertex_id},
                           {"end_vertex_id", line.end_vertex_id},
                           {"is_construction", line.is_construction},
                           {"constraint", line.constraint},
                       });
                     }
                     return lines;
                   }()},
                  {"sketch_arcs",
                   [&feature]() {
                     json arcs = json::array();
                     for (const auto& arc :
                          feature.extrude_parameters->sketch_arcs) {
                       arcs.push_back({
                           {"arc_id", arc.id},
                           {"center_x", arc.center_x},
                           {"center_y", arc.center_y},
                           {"start_x", arc.start_x},
                           {"start_y", arc.start_y},
                           {"end_x", arc.end_x},
                           {"end_y", arc.end_y},
                           {"radius", arc.radius},
                           {"ccw", arc.ccw},
                           {"is_construction", arc.is_construction},
                       });
                     }
                     return arcs;
                   }()},
                  {"sketch_circles",
                   [&feature]() {
                     json circles = json::array();
                     for (const auto& circ :
                          feature.extrude_parameters->sketch_circles) {
                       circles.push_back({
                           {"circle_id", circ.id},
                           {"center_x", circ.center_x},
                           {"center_y", circ.center_y},
                           {"radius", circ.radius},
                           {"is_construction", circ.is_construction},
                       });
                     }
                     return circles;
                   }()},
                  {"profile_kind", feature.extrude_parameters->profile_kind},
                 {"start_x", feature.extrude_parameters->start_x},
                 {"start_y", feature.extrude_parameters->start_y},
                 {"width", feature.extrude_parameters->width},
                 {"height", feature.extrude_parameters->height},
                 {"radius", feature.extrude_parameters->radius},
                 {"profile_points",
                  [&feature]() {
                    json points = json::array();
                    for (const auto& point :
                         feature.extrude_parameters->profile_points) {
                      points.push_back({
                          {"x", point.x},
                          {"y", point.y},
                      });
                    }
                    return points;
                  }()},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.extrude_parameters->inner_loops) {
                      json loop_payload = json::array();
                      for (const auto& point : loop) {
                        loop_payload.push_back({{"x", point.x}, {"y", point.y}});
                      }
                      loops.push_back(loop_payload);
                    }
                    return loops;
                  }()},
                 {"additional_profile_points",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile :
                         feature.extrude_parameters->additional_profile_points) {
                      json points = json::array();
                      for (const auto& point : profile) {
                        points.push_back({{"x", point.x}, {"y", point.y}});
                      }
                      profiles.push_back(points);
                    }
                    return profiles;
                  }()},
                 {"additional_inner_loops",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile_loops :
                         feature.extrude_parameters->additional_inner_loops) {
                      json loops = json::array();
                      for (const auto& loop : profile_loops) {
                        json loop_payload = json::array();
                        for (const auto& point : loop) {
                          loop_payload.push_back({{"x", point.x}, {"y", point.y}});
                        }
                        loops.push_back(loop_payload);
                      }
                      profiles.push_back(loops);
                    }
                    return profiles;
                 }()},
                 {"depth", feature.extrude_parameters->depth},
                 {"extent_mode", feature.extrude_parameters->extent_mode},
                 {"side1",
                  extrude_side_to_payload(feature.extrude_parameters->side1)},
                 {"side2",
                  feature.extrude_parameters->side2.has_value()
                      ? extrude_side_to_payload(
                            feature.extrude_parameters->side2.value())
                      : json(nullptr)},
                 {"thin",
                  {
                      {"enabled", feature.extrude_parameters->thin.enabled},
                      {"thickness", feature.extrude_parameters->thin.thickness},
                      {"placement", feature.extrude_parameters->thin.placement},
                  }},
                 {"mode", feature.extrude_parameters->mode},
                 {"operation", feature.extrude_parameters->operation},
                 {"intersect_result",
                  feature.extrude_parameters->intersect_result},
                 {"target_body_id",
                  feature.extrude_parameters->target_body_id.has_value()
                      ? json(feature.extrude_parameters->target_body_id.value())
                      : json(nullptr)},
             }
           : json(nullptr)},
