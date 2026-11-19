      {"toolpaths", [&viewport, scene_unchanged]() {
         json toolpaths_json = json::array();
         if (scene_unchanged) {
           return json::array();  // the client already has this scene
         }
         for (const auto& tp : viewport.toolpaths) {
           toolpaths_json.push_back(to_payload(tp));
         }
         return toolpaths_json;
       }()},
      {"drawing_sheets", [&viewport, scene_unchanged]() {
         json sheets_json = json::array();
         for (const auto& sheet : viewport.drawing_sheets) {
           sheets_json.push_back(to_payload(sheet));
         }
         return sheets_json;
       }()},
      {"cut_previews", [&viewport, scene_unchanged]() {
         json previews = json::array();
         for (const auto& preview : viewport.cut_previews) {
           previews.push_back({
               {"id", preview.id},
               {"positions", encode_f32(preview.positions)},
               {"normals", encode_f32(preview.normals)},
               {"indices", encode_u32(preview.indices)},
           });
         }
         return previews;
       }()},
