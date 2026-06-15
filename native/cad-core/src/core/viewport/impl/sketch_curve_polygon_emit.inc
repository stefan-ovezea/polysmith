      for (const auto& arc : feature.sketch_parameters->arcs) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(arc.id);
        sketch_arcs.push_back(make_sketch_arc_primitive(
            arc,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
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
          if (dimension_it != feature.sketch_parameters->dimensions.end()) {
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
          if (dim_it != feature.sketch_parameters->dimensions.end()) {
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
