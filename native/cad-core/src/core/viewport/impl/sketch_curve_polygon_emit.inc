      for (const auto& arc : feature.sketch_parameters->arcs) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(arc.id);
        sketch_arcs.push_back(make_sketch_arc_primitive(
            arc,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
        if (!view->active_sketch_feature_id.has_value() ||
            view->active_sketch_feature_id.value() != feature.id) {
          continue;
        }
        // Radial dimensions on this arc. Each kind owns an emitter in
        // sketch_radial_dimension_primitives.inc, which resolves the
        // stored sketch-local label override so a dragged label survives
        // the next viewport refetch.
        for (const auto& dim : feature.sketch_parameters->dimensions) {
          if (dim.entity_id != arc.id || dim.is_auto) {
            continue;
          }
          const bool is_selected_dim =
              view->selected_sketch_dimension_id.has_value() &&
              view->selected_sketch_dimension_id.value() == dim.id;
          if (dim.kind == "arc_radius") {
            sketch_dimensions.push_back(make_arc_radius_dimension_primitive(
                arc, dim, *feature.sketch_parameters, is_selected_dim));
          } else if (dim.kind == "arc_length") {
            sketch_dimensions.push_back(make_arc_length_dimension_primitive(
                arc, dim, *feature.sketch_parameters, is_selected_dim));
          } else if (dim.kind == "arc_angle") {
            sketch_dimensions.push_back(make_arc_angle_dimension_primitive(
                arc, dim, *feature.sketch_parameters, is_selected_dim));
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
          // As above: an explicit dimension must win over an auto one
          // rather than being masked by it.
          const auto dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "circle_radius" &&
                       dimension.entity_id == circle.id &&
                       !dimension.is_auto;
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
          // Skip auto dimensions in the predicate rather than after it:
          // an entity can carry both an auto dimension and an explicit
          // one, and matching the auto one first hid the explicit one
          // entirely.
          const auto dim_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& d) {
                return d.kind == "polygon_radius" &&
                       d.entity_id == polygon.id && !d.is_auto;
              });
          if (dim_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_dim =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dim_it->id;
            sketch_dimensions.push_back(
                make_polygon_radius_dimension_primitive(
                    polygon, *dim_it, *feature.sketch_parameters,
                    is_selected_dim));
          }
        }
      }
