      {"construction_plane_parameters",
       feature.construction_plane_parameters.has_value()
           ? json{
                 {"plane_type",
                  feature.construction_plane_parameters->plane_type},
                 {"source_plane_id",
                 feature.construction_plane_parameters->source_plane_id},
                 {"source_plane_ids",
                  feature.construction_plane_parameters->source_plane_ids},
                 {"source_axis_id",
                  feature.construction_plane_parameters->source_axis_id},
                 {"offset", feature.construction_plane_parameters->offset},
                 {"angle_degrees",
                  feature.construction_plane_parameters->angle_degrees},
                 {"plane_frame",
                  {
                      {"origin",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.origin_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.origin_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.origin_z},
                       }},
                      {"x_axis",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_z},
                       }},
                      {"y_axis",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_z},
                       }},
                      {"normal",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.normal_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.normal_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.normal_z},
                       }},
                  }},
             }
           : json(nullptr)},
      {"construction_axis_parameters",
       feature.construction_axis_parameters.has_value()
           ? json{
                 {"source_id", feature.construction_axis_parameters->source_id},
                 {"source_kind",
                  feature.construction_axis_parameters->source_kind},
                 {"start",
                  {{"x", feature.construction_axis_parameters->start_x},
                   {"y", feature.construction_axis_parameters->start_y},
                   {"z", feature.construction_axis_parameters->start_z}}},
                 {"end",
                  {{"x", feature.construction_axis_parameters->end_x},
                   {"y", feature.construction_axis_parameters->end_y},
                   {"z", feature.construction_axis_parameters->end_z}}},
             }
           : json(nullptr)},
      {"construction_point_parameters",
       feature.construction_point_parameters.has_value()
           ? json{
                 {"source_id", feature.construction_point_parameters->source_id},
                 {"source_kind",
                  feature.construction_point_parameters->source_kind},
                 {"position",
                  {{"x", feature.construction_point_parameters->x},
                   {"y", feature.construction_point_parameters->y},
                   {"z", feature.construction_point_parameters->z}}},
             }
           : json(nullptr)},
