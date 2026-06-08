      {"bodies", [&viewport]() {
         json bodies_json = json::array();
         for (const auto& body : viewport.bodies) {
           bodies_json.push_back({
               {"id", body.id},
               {"label", body.label},
               {"center",
                {{"x", body.center_x},
                 {"y", body.center_y},
                 {"z", body.center_z}}},
               {"size",
                {{"x", body.width},
                 {"y", body.height},
                 {"z", body.depth}}},
               {"local_frame",
                {
                    {"x_axis",
                     {{"x", body.local_frame.x_axis_x},
                      {"y", body.local_frame.x_axis_y},
                      {"z", body.local_frame.x_axis_z}}},
                    {"y_axis",
                     {{"x", body.local_frame.y_axis_x},
                      {"y", body.local_frame.y_axis_y},
                      {"z", body.local_frame.y_axis_z}}},
                    {"z_axis",
                     {{"x", body.local_frame.z_axis_x},
                      {"y", body.local_frame.z_axis_y},
                      {"z", body.local_frame.z_axis_z}}},
                }},
           });
         }
         return bodies_json;
       }()},
      {"edges", [&viewport]() {
         json edges_json = json::array();
         for (const auto& edge : viewport.edges) {
           edges_json.push_back({
               {"id", edge.id},
               {"owner_body_id", edge.owner_body_id},
               {"kind", edge.kind},
               {"points", edge.points},
               {"length", edge.length},
               {"is_selected", edge.is_selected},
           });
         }
         return edges_json;
       }()},
      {"vertices", [&viewport]() {
         json vertices_json = json::array();
         for (const auto& vertex : viewport.vertices) {
           vertices_json.push_back({
               {"id", vertex.id},
               {"owner_body_id", vertex.owner_body_id},
               {"position",
                {
                    {"x", vertex.x},
                    {"y", vertex.y},
                    {"z", vertex.z},
                }},
               {"is_selected", vertex.is_selected},
           });
         }
         return vertices_json;
       }()},
