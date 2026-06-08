      } else if (parameters.profile_kind == "polygon") {
        polygon_extrudes.push_back(ViewportPolygonExtrudePrimitive{
            .id = feature.id,
            .label = feature.name,
            .plane_id = parameters.plane_id,
            .plane_frame = parameters.plane_frame.has_value()
                               ? std::optional<ViewportSketchPlaneFrame>(
                                     make_sketch_plane_frame(parameters.plane_frame.value()))
                               : std::nullopt,
            .profile_points = parameters.profile_points,
            .inner_loops = parameters.inner_loops,
            .depth = parameters.depth,
            .is_selected = is_selected,
            .appearance_color = body_appearance_color(*view, feature.id),
        });

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = std::numeric_limits<double>::max();
        double max_z = std::numeric_limits<double>::lowest();

        for (const auto& point : parameters.profile_points) {
          if (parameters.plane_frame.has_value()) {
            include_world_point_for_frame(parameters.plane_frame.value(),
                                          point.x,
                                          point.y,
                                          parameters.depth,
                                          min_x,
                                          max_x,
                                          min_y,
                                          max_y,
                                          min_z,
                                          max_z);
            continue;
          }

          include_world_point_for_plane(parameters.plane_id,
                                       point.x,
                                       point.y,
                                       parameters.depth,
                                       min_x,
                                       max_x,
                                       min_y,
                                       max_y,
                                       min_z,
                                       max_z);
        }

        if (!parameters.profile_points.empty()) {
          scene_width = std::max(scene_width, max_x);
          max_height = std::max(max_height, max_y);
          max_depth = std::max(max_depth, max_z);
        }

        double face_center_x = (min_x + max_x) / 2.0;
        double face_center_y = parameters.depth;
        double face_center_z = (min_z + max_z) / 2.0;
        double face_width = max_x - min_x;
        double face_height = max_z - min_z;
        double face_normal_x = 0.0;
        double face_normal_y = 1.0;
        double face_normal_z = 0.0;
        if (parameters.plane_frame.has_value()) {
          const auto& frame = parameters.plane_frame.value();
          const double local_center_x = (min_x + max_x) / 2.0;
          const double local_center_y = (min_z + max_z) / 2.0;
          const auto base_center =
              to_world_point(frame, local_center_x, local_center_y, 0.0);
          const auto top_center =
              to_world_point(frame, local_center_x, local_center_y, parameters.depth);
          face_center_x = top_center.x;
          face_center_y = top_center.y;
          face_center_z = top_center.z;
          face_normal_x = frame.normal_x;
          face_normal_y = frame.normal_y;
          face_normal_z = frame.normal_z;

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "base",
              "Base",
              "planar",
              base_center.x,
              base_center.y,
              base_center.z,
              -frame.normal_x,
              -frame.normal_y,
              -frame.normal_z,
              make_plane_frame(base_center.x,
                               base_center.y,
                               base_center.z,
                               frame.x_axis_x,
                               frame.x_axis_y,
                               frame.x_axis_z,
                               frame.y_axis_x,
                               frame.y_axis_y,
                               frame.y_axis_z,
                               -frame.normal_x,
                               -frame.normal_y,
                               -frame.normal_z),
              face_width,
              face_height,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:base"));
        } else if (parameters.plane_id == "ref-plane-yz") {
          face_center_x = parameters.depth;
          face_center_y = (min_y + max_y) / 2.0;
          face_center_z = (min_z + max_z) / 2.0;
          face_width = max_y - min_y;
          face_height = max_z - min_z;
          face_normal_x = 1.0;
          face_normal_y = 0.0;
        } else if (parameters.plane_id == "ref-plane-xz") {
          face_center_x = (min_x + max_x) / 2.0;
          face_center_y = (min_y + max_y) / 2.0;
          face_center_z = parameters.depth;
          face_width = max_x - min_x;
          face_height = max_y - min_y;
          face_normal_y = 0.0;
          face_normal_z = 1.0;
        }

        solid_faces.push_back(make_solid_face(
            feature.id,
            feature.kind,
            "top",
            "Top",
            "planar",
            face_center_x,
            face_center_y,
            face_center_z,
            face_normal_x,
            face_normal_y,
            face_normal_z,
            parameters.plane_frame.has_value()
                ? make_plane_frame(face_center_x,
                                   face_center_y,
                                   face_center_z,
                                   parameters.plane_frame->x_axis_x,
                                   parameters.plane_frame->x_axis_y,
                                   parameters.plane_frame->x_axis_z,
                                   parameters.plane_frame->y_axis_x,
                                   parameters.plane_frame->y_axis_y,
                                   parameters.plane_frame->y_axis_z,
                                   parameters.plane_frame->normal_x,
                                   parameters.plane_frame->normal_y,
                                   parameters.plane_frame->normal_z)
                : make_face_frame_for_plane(
                      parameters.plane_id, face_center_x, face_center_y, face_center_z),
            face_width,
            face_height,
            0.0,
            view->selected_face_id.has_value() &&
                view->selected_face_id.value() == feature.id + ":face:top"));

        if (parameters.plane_frame.has_value() &&
            parameters.profile_points.size() >= 3) {
          const auto& frame = parameters.plane_frame.value();
          for (size_t index = 0; index < parameters.profile_points.size(); ++index) {
            const auto& start = parameters.profile_points[index];
            const auto& end = parameters.profile_points[(index + 1) %
                                                       parameters.profile_points.size()];

            const auto base_start = to_world_point(frame, start.x, start.y, 0.0);
            const auto base_end = to_world_point(frame, end.x, end.y, 0.0);
            const auto top_start = to_world_point(frame, start.x, start.y, parameters.depth);
            const auto top_end = to_world_point(frame, end.x, end.y, parameters.depth);

            const WorldVector edge_vector = subtract_points(base_end, base_start);
            const WorldVector depth_vector = subtract_points(top_start, base_start);
            const double edge_length = vector_length(edge_vector);
            const WorldVector face_normal = normalize_vector(cross_product(edge_vector, depth_vector));
            if (edge_length <= 0.0 ||
                vector_length(face_normal) <= 0.0) {
              continue;
            }

            const WorldPoint center{
                .x = (base_start.x + base_end.x + top_start.x + top_end.x) / 4.0,
                .y = (base_start.y + base_end.y + top_start.y + top_end.y) / 4.0,
                .z = (base_start.z + base_end.z + top_start.z + top_end.z) / 4.0,
            };

            const WorldVector edge_axis = normalize_vector(edge_vector);
            const WorldVector depth_axis = normalize_vector(depth_vector);

            solid_faces.push_back(make_solid_face(
                feature.id,
                feature.kind,
                "side-" + std::to_string(index),
                "Side",
                "planar",
                center.x,
                center.y,
                center.z,
                face_normal.x,
                face_normal.y,
                face_normal.z,
                make_plane_frame(center.x,
                                 center.y,
                                 center.z,
                                 edge_axis.x,
                                 edge_axis.y,
                                 edge_axis.z,
                                 depth_axis.x,
                                 depth_axis.y,
                                 depth_axis.z,
                                 face_normal.x,
                                 face_normal.y,
                                 face_normal.z),
                edge_length,
                parameters.depth,
                0.0,
                view->selected_face_id.has_value() &&
                    view->selected_face_id.value() ==
                        feature.id + ":face:side-" + std::to_string(index)));
          }
        }
