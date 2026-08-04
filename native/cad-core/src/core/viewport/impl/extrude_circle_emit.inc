      } else if (parameters.profile_kind == "circle") {
        if (parameters.plane_id != "ref-plane-xz") {
          continue;
        }

        if (parameters.plane_id == "ref-plane-xz") {
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
