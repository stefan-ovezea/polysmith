        // Mirror constraint badge: one badge per mirror relation,
        // placed at the axis midpoint. Multiple mirror operations on
        // the same axis stack with increasing offset, starting above
        // H/V constraint badges (which use offset 1.0–3.0).
        {
          std::map<std::string, int> axis_badge_count;
          for (const auto& rel : feature.sketch_parameters->mirror_relations) {
            const auto axis_line = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& l) { return l.id == rel.axis_line_id; });
            if (axis_line == feature.sketch_parameters->lines.end()) continue;

            const double mx = (axis_line->start_x + axis_line->end_x) / 2.0;
            const double my = (axis_line->start_y + axis_line->end_y) / 2.0;
            const double dx = axis_line->end_x - axis_line->start_x;
            const double dy = axis_line->end_y - axis_line->start_y;
            const double len = std::hypot(dx, dy);
            const double nx = len > 0.0 ? -dy / len : 0.0;
            const double ny = len > 0.0 ?  dx / len : 1.0;

            // Start at 3.5 to clear H/V badges (at 1.0–3.0), then stack.
            const int idx = axis_badge_count[rel.axis_line_id]++;
            const double offset = kConstraintBadgeOffset * (3.5 + idx * 0.8);

            const WorldPoint pos = to_world_point(
                feature.sketch_parameters->plane_id,
                mx + nx * offset, my + ny * offset,
                kSketchPlaneOffset + kConstraintBadgeOffset);

            const bool is_sel = is_sketch_entity_selected(rel.axis_line_id);
            sketch_constraints.push_back(ViewportSketchConstraintPrimitive{
                .constraint_id = rel.id,
                .plane_id = feature.sketch_parameters->plane_id,
                .kind = "mirror",
                .entity_id = rel.axis_line_id,
                .related_entity_id = std::nullopt,
                .label = "M",
                .is_selected = is_sel,
                .position_x = pos.x,
                .position_y = pos.y,
                .position_z = pos.z,
            });
          }
        }
