      {"plugin_feature_parameters",
       feature.plugin_parameters.has_value()
           ? json{
                 {"plugin_id", feature.plugin_parameters->plugin_id},
                 {"feature_type", feature.plugin_parameters->feature_type},
                 {"schema_version", feature.plugin_parameters->schema_version},
                 {"is_pending", feature.plugin_parameters->is_pending},
                 {"display_name", feature.plugin_parameters->display_name},
                 {"parameters_summary",
                  feature.plugin_parameters->parameters_summary},
                 {"parameters",
                  [&feature]() {
                    try {
                      return json::parse(
                          feature.plugin_parameters->parameters_json);
                    } catch (...) {
                      return json::object();
                    }
                  }()},
                 {"geometry",
                  [&feature]() {
                    json operations = json::array();
                    for (const auto& operation :
                         feature.plugin_parameters->geometry) {
                      operations.push_back(json{
                          {"operation", operation.operation},
                          {"primitive", operation.primitive},
                          {"x", operation.x},
                          {"y", operation.y},
                          {"z", operation.z},
                          {"width", operation.width},
                          {"depth", operation.depth},
                          {"height", operation.height},
                          {"radius", operation.radius},
                          {"top_width", operation.top_width},
                          {"top_depth", operation.top_depth},
                          {"top_radius", operation.top_radius},
                      });
                    }
                    return operations;
                  }()},
             }
           : json(nullptr)},
