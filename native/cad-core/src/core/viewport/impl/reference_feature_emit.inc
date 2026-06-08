  for (const auto& feature : view->feature_history) {
    if (feature.suppressed) {
      continue;
    }
    if (feature.kind == "construction_axis" &&
        feature.construction_axis_parameters.has_value()) {
      const auto& params = feature.construction_axis_parameters.value();
      reference_axes.push_back(ViewportReferenceAxis{
          .id = feature.id,
          .label = feature.name,
          .axis = "custom",
          .start_x = params.start_x,
          .start_y = params.start_y,
          .start_z = params.start_z,
          .end_x = params.end_x,
          .end_y = params.end_y,
          .end_z = params.end_z,
      });
      continue;
    }
    if (feature.kind == "construction_point" &&
        feature.construction_point_parameters.has_value()) {
      const auto& params = feature.construction_point_parameters.value();
      reference_points.push_back(ViewportReferencePoint{
          .id = feature.id,
          .label = feature.name,
          .x = params.x,
          .y = params.y,
          .z = params.z,
          .is_selected = view->selected_feature_id.has_value() &&
                         view->selected_feature_id.value() == feature.id,
      });
      continue;
    }
    if (feature.kind == "helix" && feature.helix_parameters.has_value() &&
        !feature.dependency_broken) {
      const auto& params = feature.helix_parameters.value();
      helices.push_back(ViewportHelixPrimitive{
          .id = feature.id,
          .label = feature.name,
          .points = params.points,
          .is_selected = view->selected_feature_id.has_value() &&
                         view->selected_feature_id.value() == feature.id,
      });
      continue;
    }
    if (feature.kind == "thread" && feature.thread_parameters.has_value() &&
        !feature.dependency_broken &&
        feature.thread_parameters->representation == "cosmetic") {
      const auto& params = feature.thread_parameters.value();
      const auto axis = resolve_construction_axis_source(*view, params.axis_source_id);
      if (axis.has_value()) {
        const double radius =
            std::max(params.major_diameter, params.minor_diameter) * 0.5 * 1.015;
        const auto points = make_cosmetic_thread_points(
            axis->start_x,
            axis->start_y,
            axis->start_z,
            axis->end_x,
            axis->end_y,
            axis->end_z,
            radius,
            params.pitch,
            params.length,
            params.start_offset,
            params.handedness != "left");
        if (!points.empty()) {
          helices.push_back(ViewportHelixPrimitive{
              .id = feature.id,
              .label = feature.name,
              .points = points,
              .is_selected = view->selected_feature_id.has_value() &&
                             view->selected_feature_id.value() == feature.id,
          });
        }
      }
      continue;
    }
    if (feature.kind == "hole" && feature.hole_parameters.has_value() &&
        !feature.dependency_broken &&
        feature.hole_parameters->thread_enabled &&
        feature.hole_parameters->thread_representation == "cosmetic" &&
        feature.hole_parameters->thread_pitch > 0.0) {
      const auto& params = feature.hole_parameters.value();
      const double start_x = params.plane_frame.origin_x +
                             params.plane_frame.x_axis_x * params.center_x +
                             params.plane_frame.y_axis_x * params.center_y;
      const double start_y = params.plane_frame.origin_y +
                             params.plane_frame.x_axis_y * params.center_x +
                             params.plane_frame.y_axis_y * params.center_y;
      const double start_z = params.plane_frame.origin_z +
                             params.plane_frame.x_axis_z * params.center_x +
                             params.plane_frame.y_axis_z * params.center_y;
      const double length = std::min(
          std::abs(params.thread_depth),
          params.extent_type == "through_all" ? std::max(10000.0, std::abs(params.depth))
                                              : std::abs(params.depth));
      const double end_x = start_x - params.plane_frame.normal_x * length;
      const double end_y = start_y - params.plane_frame.normal_y * length;
      const double end_z = start_z - params.plane_frame.normal_z * length;
      const double radius =
          std::max(params.major_diameter, params.minor_diameter) * 0.5;
      const auto points = make_cosmetic_thread_points(start_x,
                                                      start_y,
                                                      start_z,
                                                      end_x,
                                                      end_y,
                                                      end_z,
                                                      radius,
                                                      params.thread_pitch,
                                                      length,
                                                      0.0,
                                                      true);
      if (!points.empty()) {
        helices.push_back(ViewportHelixPrimitive{
            .id = feature.id + ":thread",
            .label = feature.name + " thread",
            .points = points,
            .is_selected = view->selected_feature_id.has_value() &&
                           view->selected_feature_id.value() == feature.id,
        });
      }
      continue;
    }
    if (feature.kind == "fastener" && feature.fastener_parameters.has_value() &&
        feature.fastener_parameters->thread_representation == "cosmetic") {
      const auto& params = feature.fastener_parameters.value();
      const double shaft_radius = params.diameter * 0.5;
      const double thread_length =
          std::min(std::max(params.thread_length, 0.0), params.length);
      const double pitch =
          params.pitch > 0.0 ? params.pitch : std::max(params.diameter * 0.16, 0.2);
      const auto points = make_cosmetic_thread_points(
          shaft_radius,
          0.0,
          shaft_radius,
          shaft_radius,
          params.length,
          shaft_radius,
          shaft_radius * 1.015,
          pitch,
          thread_length,
          0.0,
          true);
      if (!points.empty()) {
        helices.push_back(ViewportHelixPrimitive{
            .id = feature.id + ":thread",
            .label = feature.name + " thread",
            .points = points,
            .is_selected = view->selected_feature_id.has_value() &&
                           view->selected_feature_id.value() == feature.id,
        });
      }
    }
  }
