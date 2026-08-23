      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          if (relation.kind == "tangent_circle_line") {
            // Circle-slave tangent (circle tool modes): badge at the
            // contact point on the circle nearest the defining line.
            const auto circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& c) {
                  return c.id == relation.first_line_id;
                });
            const auto line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& l) {
                  return l.id == relation.second_line_id;
                });
            if (circle_it == feature.sketch_parameters->circles.end() ||
                line_it == feature.sketch_parameters->lines.end()) {
              continue;
            }
            const double abx = line_it->end_x - line_it->start_x;
            const double aby = line_it->end_y - line_it->start_y;
            const double len = std::hypot(abx, aby);
            if (len <= 1e-9) {
              continue;
            }
            // Unit normal from the line toward the circle center.
            double nx = -aby / len;
            double ny = abx / len;
            const double dx = circle_it->center_x -
                              (line_it->start_x + line_it->end_x) / 2.0;
            const double dy = circle_it->center_y -
                              (line_it->start_y + line_it->end_y) / 2.0;
            if (dx * nx + dy * ny < 0.0) {
              nx = -nx;
              ny = -ny;
            }
            const double cx = circle_it->center_x + nx * circle_it->radius;
            const double cy = circle_it->center_y + ny * circle_it->radius;
            const bool is_selected =
                is_sketch_entity_selected(circle_it->id) ||
                is_sketch_entity_selected(line_it->id);
            const WorldPoint pos = to_world_point(
                feature.sketch_parameters->plane_id, cx, cy,
                kSketchPlaneOffset + kConstraintBadgeOffset);
            sketch_constraints.push_back(ViewportSketchConstraintPrimitive{
                .constraint_id = "constraint-" + relation.kind + "-" +
                                 relation.first_line_id + "-" +
                                 relation.second_line_id,
                .plane_id = feature.sketch_parameters->plane_id,
                .kind = relation.kind,
                .entity_id = relation.first_line_id,
                .related_entity_id = relation.second_line_id,
                .label = "T",
                .is_selected = is_selected,
                .position_x = pos.x,
                .position_y = pos.y,
                .position_z = pos.z,
            });
            continue;
          }
          if (relation.kind != "equal_length" &&
              relation.kind != "perpendicular" &&
              relation.kind != "parallel") {
            continue;
          }

          const auto first_line_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) { return line.id == relation.first_line_id; });
          const auto second_line_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == relation.second_line_id;
              });

          if (first_line_it == feature.sketch_parameters->lines.end() ||
              second_line_it == feature.sketch_parameters->lines.end()) {
            continue;
          }

          const auto& first_line = *first_line_it;
          const auto& second_line = *second_line_it;
          const bool is_selected =
              is_sketch_entity_selected(first_line.id) ||
              is_sketch_entity_selected(second_line.id);

          const std::string label =
              relation.kind == "equal_length"   ? "="
              : relation.kind == "perpendicular" ? "P"
              : relation.kind == "parallel"     ? "//"
              : "?";

          // Determine the badge position based on constraint kind.
          double badge_sketch_x = 0.0;
          double badge_sketch_y = 0.0;

          if (relation.kind == "perpendicular") {
            // Place at the shared corner. Find the endpoint that both
            // lines share (matching point IDs or nearly-equal coords).
            const double eps = 1e-6;
            double cx = 0.0, cy = 0.0;
            bool found = false;
            const std::array<std::pair<double, double>, 2> a_pts = {{
                {first_line.start_x, first_line.start_y},
                {first_line.end_x, first_line.end_y}}};
            const std::array<std::pair<double, double>, 2> b_pts = {{
                {second_line.start_x, second_line.start_y},
                {second_line.end_x, second_line.end_y}}};
            for (const auto& ap : a_pts) {
              for (const auto& bp : b_pts) {
                if (std::abs(ap.first - bp.first) < eps &&
                    std::abs(ap.second - bp.second) < eps) {
                  cx = (ap.first + bp.first) / 2.0;
                  cy = (ap.second + bp.second) / 2.0;
                  found = true;
                  break;
                }
              }
              if (found) break;
            }
            if (found) {
              // Offset along the angle bisector so the badge sits
              // outside the corner.
              const double dx1 = first_line.end_x - first_line.start_x;
              const double dy1 = first_line.end_y - first_line.start_y;
              const double dx2 = second_line.end_x - second_line.start_x;
              const double dy2 = second_line.end_y - second_line.start_y;
              const double len1 = std::hypot(dx1, dy1);
              const double len2 = std::hypot(dx2, dy2);
              double nx = 0.0, ny = 0.0;
              if (len1 > 0.0) { nx += -dy1 / len1; ny += dx1 / len1; }
              if (len2 > 0.0) { nx += -dy2 / len2; ny += dx2 / len2; }
              const double bn = std::hypot(nx, ny);
              if (bn > 0.0) { nx /= bn; ny /= bn; }
              badge_sketch_x = cx + nx * kConstraintBadgeOffset * 1.5;
              badge_sketch_y = cy + ny * kConstraintBadgeOffset * 1.5;
            } else {
              // Fallback: no shared corner found — use first line midpoint.
              badge_sketch_x = (first_line.start_x + first_line.end_x) / 2.0;
              badge_sketch_y = (first_line.start_y + first_line.end_y) / 2.0;
            }
          } else {
            // parallel / equal_length: place at the first line's midpoint,
            // offset outward along its normal so the badge is clearly tied
            // to that line.
            const double dx = first_line.end_x - first_line.start_x;
            const double dy = first_line.end_y - first_line.start_y;
            const double len = std::hypot(dx, dy);
            const double nx = len > 0.0 ? -dy / len : 0.0;
            const double ny = len > 0.0 ?  dx / len : 1.0;
            badge_sketch_x = (first_line.start_x + first_line.end_x) / 2.0 +
                             nx * kConstraintBadgeOffset;
            badge_sketch_y = (first_line.start_y + first_line.end_y) / 2.0 +
                             ny * kConstraintBadgeOffset;
          }

          const WorldPoint pos = to_world_point(
              feature.sketch_parameters->plane_id,
              badge_sketch_x, badge_sketch_y,
              kSketchPlaneOffset + kConstraintBadgeOffset);

          sketch_constraints.push_back(ViewportSketchConstraintPrimitive{
              .constraint_id = "constraint-" + relation.kind + "-" +
                               relation.first_line_id + "-" +
                               relation.second_line_id,
              .plane_id = feature.sketch_parameters->plane_id,
              .kind = relation.kind,
              .entity_id = relation.first_line_id,
              .related_entity_id = relation.second_line_id,
              .label = label,
              .is_selected = is_selected,
              .position_x = pos.x,
              .position_y = pos.y,
              .position_z = pos.z,
          });
        }
