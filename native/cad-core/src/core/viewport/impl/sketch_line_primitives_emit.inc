      for (const auto& line : feature.sketch_parameters->lines) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(line.id);
        sketch_lines.push_back(
            make_sketch_line_primitive(line,
                                       *feature.sketch_parameters,
                                       is_selected_sketch_entity));
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "line_length" &&
                       dimension.entity_id == line.id;
              });
          if (dimension_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_dimension =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dimension_it->id;
            sketch_dimensions.push_back(make_line_dimension_primitive(
                line,
                *dimension_it,
                *feature.sketch_parameters,
                is_selected_dimension));
          }

          const auto angle_dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "line_angle" &&
                       dimension.entity_id == line.id;
              });
          if (angle_dimension_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_angle_dim =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() ==
                    angle_dimension_it->id;
            sketch_dimensions.push_back(make_line_angle_dimension_primitive(
                line,
                *angle_dimension_it,
                *feature.sketch_parameters,
                is_selected_angle_dim));
          }

          if (line.constraint.has_value()) {
            const bool has_relation =
                lines_with_relation_badge.find(line.id) !=
                lines_with_relation_badge.end();
            sketch_constraints.push_back(make_line_constraint_primitive(
                line,
                feature.sketch_parameters->plane_id,
                line.constraint.value(),
                line.constraint.value() == "horizontal" ? "H" : "V",
                is_selected_sketch_entity,
                /*related_entity_id=*/std::nullopt,
                /*badge_offset_multiplier=*/has_relation ? 3.0 : 1.0));
          }
        }
      }
