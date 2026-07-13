        // Tangent (line ↔ circle) badge. Same line-mounted "T" glyph
        // rendered at the line's midpoint as the other line-line
        // relations. We don't put a matching badge on the circle —
        // the relation only drives the line's end, so attaching the
        // marker to the line keeps the affordance visually tied to
        // the entity that actually moves.
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          if (relation.kind != "tangent_line_circle") {
            continue;
          }

          const auto line_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == relation.first_line_id;
              });
          if (line_it == feature.sketch_parameters->lines.end()) {
            continue;
          }

          const bool is_selected =
              is_sketch_entity_selected(line_it->id);
          sketch_constraints.push_back(make_line_constraint_primitive(
              *line_it,
              feature.sketch_parameters->plane_id,
              relation.kind,
              "T",
              is_selected,
              relation.second_line_id));
        }

        // Midpoint anchors render a small "M" badge at the host
        // line's midpoint so the user sees that the bound point is
        // tracking the line's midpoint (CAD convention).
        for (const auto& anchor : feature.sketch_parameters->midpoint_anchors) {
          const auto host_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) { return line.id == anchor.line_id; });
          if (host_it == feature.sketch_parameters->lines.end()) {
            continue;
          }
          const bool is_selected =
              is_sketch_entity_selected(anchor.vertex_id);
          sketch_constraints.push_back(make_line_constraint_primitive(
              *host_it,
              feature.sketch_parameters->plane_id,
              "midpoint",
              "M",
              is_selected,
              anchor.vertex_id));
        }

        // Point-line anchors render a "/" badge at the anchor's
        // parametric position along the host line. We can't reuse
        // `make_line_constraint_primitive` (it always places the
        // badge at the line's midpoint), so we build the primitive
        // inline here, mirroring the same offset / normal math.
        for (const auto& anchor :
             feature.sketch_parameters->point_line_anchors) {
          const auto host_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) { return line.id == anchor.line_id; });
          if (host_it == feature.sketch_parameters->lines.end()) {
            continue;
          }
          const double dx = host_it->end_x - host_it->start_x;
          const double dy = host_it->end_y - host_it->start_y;
          const double length = std::sqrt(dx * dx + dy * dy);
          const double normal_x = length > 0.0 ? -dy / length : 0.0;
          const double normal_y = length > 0.0 ? dx / length : 1.0;
          const double anchor_x = host_it->start_x + anchor.t * dx;
          const double anchor_y = host_it->start_y + anchor.t * dy;
          const WorldPoint position = to_world_point(
              feature.sketch_parameters->plane_id,
              anchor_x + normal_x * kConstraintBadgeOffset,
              anchor_y + normal_y * kConstraintBadgeOffset);
          const bool is_selected =
              is_sketch_entity_selected(anchor.vertex_id);
          sketch_constraints.push_back(ViewportSketchConstraintPrimitive{
              .constraint_id = "constraint-on_line-" + anchor.vertex_id,
              .plane_id = feature.sketch_parameters->plane_id,
              .kind = "on_line",
              .entity_id = host_it->id,
              .related_entity_id = anchor.vertex_id,
              .label = "/",
              .is_selected = is_selected,
              .position_x = position.x,
              .position_y = position.y,
              .position_z = position.z,
          });
        }
