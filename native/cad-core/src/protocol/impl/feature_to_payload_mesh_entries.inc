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
      // A PCB layer carries no geometry inline — only its source and the
      // summary counts.  The rings are re-derived from the file on demand, so
      // this block stays small no matter how dense the copper is.
      {"pcb_layer_parameters",
       feature.pcb_layer_parameters.has_value()
           ? json{
                 {"file_path", feature.pcb_layer_parameters->file_path},
                 {"format", feature.pcb_layer_parameters->format ==
                                    polysmith::core::PcbLayerFormat::Excellon
                                ? "excellon"
                                : "gerber"},
                 {"role", polysmith::core::pcb_role_id(feature.pcb_layer_parameters->role)},
                 {"file_polarity", feature.pcb_layer_parameters->file_polarity},
                 {"file_function", feature.pcb_layer_parameters->file_function},
                 {"role_from_attributes",
                  feature.pcb_layer_parameters->role_from_attributes},
                 {"mirror_x", feature.pcb_layer_parameters->mirror_x},
                 {"ring_count", feature.pcb_layer_parameters->ring_count},
                 {"hit_count", feature.pcb_layer_parameters->hit_count},
                 {"tool_count", feature.pcb_layer_parameters->tool_count},
                 {"bounds_valid", feature.pcb_layer_parameters->bounds_valid},
                 {"min_x", feature.pcb_layer_parameters->min_x},
                 {"min_y", feature.pcb_layer_parameters->min_y},
                 {"max_x", feature.pcb_layer_parameters->max_x},
                 {"max_y", feature.pcb_layer_parameters->max_y},
                 {"warnings", feature.pcb_layer_parameters->warnings},
             }
           : json(nullptr)},
