                 {"slots",
                  [&feature]() {
                    json slots = json::array();
                    for (const auto& slot : feature.sketch_parameters->slots) {
                      slots.push_back({
                          {"slot_id", slot.id},
                          {"center_vertex_id", slot.center_vertex_id},
                          {"center_x", slot.center_x},
                          {"center_y", slot.center_y},
                          {"length", slot.length},
                          {"radius", slot.radius},
                          {"rotation", slot.rotation},
                          {"mode", slot.mode},
                          {"is_construction", slot.is_construction},
                      });
                    }
                    return slots;
                  }()},
