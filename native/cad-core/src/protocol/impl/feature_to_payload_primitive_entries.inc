      {"box_parameters",
       feature.box_parameters.has_value()
           ? json{
                 {"width", feature.box_parameters->width},
                 {"height", feature.box_parameters->height},
                 {"depth", feature.box_parameters->depth},
             }
           : json(nullptr)},
      {"cylinder_parameters",
       feature.cylinder_parameters.has_value()
           ? json{
                 {"radius", feature.cylinder_parameters->radius},
                 {"height", feature.cylinder_parameters->height},
             }
           : json(nullptr)},
