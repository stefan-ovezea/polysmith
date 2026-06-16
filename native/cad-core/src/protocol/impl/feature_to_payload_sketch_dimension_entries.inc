                 {"dimensions",
                  [&feature]() {
                    json dimensions = json::array();
                    for (const auto& dimension :
                         feature.sketch_parameters->dimensions) {
                      dimensions.push_back({
                          {"dimension_id", dimension.id},
                          {"kind", dimension.kind},
                          {"entity_id", dimension.entity_id},
                          {"secondary_entity_id",
                           dimension.secondary_entity_id},
                          {"value", dimension.value},
                          {"expression", dimension.expression},
                          {"driven", dimension.driven},
                          {"is_auto", dimension.is_auto},
                          {"display_as", dimension.display_as},
                          {"label_x",
                           dimension.label_x.has_value()
                               ? json(*dimension.label_x)
                               : json(nullptr)},
                          {"label_y",
                           dimension.label_y.has_value()
                               ? json(*dimension.label_y)
                               : json(nullptr)},
                          {"pivot_x",
                           dimension.pivot_x.has_value()
                               ? json(*dimension.pivot_x)
                               : json(nullptr)},
                          {"pivot_y",
                           dimension.pivot_y.has_value()
                               ? json(*dimension.pivot_y)
                               : json(nullptr)},
                      });
                    }
                    return dimensions;
                  }()},
