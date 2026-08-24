      for (const auto& arc : feature.sketch_parameters->arcs) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(arc.id);
        sketch_arcs.push_back(make_sketch_arc_primitive(
            arc,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dim_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& d) {
                return d.kind == "arc_radius" && d.entity_id == arc.id;
              });
          if (dim_it != feature.sketch_parameters->dimensions.end() && !dim_it->is_auto) {
            const bool is_selected_dim =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dim_it->id;
            const WorldPoint center =
                to_world_point(*feature.sketch_parameters, arc.center_x, arc.center_y);
            const WorldPoint dim_start = to_world_point(
                *feature.sketch_parameters, arc.center_x, arc.center_y);
            // Compute arc midpoint so the leader points toward the curved
            // surface, not toward the opening between start and end.
            const double sa = std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
            const double ea = std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
            double sweep = arc.ccw
                ? std::fmod(ea - sa + 2.0 * M_PI, 2.0 * M_PI)
                : std::fmod(sa - ea + 2.0 * M_PI, 2.0 * M_PI);
            double ma = arc.ccw ? sa + sweep * 0.5 : sa - sweep * 0.5;
            const double mid_x = arc.center_x + arc.radius * std::cos(ma);
            const double mid_y = arc.center_y + arc.radius * std::sin(ma);
            const WorldPoint dim_end = to_world_point(
                *feature.sketch_parameters, mid_x, mid_y);
            const WorldPoint label = to_world_point(
                *feature.sketch_parameters,
                arc.center_x + (mid_x - arc.center_x) * 0.5,
                arc.center_y + (mid_y - arc.center_y) * 0.5);
            sketch_dimensions.push_back(ViewportSketchDimensionPrimitive{
                .dimension_id = dim_it->id,
                .plane_id = feature.sketch_parameters->plane_id,
                .kind = "arc_radius",
                .entity_id = arc.id,
                .label = "R " + format_dimension_value(dim_it->value) + " mm",
                .is_selected = is_selected_dim,
                .anchor_start_x = dim_start.x, .anchor_start_y = dim_start.y, .anchor_start_z = dim_start.z,
                .anchor_end_x = dim_end.x, .anchor_end_y = dim_end.y, .anchor_end_z = dim_end.z,
                .dimension_start_x = dim_start.x, .dimension_start_y = dim_start.y, .dimension_start_z = dim_start.z,
                .dimension_end_x = dim_end.x, .dimension_end_y = dim_end.y, .dimension_end_z = dim_end.z,
                .label_x = label.x, .label_y = label.y, .label_z = label.z,
                .driven = dim_it->driven,
                .display_as = "",
            });
          }
      // Arc-length dimension label: reuses the arc-radius leader
      // geometry with an "L" prefix at the arc midpoint.
      // arc_length label (inside the arc loop)
      for (const auto& dim : feature.sketch_parameters->dimensions) {
        if (dim.kind != "arc_length" ||
            dim.entity_id != arc.id ||
            dim.is_auto) {
          continue;
        }
        const double sa = std::atan2(arc.start_y - arc.center_y,
                                     arc.start_x - arc.center_x);
        const double ea = std::atan2(arc.end_y - arc.center_y,
                                     arc.end_x - arc.center_x);
        double sweep = arc.ccw
            ? std::fmod(ea - sa + 2.0 * M_PI, 2.0 * M_PI)
            : std::fmod(sa - ea + 2.0 * M_PI, 2.0 * M_PI);
        const double ma = arc.ccw ? sa + sweep * 0.5 : sa - sweep * 0.5;
        const double mid_x = arc.center_x + arc.radius * std::cos(ma);
        const double mid_y = arc.center_y + arc.radius * std::sin(ma);
        const WorldPoint mid =
            to_world_point(*feature.sketch_parameters, mid_x, mid_y);
        const WorldPoint center =
            to_world_point(*feature.sketch_parameters,
                           arc.center_x, arc.center_y);
        const bool is_selected_dim =
            view->selected_sketch_dimension_id.has_value() &&
            view->selected_sketch_dimension_id.value() == dim.id;
        sketch_dimensions.push_back(ViewportSketchDimensionPrimitive{
            .dimension_id = dim.id,
            .plane_id = feature.sketch_parameters->plane_id,
            .kind = "arc_length",
            .entity_id = arc.id,
            .label = "L " + format_dimension_value(dim.value) + " mm",
            .is_selected = is_selected_dim,
            .anchor_start_x = center.x, .anchor_start_y = center.y, .anchor_start_z = center.z,
            .anchor_end_x = mid.x, .anchor_end_y = mid.y, .anchor_end_z = mid.z,
            .dimension_start_x = center.x, .dimension_start_y = center.y, .dimension_start_z = center.z,
            .dimension_end_x = mid.x, .dimension_end_y = mid.y, .dimension_end_z = mid.z,
            .label_x = mid.x, .label_y = mid.y, .label_z = mid.z,
            .driven = dim.driven,
            .display_as = "",
        });
      }
        }
      }

      for (const auto& circle : feature.sketch_parameters->circles) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(circle.id);
        sketch_circles.push_back(make_sketch_circle_primitive(
            circle,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "circle_radius" &&
                       dimension.entity_id == circle.id;
              });
          if (dimension_it != feature.sketch_parameters->dimensions.end() && !dimension_it->is_auto) {
            const bool is_selected_dimension =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dimension_it->id;
            sketch_dimensions.push_back(make_circle_dimension_primitive(
                circle,
                *dimension_it,
                *feature.sketch_parameters,
                is_selected_dimension));
          }
        }
      }

      for (const auto& ellipse : feature.sketch_parameters->ellipses) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(ellipse.id);
        sketch_ellipses.push_back(make_sketch_ellipse_primitive(
            ellipse,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
      }

      for (const auto& spline : feature.sketch_parameters->splines) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(spline.id);
        sketch_splines.push_back(make_sketch_spline_primitive(
            spline,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
      }

      for (const auto& polygon : feature.sketch_parameters->polygons) {
        // Polygons are now decomposed into individual lines — the
        // lines handle the visual representation. The polygon entry
        // is kept only for selection/identification. Skip the
        // primitive to avoid double-rendering.
        // Emit polygon radius dimension
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dim_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& d) {
                return d.kind == "polygon_radius" && d.entity_id == polygon.id;
              });
          if (dim_it != feature.sketch_parameters->dimensions.end() && !dim_it->is_auto) {
            const bool is_selected_dim =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dim_it->id;
            const WorldPoint pc = to_world_point(*feature.sketch_parameters, polygon.center_x, polygon.center_y);
            const WorldPoint pd = to_world_point(*feature.sketch_parameters, polygon.center_x + polygon.radius, polygon.center_y);
            auto prim = ViewportSketchDimensionPrimitive{
                .dimension_id = dim_it->id,
                .plane_id = feature.sketch_parameters->plane_id,
                .kind = "polygon_radius",
                .entity_id = polygon.id,
                .label = "R " + format_dimension_value(dim_it->value) + " mm",
                .is_selected = is_selected_dim,
                .anchor_start_x = pc.x, .anchor_start_y = pc.y, .anchor_start_z = pc.z,
                .anchor_end_x = pd.x, .anchor_end_y = pd.y, .anchor_end_z = pd.z,
                .dimension_start_x = pc.x, .dimension_start_y = pc.y, .dimension_start_z = pc.z,
                .dimension_end_x = pd.x, .dimension_end_y = pd.y, .dimension_end_z = pd.z,
                .label_x = pd.x, .label_y = pd.y, .label_z = pd.z,
                .driven = dim_it->driven,
                .display_as = "",
            };
            if (dim_it->label_x.has_value() && dim_it->label_y.has_value()) {
              const double radius = polygon.radius;
              if (radius > 1e-6) {
                const double normal_x = 0.0;
                const double normal_y = 1.0;
                const double label_x = polygon.center_x + radius;
                const double label_y = polygon.center_y;
                const double raw_offset_x = *dim_it->label_x - label_x;
                const double raw_offset_y = *dim_it->label_y - label_y;
                const double along = raw_offset_x * normal_x + raw_offset_y * normal_y;
                const double offset_x = normal_x * along;
                const double offset_y = normal_y * along;
                const WorldPoint shifted_start = to_world_point(
                    *feature.sketch_parameters,
                    polygon.center_x + offset_x,
                    polygon.center_y + offset_y);
                const WorldPoint shifted_end = to_world_point(
                    *feature.sketch_parameters,
                    polygon.center_x + radius + offset_x,
                    polygon.center_y + offset_y);
                const WorldPoint shifted_label =
                    to_world_point(*feature.sketch_parameters, label_x + offset_x, label_y + offset_y);
                prim.dimension_start_x = shifted_start.x;
                prim.dimension_start_y = shifted_start.y;
                prim.dimension_start_z = shifted_start.z;
                prim.dimension_end_x = shifted_end.x;
                prim.dimension_end_y = shifted_end.y;
                prim.dimension_end_z = shifted_end.z;
                prim.label_x = shifted_label.x;
                prim.label_y = shifted_label.y;
                prim.label_z = shifted_label.z;
              }
            }
            sketch_dimensions.push_back(prim);
          }
        }
      }
