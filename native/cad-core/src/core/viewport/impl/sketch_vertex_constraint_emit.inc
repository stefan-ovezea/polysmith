      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& point : feature.sketch_parameters->vertices) {
          // Text glyph vertices are derived geometry owned by the text
          // entity — render neither their point spheres nor FIX badges
          // (the glyph lines carry the visual; a marker per glyph
          // corner would drown the sketch). They stay in the params so
          // the solver keeps treating them as fixed.
          if (point.id.rfind("vertex-text-", 0) == 0) {
            continue;
          }
          // Projection-derived vertices are owned by the projection
          // record — render the projected lines, not a sphere per
          // endpoint, and no FIX badge (they're fixed by definition).
          // A mesh projection can emit hundreds of vertices; the
          // spheres + badges read as a field of "highlighted" points.
          const bool is_projection_vertex =
              projection_vertex_ids.count(point.id) > 0;
          if (is_projection_vertex) {
            continue;
          }
          const bool is_selected_point =
              is_sketch_vertex_selected(point.id);
          sketch_vertices.push_back(make_sketch_vertex_primitive(
              point, *feature.sketch_parameters, is_selected_point));
          // The "FIX" badge renders only for points with an EXPLICIT
          // fixed constraint record (the Fix tool). Internally frozen
          // points — trim split points, slot-generated vertices,
          // ellipse axis points — keep is_fixed so the solver leaves
          // them alone, but get no badge: they are bookkeeping, not
          // user intent, and a badge per junction cluttered the
          // viewport.
          if (point.is_fixed && point.kind != "projected") {
            bool has_explicit_fix = false;
            for (const auto& c : feature.sketch_parameters->constraints) {
              if (c.kind != "fixed") continue;
              for (const auto& tid : c.target_ids) {
                if (tid == point.id) {
                  has_explicit_fix = true;
                  break;
                }
              }
              if (has_explicit_fix) break;
            }
            if (has_explicit_fix) {
              sketch_constraints.push_back(make_point_constraint_primitive(
                  point, *feature.sketch_parameters, is_selected_point));
            }
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
                const std::string& sp = line.start_vertex_id;
                const std::string& ep = line.end_vertex_id;
                for (const auto& tid2 : c.target_ids) {
                  if (tid2 == tid) continue;
                  for (const auto& line2 : feature.sketch_parameters->lines) {
                    if (line2.id != tid2) continue;
                    if (line2.start_vertex_id == sp || line2.end_vertex_id == sp) {
                      shared_point_id = sp; break;
                    }
                    if (line2.start_vertex_id == ep || line2.end_vertex_id == ep) {
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
                    is_sketch_vertex_selected(shared_point_id);
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
                      is_sketch_vertex_selected(
                          circle_center_vertex_id(circ));
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
          if (c.kind == "symmetric") {
            // S1: two points mirrored about an axis line. Badge at the middle
            // of the point pair, on the axis line's side of the sketch.
            if (c.target_ids.size() == 3) {
              const auto position_of = [&](const std::string& id)
                  -> std::optional<std::pair<double, double>> {
                const auto it = std::find_if(
                    feature.sketch_parameters->vertices.begin(),
                    feature.sketch_parameters->vertices.end(),
                    [&](const SketchVertex& vertex) { return vertex.id == id; });
                if (it == feature.sketch_parameters->vertices.end()) {
                  return std::nullopt;
                }
                return std::make_pair(it->x, it->y);
              };
              const auto first = position_of(c.target_ids[0]);
              const auto second = position_of(c.target_ids[1]);
              if (first.has_value() && second.has_value()) {
                const WorldPoint pos = to_world_point(
                    *feature.sketch_parameters,
                    (first->first + second->first) / 2.0,
                    (first->second + second->second) / 2.0,
                    kSketchPlaneOffset + kConstraintBadgeOffset);
                const bool is_sel =
                    is_sketch_vertex_selected(c.target_ids[0]) ||
                    is_sketch_vertex_selected(c.target_ids[1]) ||
                    is_sketch_entity_selected(c.target_ids[2]);
                sketch_constraints.push_back(
                    ViewportSketchConstraintPrimitive{
                        .constraint_id = c.constraint_id,
                        .plane_id = feature.sketch_parameters->plane_id,
                        .kind = "symmetric",
                        .entity_id = c.target_ids[2],
                        .related_entity_id = c.target_ids[0],
                        .label = "S",
                        .is_selected = is_sel,
                        .position_x = pos.x,
                        .position_y = pos.y,
                        .position_z = pos.z,
                    });
              }
            }
          }
          if (c.kind == "tangent_circle_circle" || c.kind == "tangent_arc_arc") {
            // S1: curve-pair tangency. Badge at the touch point, on the line
            // joining the two centres at the first curve's radius.
            if (c.target_ids.size() == 2) {
              const auto centre_radius = [&](const std::string& id)
                  -> std::optional<std::pair<std::pair<double, double>, double>> {
                for (const auto& circle : feature.sketch_parameters->circles) {
                  if (circle.id == id) {
                    return std::make_pair(
                        std::make_pair(circle.center_x, circle.center_y),
                        circle.radius);
                  }
                }
                for (const auto& arc : feature.sketch_parameters->arcs) {
                  if (arc.id == id) {
                    return std::make_pair(
                        std::make_pair(arc.center_x, arc.center_y), arc.radius);
                  }
                }
                return std::nullopt;
              };
              const auto first = centre_radius(c.target_ids[0]);
              const auto second = centre_radius(c.target_ids[1]);
              if (first.has_value() && second.has_value()) {
                const double dx = second->first.first - first->first.first;
                const double dy = second->first.second - first->first.second;
                const double dist = std::hypot(dx, dy);
                const double ux = dist > 1e-12 ? dx / dist : 1.0;
                const double uy = dist > 1e-12 ? dy / dist : 0.0;
                const WorldPoint pos = to_world_point(
                    *feature.sketch_parameters,
                    first->first.first + ux * first->second,
                    first->first.second + uy * first->second,
                    kSketchPlaneOffset + kConstraintBadgeOffset);
                const bool is_sel =
                    is_sketch_entity_selected(c.target_ids[0]) ||
                    is_sketch_entity_selected(c.target_ids[1]);
                sketch_constraints.push_back(
                    ViewportSketchConstraintPrimitive{
                        .constraint_id = c.constraint_id,
                        .plane_id = feature.sketch_parameters->plane_id,
                        .kind = c.kind,
                        .entity_id = c.target_ids[0],
                        .related_entity_id = c.target_ids[1],
                        .label = "T",
                        .is_selected = is_sel,
                        .position_x = pos.x,
                        .position_y = pos.y,
                        .position_z = pos.z,
                    });
              }
            }
          }
          if (c.kind == "midpoint") {
            // S1: midpoint constraints used to constrain the sketch without
            // any badge, so an inferred anchor was invisible. target_ids is
            // {vertex_id, line_id}; badge the HOST LINE at its midpoint.
            if (c.target_ids.size() == 2) {
              const auto& line_it = std::find_if(
                  feature.sketch_parameters->lines.begin(),
                  feature.sketch_parameters->lines.end(),
                  [&](const SketchLine& line) {
                    return line.id == c.target_ids[1];
                  });
              if (line_it != feature.sketch_parameters->lines.end()) {
                const WorldPoint pos = to_world_point(
                    *feature.sketch_parameters,
                    (line_it->start_x + line_it->end_x) / 2.0,
                    (line_it->start_y + line_it->end_y) / 2.0,
                    kSketchPlaneOffset + kConstraintBadgeOffset);
                const bool is_sel =
                    is_sketch_entity_selected(line_it->id) ||
                    is_sketch_vertex_selected(c.target_ids[0]);
                sketch_constraints.push_back(
                    ViewportSketchConstraintPrimitive{
                        .constraint_id = c.constraint_id,
                        .plane_id = feature.sketch_parameters->plane_id,
                        .kind = "midpoint",
                        .entity_id = line_it->id,
                        .related_entity_id = c.target_ids[0],
                        .label = "M",
                        .is_selected = is_sel,
                        .position_x = pos.x,
                        .position_y = pos.y,
                        .position_z = pos.z,
                    });
              }
            }
          }
        }
      }
