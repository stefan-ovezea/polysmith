    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      const auto profiles = detect_sketch_profiles(feature);

      // Emit Mirror tool preview geometry first so the rest of
      // the per-line bookkeeping (constraint badges, dimensions,
      // anchors) doesn't run for transient entities — they're
      // pure visual previews. The flag flips them to a dashed
      // translucent style on the UI side.
      if (feature.sketch_parameters->pending_mirror.has_value()) {
        for (const auto& preview_line :
             feature.sketch_parameters->pending_mirror->generated_lines) {
          auto primitive = make_sketch_line_primitive(
              preview_line, *feature.sketch_parameters, /*is_selected=*/false);
          primitive.is_preview = true;
          sketch_lines.push_back(primitive);
        }
        for (const auto& preview_circle :
             feature.sketch_parameters->pending_mirror->generated_circles) {
          auto primitive = make_sketch_circle_primitive(
              preview_circle,
              *feature.sketch_parameters,
              /*is_selected=*/false);
          primitive.is_preview = true;
          sketch_circles.push_back(primitive);
        }
      }

      const auto is_sketch_entity_selected = [&](const std::string& id) {
        if (!view->selected_sketch_entity_ids.empty()) {
          return std::find(view->selected_sketch_entity_ids.begin(),
                           view->selected_sketch_entity_ids.end(),
                           id) != view->selected_sketch_entity_ids.end();
        }
        return view->selected_sketch_entity_id.has_value() &&
               view->selected_sketch_entity_id.value() == id;
      };
      const auto is_sketch_point_selected = [&](const std::string& id) {
        if (!view->selected_sketch_point_ids.empty()) {
          return std::find(view->selected_sketch_point_ids.begin(),
                           view->selected_sketch_point_ids.end(),
                           id) != view->selected_sketch_point_ids.end();
        }
        return view->selected_sketch_point_id.has_value() &&
               view->selected_sketch_point_id.value() == id;
      };
      // Track which lines carry at least one relation badge so we
      // can offset the H/V badge to avoid overlap.
      std::unordered_set<std::string> lines_with_relation_badge;
      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          lines_with_relation_badge.insert(relation.first_line_id);
          if (!relation.second_line_id.empty()) {
            lines_with_relation_badge.insert(relation.second_line_id);
          }
        }
      }

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

      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& relation : feature.sketch_parameters->line_relations) {
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
              is_sketch_entity_selected(anchor.point_id);
          sketch_constraints.push_back(make_line_constraint_primitive(
              *host_it,
              feature.sketch_parameters->plane_id,
              "midpoint",
              "M",
              is_selected,
              anchor.point_id));
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
              is_sketch_entity_selected(anchor.point_id);
          sketch_constraints.push_back(ViewportSketchConstraintPrimitive{
              .constraint_id = "constraint-on_line-" + anchor.point_id,
              .plane_id = feature.sketch_parameters->plane_id,
              .kind = "on_line",
              .entity_id = host_it->id,
              .related_entity_id = anchor.point_id,
              .label = "/",
              .is_selected = is_selected,
              .position_x = position.x,
              .position_y = position.y,
              .position_z = position.z,
          });
        }

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
            const auto point_a_it = std::find_if(
                feature.sketch_parameters->points.begin(),
                feature.sketch_parameters->points.end(),
                [&](const SketchPoint& p) {
                  return p.id == dimension.entity_id;
                });
            const auto point_b_it = std::find_if(
                feature.sketch_parameters->points.begin(),
                feature.sketch_parameters->points.end(),
                [&](const SketchPoint& p) {
                  return p.id == dimension.secondary_entity_id;
                });
            if (point_a_it == feature.sketch_parameters->points.end() ||
                point_b_it == feature.sketch_parameters->points.end()) {
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
            sketch_dimensions.push_back(ViewportSketchDimensionPrimitive{
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
            });
          }
        }
      }

      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& point : feature.sketch_parameters->points) {
          const bool is_selected_point =
              is_sketch_point_selected(point.id);
          sketch_points.push_back(make_sketch_point_primitive(
              point, *feature.sketch_parameters, is_selected_point));
          if (point.is_fixed) {
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

      for (const auto& rectangle : profiles.polygons) {
        const bool is_selected_profile =
            std::find(view->selected_sketch_profile_ids.begin(),
                      view->selected_sketch_profile_ids.end(),
                      rectangle.id) != view->selected_sketch_profile_ids.end();
        sketch_profiles.push_back(
            make_rectangle_profile_primitive(rectangle, is_selected_profile));
      }

      for (const auto& circle_profile : profiles.circles) {
        const bool is_selected_profile =
            std::find(view->selected_sketch_profile_ids.begin(),
                      view->selected_sketch_profile_ids.end(),
                      circle_profile.id) !=
            view->selected_sketch_profile_ids.end();
        sketch_profiles.push_back(
            make_circle_profile_primitive(circle_profile, is_selected_profile));
      }
    }
  }
