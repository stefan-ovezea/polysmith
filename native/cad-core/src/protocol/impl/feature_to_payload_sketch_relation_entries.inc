                 {"line_relations",
                  [&feature]() {
                    json relations = json::array();
                    for (const auto& relation :
                         feature.sketch_parameters->line_relations) {
                      relations.push_back({
                          {"relation_id", relation.id},
                          {"kind", relation.kind},
                          {"first_line_id", relation.first_line_id},
                          {"second_line_id", relation.second_line_id},
                        });
                    }
                    return relations;
                   }()},
                  {"constraints",
                   [&feature]() {
                     json constraint_list = json::array();
                     for (const auto& constraint :
                          feature.sketch_parameters->constraints) {
                       json target_ids = json::array();
                       for (const auto& tid : constraint.target_ids) {
                         target_ids.push_back(tid);
                       }
                       constraint_list.push_back({
                           {"constraint_id", constraint.constraint_id},
                           {"kind", constraint.kind},
                           {"target_ids", target_ids},
                           {"value", constraint.value},
                           {"driven", constraint.driven},
                         });
                     }
                     return constraint_list;
                   }()},
                  {"midpoint_anchors",
                  [&feature]() {
                    json anchors = json::array();
                    for (const auto& anchor :
                         feature.sketch_parameters->midpoint_anchors) {
                      anchors.push_back({
                          {"anchor_id", anchor.id},
                          {"point_id", anchor.point_id},
                          {"line_id", anchor.line_id},
                      });
                    }
                    return anchors;
                  }()},
                 {"point_line_anchors",
                  [&feature]() {
                    json anchors = json::array();
                    for (const auto& anchor :
                         feature.sketch_parameters->point_line_anchors) {
                      anchors.push_back({
                          {"anchor_id", anchor.id},
                          {"point_id", anchor.point_id},
                          {"line_id", anchor.line_id},
                          {"t", anchor.t},
                      });
                    }
                    return anchors;
                  }()},
