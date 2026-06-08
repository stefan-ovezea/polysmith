      {"toolpaths", [&viewport]() {
         json toolpaths_json = json::array();
         for (const auto& tp : viewport.toolpaths) {
           toolpaths_json.push_back(to_payload(tp));
         }
         return toolpaths_json;
       }()},
      {"cut_previews", [&viewport]() {
         json previews = json::array();
         for (const auto& preview : viewport.cut_previews) {
           previews.push_back({
               {"id", preview.id},
               {"positions", preview.positions},
               {"normals", preview.normals},
               {"indices", preview.indices},
           });
         }
         return previews;
       }()},
