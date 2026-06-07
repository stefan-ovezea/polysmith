  for (const auto& feature : view->feature_history) {
    // Suppressed features are excluded from every viewport-visible
    // emission path: legacy primitives, sketch overlays, body-derived
    // faces (already excluded via body_compiler skipping them). The UI
    // still shows the feature in the timeline / hierarchy, just dimmed.
    if (feature.suppressed) {
      continue;
    }
    const bool is_selected =
        view->selected_feature_id.has_value() &&
        view->selected_feature_id.value() == feature.id;
    const bool feature_consumed_by_boolean =
        consumed.find(feature.id) != consumed.end();

    if (feature.kind == "box" && feature.box_parameters.has_value()) {
      if (feature_consumed_by_boolean) {
        continue;
      }
      const auto& parameters = feature.box_parameters.value();
      boxes.push_back(ViewportBoxPrimitive{
          .id = feature.id,
          .label = feature.name,
          .width = parameters.width,
          .height = parameters.height,
          .depth = parameters.depth,
          .x_offset = current_x_offset,
          .center_x = current_x_offset + parameters.width / 2.0,
          .center_y = parameters.height / 2.0,
          .center_z = parameters.depth / 2.0,
          .is_selected = is_selected,
          .appearance_color = body_appearance_color(*view, feature.id),
      });
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "top",
          "Top",
          "planar",
          current_x_offset + parameters.width / 2.0,
          parameters.height,
          parameters.depth / 2.0,
          0.0,
          1.0,
          0.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           parameters.height,
                           parameters.depth / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           1.0,
                           0.0),
          parameters.width,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:top"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "bottom",
          "Bottom",
          "planar",
          current_x_offset + parameters.width / 2.0,
          0.0,
          parameters.depth / 2.0,
          0.0,
          -1.0,
          0.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           0.0,
                           parameters.depth / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           -1.0,
                           0.0),
          parameters.width,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:bottom"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "front",
          "Front",
          "planar",
          current_x_offset + parameters.width / 2.0,
          parameters.height / 2.0,
          parameters.depth,
          0.0,
          0.0,
          1.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           parameters.height / 2.0,
                           parameters.depth,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0),
          parameters.width,
          parameters.height,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:front"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "back",
          "Back",
          "planar",
          current_x_offset + parameters.width / 2.0,
          parameters.height / 2.0,
          0.0,
          0.0,
          0.0,
          -1.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           parameters.height / 2.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           -1.0),
          parameters.width,
          parameters.height,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:back"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "right",
          "Right",
          "planar",
          current_x_offset + parameters.width,
          parameters.height / 2.0,
          parameters.depth / 2.0,
          1.0,
          0.0,
          0.0,
          make_plane_frame(current_x_offset + parameters.width,
                           parameters.height / 2.0,
                           parameters.depth / 2.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           1.0,
                           0.0,
                           0.0),
          parameters.height,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:right"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "left",
          "Left",
          "planar",
          current_x_offset,
          parameters.height / 2.0,
          parameters.depth / 2.0,
          -1.0,
          0.0,
          0.0,
          make_plane_frame(current_x_offset,
                           parameters.height / 2.0,
                           parameters.depth / 2.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           -1.0,
                           0.0,
                           0.0),
          parameters.height,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:left"));

      current_x_offset += parameters.width + kBoxSpacing;
      scene_width = std::max(scene_width, current_x_offset - kBoxSpacing);
      if (parameters.height > max_height) {
        max_height = parameters.height;
      }
      if (parameters.depth > max_depth) {
        max_depth = parameters.depth;
      }
      continue;
    }

    if (feature.kind == "cylinder" && feature.cylinder_parameters.has_value()) {
      if (feature_consumed_by_boolean) {
        continue;
      }
      const auto& parameters = feature.cylinder_parameters.value();
      const double diameter = parameters.radius * 2.0;

      cylinders.push_back(ViewportCylinderPrimitive{
          .id = feature.id,
          .label = feature.name,
          .radius = parameters.radius,
          .height = parameters.height,
          .x_offset = current_x_offset,
          .center_x = current_x_offset + diameter / 2.0,
          .center_y = parameters.height / 2.0,
          .center_z = diameter / 2.0,
          .is_selected = is_selected,
          .appearance_color = body_appearance_color(*view, feature.id),
      });
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "top",
          "Top",
          "planar",
          current_x_offset + diameter / 2.0,
          parameters.height,
          diameter / 2.0,
          0.0,
          1.0,
          0.0,
          make_plane_frame(current_x_offset + diameter / 2.0,
                           parameters.height,
                           diameter / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           1.0,
                           0.0),
          diameter,
          diameter,
          parameters.radius,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:top"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "bottom",
          "Bottom",
          "planar",
          current_x_offset + diameter / 2.0,
          0.0,
          diameter / 2.0,
          0.0,
          -1.0,
          0.0,
          make_plane_frame(current_x_offset + diameter / 2.0,
                           0.0,
                           diameter / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           -1.0,
                           0.0),
          diameter,
          diameter,
          parameters.radius,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:bottom"));

      current_x_offset += diameter + kBoxSpacing;
      scene_width = std::max(scene_width, current_x_offset - kBoxSpacing);
      if (parameters.height > max_height) {
        max_height = parameters.height;
      }
      if (diameter > max_depth) {
        max_depth = diameter;
      }
      continue;
    }

    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      if (feature.dependency_broken || feature.status == "warning") {
        continue;
      }
      if (feature_consumed_by_boolean) {
        continue;
      }
      const auto& parameters = feature.extrude_parameters.value();

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
      } else if (parameters.profile_kind == "circle") {
        if (parameters.plane_id != "ref-plane-xy") {
          continue;
        }

        if (parameters.plane_id == "ref-plane-xy") {
          cylinders.push_back(ViewportCylinderPrimitive{
              .id = feature.id,
              .label = feature.name,
              .radius = parameters.radius,
              .height = parameters.depth,
              .x_offset = parameters.start_x - parameters.radius,
              .center_x = parameters.start_x,
              .center_y = parameters.depth / 2.0,
              .center_z = parameters.start_y,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          max_height = std::max(max_height, parameters.depth);
          max_depth = std::max(max_depth, parameters.start_y + parameters.radius);
          scene_width = std::max(scene_width, parameters.start_x + parameters.radius);
        } else {
          cylinders.push_back(ViewportCylinderPrimitive{
              .id = feature.id,
              .label = feature.name,
              .radius = parameters.radius,
              .height = parameters.depth,
              .x_offset = parameters.start_x - parameters.radius,
              .center_x = parameters.start_x,
              .center_y = parameters.depth / 2.0,
              .center_z = parameters.start_y,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          max_height = std::max(max_height, parameters.depth);
          max_depth = std::max(max_depth, parameters.start_y + parameters.radius);
          scene_width = std::max(scene_width, parameters.start_x + parameters.radius);
        }
        solid_faces.push_back(make_solid_face(
            feature.id,
            feature.kind,
            "top",
            "Top",
            "planar",
            parameters.start_x,
            parameters.depth,
            parameters.start_y,
            0.0,
            1.0,
            0.0,
            make_face_frame_for_plane(
                parameters.plane_id,
                parameters.start_x,
                parameters.depth,
                parameters.start_y),
            parameters.radius * 2.0,
            parameters.radius * 2.0,
            parameters.radius,
            view->selected_face_id.has_value() &&
                view->selected_face_id.value() == feature.id + ":face:top"));
      }
      continue;
    }

