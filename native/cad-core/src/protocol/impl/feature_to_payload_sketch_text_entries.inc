                 {"texts",
                  [&feature]() {
                    json texts = json::array();
                    for (const auto& entry : feature.sketch_parameters->texts) {
                      texts.push_back({
                          {"text_id", entry.id},
                          {"text", entry.text},
                          {"font_path",
                           entry.font_path.empty() ? json(nullptr)
                                                   : json(entry.font_path)},
                          {"height_mm", entry.height_mm},
                          {"angle_deg", entry.angle_deg},
                          {"anchor_x", entry.anchor_x},
                          {"anchor_y", entry.anchor_y},
                          {"h_align", entry.h_align},
                          {"v_align", entry.v_align},
                          {"char_spacing", entry.char_spacing},
                          {"path_entity_id",
                           entry.path_entity_id.has_value()
                               ? json(entry.path_entity_id.value())
                               : json(nullptr)},
                          {"path_offset",
                           entry.path_offset.has_value()
                               ? json(entry.path_offset.value())
                               : json(nullptr)},
                      });
                    }
                    return texts;
                  }()},
