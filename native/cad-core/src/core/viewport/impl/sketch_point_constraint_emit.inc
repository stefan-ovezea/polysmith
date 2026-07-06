      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& point : feature.sketch_parameters->points) {
          const bool is_selected_point =
              is_sketch_point_selected(point.id);
          sketch_vertices.push_back(make_sketch_point_primitive(
              point, *feature.sketch_parameters, is_selected_point));
          if (point.is_fixed && point.kind != "projected") {
            sketch_constraints.push_back(make_point_constraint_primitive(
                point, *feature.sketch_parameters, is_selected_point));
          }
        }

        // Emit badge primitives for explicit coincident and concentric
        // constraints stored in the sketch parameters.
        for (const auto& c : feature.sketch_parameters->constraints) {
          if (c.kind == "coincident") {
            // Find the shared point position from the lines listed in
            // target_ids. After the point merge, all lines share one
            // endpoint; find a point ID that appears in at least two.
            std::string shared_point_id;
            for (const auto& tid : c.target_ids) {
              for (const auto& line : feature.sketch_parameters->lines) {
                if (line.id != tid) continue;
                const std::string& sp = line.start_point_id;
                const std::string& ep = line.end_point_id;
                for (const auto& tid2 : c.target_ids) {
                  if (tid2 == tid) continue;
                  for (const auto& line2 : feature.sketch_parameters->lines) {
                    if (line2.id != tid2) continue;
                    if (line2.start_point_id == sp || line2.end_point_id == sp) {
                      shared_point_id = sp; break;
                    }
                    if (line2.start_point_id == ep || line2.end_point_id == ep) {
                      shared_point_id = ep; break;
                    }
                  }
                  if (!shared_point_id.empty()) break;
                }
                if (!shared_point_id.empty()) break;
              }
              if (!shared_point_id.empty()) break;
            }
            if (!shared_point_id.empty()) {
              const auto pt_pos = find_point_position(
                  *feature.sketch_parameters, shared_point_id);
              if (pt_pos.has_value()) {
                const WorldPoint pos = to_world_point(
                    *feature.sketch_parameters,
                    std::get<0>(pt_pos.value()),
                    std::get<1>(pt_pos.value()),
                    kSketchPlaneOffset + kConstraintBadgeOffset);
                const bool is_sel =
                    is_sketch_point_selected(shared_point_id);
                sketch_constraints.push_back(
                    ViewportSketchConstraintPrimitive{
                        .constraint_id = c.constraint_id,
                        .plane_id = feature.sketch_parameters->plane_id,
                        .kind = "coincident",
                        .entity_id = c.constraint_id,
                        .related_entity_id = std::nullopt,
                        .label = "∞",
                        .is_selected = is_sel,
                        .position_x = pos.x,
                        .position_y = pos.y,
                        .position_z = pos.z,
                    });
              }
            }
          }
          if (c.kind == "concentric") {
            // Target IDs are circle IDs; place badge at the first
            // circle's center.
            if (!c.target_ids.empty()) {
              const auto& circ_id = c.target_ids[0];
              for (const auto& circ : feature.sketch_parameters->circles) {
                if (circ.id == circ_id) {
                  const WorldPoint pos = to_world_point(
                      *feature.sketch_parameters,
                      circ.center_x, circ.center_y,
                      kSketchPlaneOffset + kConstraintBadgeOffset);
                  const bool is_sel =
                      is_sketch_point_selected(
                          "point-circle-" + circ_id + "-center");
                  sketch_constraints.push_back(
                      ViewportSketchConstraintPrimitive{
                          .constraint_id = c.constraint_id,
                          .plane_id = feature.sketch_parameters->plane_id,
                          .kind = "concentric",
                          .entity_id = circ_id,
                          .related_entity_id = std::nullopt,
                          .label = "◎",
                          .is_selected = is_sel,
                          .position_x = pos.x,
                          .position_y = pos.y,
                          .position_z = pos.z,
                      });
                  break;
                }
              }
            }
          }
        }
      }
