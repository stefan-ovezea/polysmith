      {"mesh_import_parameters",
       feature.mesh_import_parameters.has_value()
           ? json{
                 {"file_path", feature.mesh_import_parameters->file_path},
                 {"scale", feature.mesh_import_parameters->scale},
             }
           : json(nullptr)},
      {"mesh_to_body_parameters",
       feature.mesh_to_body_parameters.has_value()
           ? json{
                 {"source_body_id",
                  feature.mesh_to_body_parameters->source_body_id},
                 {"serialized_shape",
                  include_opaque
                      ? json(feature.mesh_to_body_parameters->serialized_shape)
                      : json("")},
             }
           : json(nullptr)},
