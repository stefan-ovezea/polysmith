      if (parameters.profile_kind == "rectangle") {
        double face_center_x = 0.0;
        double face_center_y = 0.0;
        double face_center_z = 0.0;
        double face_normal_x = 0.0;
        double face_normal_y = 0.0;
        double face_normal_z = 0.0;
        double face_width = parameters.width;
        double face_height = parameters.height;
        double face_origin_x = 0.0;
        double face_origin_y = 0.0;
        double face_origin_z = 0.0;

        if (parameters.plane_frame.has_value()) {
          const auto& frame = parameters.plane_frame.value();
          polygon_extrudes.push_back(ViewportPolygonExtrudePrimitive{
              .id = feature.id,
              .label = feature.name,
              .plane_id = parameters.plane_id,
              .plane_frame = std::optional<ViewportSketchPlaneFrame>(
                  make_sketch_plane_frame(frame)),
              .profile_points = {
                  SketchProfilePoint{.x = parameters.start_x, .y = parameters.start_y},
                  SketchProfilePoint{.x = parameters.start_x + parameters.width,
                                     .y = parameters.start_y},
                  SketchProfilePoint{.x = parameters.start_x + parameters.width,
                                     .y = parameters.start_y + parameters.height},
                  SketchProfilePoint{.x = parameters.start_x,
                                     .y = parameters.start_y + parameters.height},
              },
              .inner_loops = {},
              .depth = parameters.depth,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });

          const auto top_center = to_world_point(
              frame,
              parameters.start_x + parameters.width / 2.0,
              parameters.start_y + parameters.height / 2.0,
              parameters.depth);
          const auto base_center = to_world_point(
              frame,
              parameters.start_x + parameters.width / 2.0,
              parameters.start_y + parameters.height / 2.0,
              0.0);

          face_center_x = top_center.x;
          face_center_y = top_center.y;
          face_center_z = top_center.z;
          face_normal_x = frame.normal_x;
          face_normal_y = frame.normal_y;
          face_normal_z = frame.normal_z;
          face_origin_x = top_center.x;
          face_origin_y = top_center.y;
          face_origin_z = top_center.z;

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
        } else if (parameters.plane_id == "ref-plane-xy") {
          boxes.push_back(ViewportBoxPrimitive{
              .id = feature.id,
              .label = feature.name,
              .width = parameters.width,
              .height = parameters.depth,
              .depth = parameters.height,
              .x_offset = parameters.start_x,
              .center_x = parameters.start_x + parameters.width / 2.0,
              .center_y = parameters.depth / 2.0,
              .center_z = parameters.start_y + parameters.height / 2.0,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          face_center_x = parameters.start_x + parameters.width / 2.0;
          face_center_y = parameters.depth;
          face_center_z = parameters.start_y + parameters.height / 2.0;
          face_normal_y = 1.0;
          face_origin_x = face_center_x;
          face_origin_y = face_center_y;
          face_origin_z = face_center_z;
          max_height = std::max(max_height, parameters.depth);
          max_depth = std::max(max_depth, parameters.start_y + parameters.height);
          scene_width = std::max(scene_width, parameters.start_x + parameters.width);
        } else if (parameters.plane_id == "ref-plane-yz") {
          boxes.push_back(ViewportBoxPrimitive{
              .id = feature.id,
              .label = feature.name,
              .width = parameters.depth,
              .height = parameters.width,
              .depth = parameters.height,
              .x_offset = 0.0,
              .center_x = parameters.depth / 2.0,
              .center_y = parameters.start_x + parameters.width / 2.0,
              .center_z = parameters.start_y + parameters.height / 2.0,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          face_center_x = parameters.depth;
          face_center_y = parameters.start_x + parameters.width / 2.0;
          face_center_z = parameters.start_y + parameters.height / 2.0;
          face_normal_x = 1.0;
          face_origin_x = face_center_x;
          face_origin_y = face_center_y;
          face_origin_z = face_center_z;
          max_height = std::max(max_height, parameters.start_x + parameters.width);
          max_depth = std::max(max_depth, parameters.start_y + parameters.height);
          scene_width = std::max(scene_width, parameters.depth);
        } else {
          boxes.push_back(ViewportBoxPrimitive{
              .id = feature.id,
              .label = feature.name,
              .width = parameters.width,
              .height = parameters.height,
              .depth = parameters.depth,
              .x_offset = parameters.start_x,
              .center_x = parameters.start_x + parameters.width / 2.0,
              .center_y = parameters.start_y + parameters.height / 2.0,
              .center_z = parameters.depth / 2.0,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          face_center_x = parameters.start_x + parameters.width / 2.0;
          face_center_y = parameters.start_y + parameters.height / 2.0;
          face_center_z = parameters.depth;
          face_normal_z = 1.0;
          face_origin_x = face_center_x;
          face_origin_y = face_center_y;
          face_origin_z = face_center_z;
          max_height = std::max(max_height, parameters.start_y + parameters.height);
          max_depth = std::max(max_depth, parameters.depth);
          scene_width = std::max(scene_width, parameters.start_x + parameters.width);
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
                ? make_plane_frame(face_origin_x,
                                   face_origin_y,
                                   face_origin_z,
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
                      parameters.plane_id, face_origin_x, face_origin_y, face_origin_z),
            face_width,
            face_height,
            0.0,
            view->selected_face_id.has_value() &&
                view->selected_face_id.value() == feature.id + ":face:top"));

        #include "core/viewport/impl/extrude_rectangle_side_faces_emit.inc"
