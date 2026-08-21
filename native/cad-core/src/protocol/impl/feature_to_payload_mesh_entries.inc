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
      {"step_import_parameters",
       feature.step_import_parameters.has_value()
           ? json{
                 {"file_path", feature.step_import_parameters->file_path},
                 {"source_units", feature.step_import_parameters->source_units},
                 {"serialized_shape",
                  include_opaque
                      ? json(feature.step_import_parameters->serialized_shape)
                      : json("")},
                 {"solid_count", feature.step_import_parameters->solid_count},
                 {"face_count", feature.step_import_parameters->face_count},
             }
           : json(nullptr)},
      {"iges_import_parameters",
       feature.iges_import_parameters.has_value()
           ? json{
                 {"file_path", feature.iges_import_parameters->file_path},
                 {"source_units", feature.iges_import_parameters->source_units},
                 {"serialized_shape",
                  include_opaque
                      ? json(feature.iges_import_parameters->serialized_shape)
                      : json("")},
                 {"solid_count", feature.iges_import_parameters->solid_count},
                 {"face_count", feature.iges_import_parameters->face_count},
             }
           : json(nullptr)},
