        // Angle dimensions span two lines. Emit them once per dim
        // (rather than per line) so we don't double-render. Skip
        // silently if either referenced line is missing — that's
        // possible mid-edit when a line is being deleted.
        for (const auto& dimension :
             feature.sketch_parameters->dimensions) {
          if (dimension.kind != "angle") {
            continue;
          }
          const auto line_a_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == dimension.entity_id;
              });
          const auto line_b_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == dimension.secondary_entity_id;
              });
          if (line_a_it == feature.sketch_parameters->lines.end() ||
              line_b_it == feature.sketch_parameters->lines.end()) {
            continue;
          }
          const bool is_selected_dimension =
              view->selected_sketch_dimension_id.has_value() &&
              view->selected_sketch_dimension_id.value() == dimension.id;
          sketch_dimensions.push_back(make_angle_dimension_primitive(
              *line_a_it,
              *line_b_it,
              dimension,
              *feature.sketch_parameters,
              is_selected_dimension));
        }

        for (const auto& dimension :
             feature.sketch_parameters->dimensions) {
          const bool is_selected_dimension =
              view->selected_sketch_dimension_id.has_value() &&
              view->selected_sketch_dimension_id.value() == dimension.id;
          if (dimension.kind == "circle_center_distance") {
            const auto driven_circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& circle) {
                  return circle.id == dimension.entity_id;
                });
            const auto reference_circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& circle) {
                  return circle.id == dimension.secondary_entity_id;
                });
            if (driven_circle_it == feature.sketch_parameters->circles.end() ||
                reference_circle_it == feature.sketch_parameters->circles.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_circle_center_distance_dimension_primitive(
                    *driven_circle_it,
                    *reference_circle_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          } else if (dimension.kind == "circle_line_distance") {
            const auto circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& circle) {
                  return circle.id == dimension.entity_id;
                });
            const auto line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& line) {
                  return line.id == dimension.secondary_entity_id;
                });
            if (circle_it == feature.sketch_parameters->circles.end() ||
                line_it == feature.sketch_parameters->lines.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_circle_line_distance_dimension_primitive(
                    *circle_it,
                    *line_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          } else if (dimension.kind == "point_distance") {
            auto find_v = [&](const std::string& vid) {
              return std::find_if(
                  feature.sketch_parameters->vertices.begin(),
                  feature.sketch_parameters->vertices.end(),
                  [&](const SketchVertex& p) {
                    return p.id == vid || p.vertex_id == vid;
                  });
            };
            const auto point_a_it = find_v(dimension.entity_id);
            const auto point_b_it = find_v(dimension.secondary_entity_id);
            if (point_a_it == feature.sketch_parameters->vertices.end() ||
                point_b_it == feature.sketch_parameters->vertices.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_point_distance_dimension_primitive(
                    *point_a_it,
                    *point_b_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          } else if (dimension.kind == "line_line_distance") {
            const auto driven_line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& line) {
                  return line.id == dimension.entity_id;
                });
            const auto reference_line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& line) {
                  return line.id == dimension.secondary_entity_id;
                });
            if (driven_line_it == feature.sketch_parameters->lines.end() ||
                reference_line_it == feature.sketch_parameters->lines.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_line_line_distance_dimension_primitive(
                    *driven_line_it,
                    *reference_line_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          }
        }
      }
