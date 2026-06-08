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
