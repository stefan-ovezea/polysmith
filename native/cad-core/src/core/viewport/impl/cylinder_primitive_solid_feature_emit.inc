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
