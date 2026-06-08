      {"fillet_parameters",
       feature.fillet_parameters.has_value()
           ? json{
                 {"target_body_id",
                  feature.fillet_parameters->target_body_id},
                 {"edge_ids", feature.fillet_parameters->edge_ids},
                 {"radius", feature.fillet_parameters->radius},
                 {"is_pending", feature.fillet_parameters->is_pending},
             }
           : json(nullptr)},
      {"chamfer_parameters",
       feature.chamfer_parameters.has_value()
           ? json{
                 {"target_body_id",
                  feature.chamfer_parameters->target_body_id},
                 {"edge_ids", feature.chamfer_parameters->edge_ids},
                 {"distance", feature.chamfer_parameters->distance},
                 {"is_pending", feature.chamfer_parameters->is_pending},
             }
           : json(nullptr)},
      {"shell_parameters",
       feature.shell_parameters.has_value()
           ? json{
                 {"target_body_id", feature.shell_parameters->target_body_id},
                 {"removed_face_ids",
                  feature.shell_parameters->removed_face_ids},
                 {"thickness", feature.shell_parameters->thickness},
                 {"is_pending", feature.shell_parameters->is_pending},
             }
           : json(nullptr)},
