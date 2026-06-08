      {"loft_parameters",
       feature.loft_parameters.has_value()
           ? json{
                 {"ruled", feature.loft_parameters->ruled},
                 {"sections",
                  [&feature]() {
                    json sections = json::array();
                    for (const auto& section :
                         feature.loft_parameters->sections) {
                      sections.push_back({
                          {"sketch_feature_id", section.sketch_feature_id},
                          {"profile_id", section.profile_id},
                          {"plane_id", section.plane_id},
                          {"plane_frame",
                           section.plane_frame.has_value()
                               ? plane_frame_to_payload(section.plane_frame.value())
                               : json(nullptr)},
                          {"profile_points",
                           profile_points_to_payload(section.profile_points)},
                      });
                    }
                    return sections;
                  }()},
             }
           : json(nullptr)},
      {"revolve_parameters",
       feature.revolve_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.revolve_parameters->sketch_feature_id},
                 {"profile_id", feature.revolve_parameters->profile_id},
                 {"plane_id", feature.revolve_parameters->plane_id},
                 {"plane_frame",
                  feature.revolve_parameters->plane_frame.has_value()
                      ? plane_frame_to_payload(
                            feature.revolve_parameters->plane_frame.value())
                      : json(nullptr)},
                 {"profile_kind", feature.revolve_parameters->profile_kind},
                 {"profile_points",
                  profile_points_to_payload(
                      feature.revolve_parameters->profile_points)},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.revolve_parameters->inner_loops) {
                      loops.push_back(profile_points_to_payload(loop));
                    }
                    return loops;
                  }()},
                 {"axis_sketch_feature_id",
                  feature.revolve_parameters->axis_sketch_feature_id},
                 {"axis_entity_id",
                  feature.revolve_parameters->axis_entity_id},
                 {"axis_start_x", feature.revolve_parameters->axis_start_x},
                 {"axis_start_y", feature.revolve_parameters->axis_start_y},
                 {"axis_start_z", feature.revolve_parameters->axis_start_z},
                 {"axis_end_x", feature.revolve_parameters->axis_end_x},
                 {"axis_end_y", feature.revolve_parameters->axis_end_y},
                 {"axis_end_z", feature.revolve_parameters->axis_end_z},
                 {"angle_degrees",
                  feature.revolve_parameters->angle_degrees},
             }
           : json(nullptr)},
      {"sweep_parameters",
       feature.sweep_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.sweep_parameters->sketch_feature_id},
                 {"profile_id", feature.sweep_parameters->profile_id},
                 {"plane_id", feature.sweep_parameters->plane_id},
                 {"plane_frame",
                  feature.sweep_parameters->plane_frame.has_value()
                      ? plane_frame_to_payload(
                            feature.sweep_parameters->plane_frame.value())
                      : json(nullptr)},
                 {"profile_kind", feature.sweep_parameters->profile_kind},
                 {"profile_points",
                  profile_points_to_payload(
                      feature.sweep_parameters->profile_points)},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.sweep_parameters->inner_loops) {
                      loops.push_back(profile_points_to_payload(loop));
                    }
                    return loops;
                  }()},
                 {"path_sketch_feature_id",
                  feature.sweep_parameters->path_sketch_feature_id},
                 {"path_entity_id",
                  feature.sweep_parameters->path_entity_id},
                 {"path_start_x", feature.sweep_parameters->path_start_x},
                 {"path_start_y", feature.sweep_parameters->path_start_y},
                 {"path_start_z", feature.sweep_parameters->path_start_z},
                 {"path_end_x", feature.sweep_parameters->path_end_x},
                 {"path_end_y", feature.sweep_parameters->path_end_y},
                 {"path_end_z", feature.sweep_parameters->path_end_z},
                 {"path_segments",
                  [&feature]() {
                    json segments = json::array();
                    for (const auto& segment :
                         feature.sweep_parameters->path_segments) {
                      segments.push_back(
                          json{{"entity_id", segment.entity_id},
                               {"kind", segment.kind},
                               {"start_x", segment.start_x},
                               {"start_y", segment.start_y},
                               {"start_z", segment.start_z},
                               {"end_x", segment.end_x},
                               {"end_y", segment.end_y},
                               {"end_z", segment.end_z},
                               {"center_x", segment.center_x},
                               {"center_y", segment.center_y},
                               {"center_z", segment.center_z},
                               {"mid_x", segment.mid_x},
                               {"mid_y", segment.mid_y},
                               {"mid_z", segment.mid_z},
                               {"radius", segment.radius},
                               {"ccw", segment.ccw}});
                    }
                    return segments;
                  }()},
             }
           : json(nullptr)},
