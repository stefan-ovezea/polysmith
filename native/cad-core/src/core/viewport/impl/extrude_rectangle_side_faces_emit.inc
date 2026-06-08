        if (parameters.plane_frame.has_value()) {
          const auto& frame = parameters.plane_frame.value();
          const auto make_side_frame = [&](double origin_x,
                                           double origin_y,
                                           double origin_z,
                                           double normal_x,
                                           double normal_y,
                                           double normal_z,
                                           bool along_x_axis) {
            return make_plane_frame(origin_x,
                                    origin_y,
                                    origin_z,
                                    along_x_axis ? frame.y_axis_x : frame.x_axis_x,
                                    along_x_axis ? frame.y_axis_y : frame.x_axis_y,
                                    along_x_axis ? frame.y_axis_z : frame.x_axis_z,
                                    frame.normal_x,
                                    frame.normal_y,
                                    frame.normal_z,
                                    normal_x,
                                    normal_y,
                                    normal_z);
          };

          const auto left_center = to_world_point(frame, 0.0, parameters.height / 2.0, parameters.depth / 2.0);
          const auto right_center = to_world_point(frame, parameters.width, parameters.height / 2.0, parameters.depth / 2.0);
          const auto front_center = to_world_point(frame, parameters.width / 2.0, 0.0, parameters.depth / 2.0);
          const auto back_center = to_world_point(frame, parameters.width / 2.0, parameters.height, parameters.depth / 2.0);

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "left",
              "Left",
              "planar",
              left_center.x,
              left_center.y,
              left_center.z,
              -frame.x_axis_x,
              -frame.x_axis_y,
              -frame.x_axis_z,
              make_side_frame(left_center.x,
                              left_center.y,
                              left_center.z,
                              -frame.x_axis_x,
                              -frame.x_axis_y,
                              -frame.x_axis_z,
                              true),
              parameters.height,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:left"));

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "right",
              "Right",
              "planar",
              right_center.x,
              right_center.y,
              right_center.z,
              frame.x_axis_x,
              frame.x_axis_y,
              frame.x_axis_z,
              make_side_frame(right_center.x,
                              right_center.y,
                              right_center.z,
                              frame.x_axis_x,
                              frame.x_axis_y,
                              frame.x_axis_z,
                              true),
              parameters.height,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:right"));

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "front",
              "Front",
              "planar",
              front_center.x,
              front_center.y,
              front_center.z,
              -frame.y_axis_x,
              -frame.y_axis_y,
              -frame.y_axis_z,
              make_side_frame(front_center.x,
                              front_center.y,
                              front_center.z,
                              -frame.y_axis_x,
                              -frame.y_axis_y,
                              -frame.y_axis_z,
                              false),
              parameters.width,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:front"));

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "back",
              "Back",
              "planar",
              back_center.x,
              back_center.y,
              back_center.z,
              frame.y_axis_x,
              frame.y_axis_y,
              frame.y_axis_z,
              make_side_frame(back_center.x,
                              back_center.y,
                              back_center.z,
                              frame.y_axis_x,
                              frame.y_axis_y,
                              frame.y_axis_z,
                              false),
              parameters.width,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:back"));
        }
