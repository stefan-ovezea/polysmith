// Ad-hoc enforcement for the constraint-completion kinds (symmetric,
// midpoint, collinear, tangent pairs).  These run after the solver in
// refresh_sketch_derived_state and provide a deterministic fallback
// when the solver did not run or failed; like all enforcement passes,
// they are no-ops when the solver already satisfied the constraints.

void drive_line_collinear_to_reference(SketchLine& driven_line,
                                       const SketchLine& reference_line) {
  const double rx = reference_line.end_x - reference_line.start_x;
  const double ry = reference_line.end_y - reference_line.start_y;
  const double rlen = std::hypot(rx, ry);
  if (rlen < 1e-12) return;

  const double mx = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double my = (driven_line.start_y + driven_line.end_y) / 2.0;
  const double len =
      std::hypot(driven_line.end_x - driven_line.start_x,
                 driven_line.end_y - driven_line.start_y);
  if (len < 1e-12) return;

  // Direction = reference direction; center = the driven line's midpoint
  // projected onto the reference's infinite axis.
  const double ux = rx / rlen;
  const double uy = ry / rlen;
  const double t = (mx - reference_line.start_x) * ux +
                   (my - reference_line.start_y) * uy;
  const double cx = reference_line.start_x + t * ux;
  const double cy = reference_line.start_y + t * uy;

  driven_line.start_x = cx - ux * len / 2.0;
  driven_line.start_y = cy - uy * len / 2.0;
  driven_line.end_x = cx + ux * len / 2.0;
  driven_line.end_y = cy + uy * len / 2.0;
}

void enforce_collinear_relations(SketchFeatureParameters& parameters,
                                 const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(), visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    const auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      // tangent_line_line behaves as collinear (two tangent lines share
      // a corner and continue along the same axis).
      if (relation.kind != "collinear" &&
          relation.kind != "tangent_line_line") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) continue;

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_collinear_to_reference(driven_line, reference_line);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x, driven_line.start_y,
                    driven_line.end_x, driven_line.end_y);

      if (!points_match(previous_start_x, previous_start_y,
                        driven_line.start_x, driven_line.start_y)) {
        propagate_connected_point_move(parameters, driven_line.start_vertex_id,
                                       driven_line.start_x, driven_line.start_y);
      }
      if (!points_match(previous_end_x, previous_end_y, driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters, driven_line.end_vertex_id,
                                       driven_line.end_x, driven_line.end_y);
      }

      frontier.push_back(other_line_id);
    }
  }
}

void enforce_symmetric_constraints(SketchFeatureParameters& parameters) {
  for (auto& constraint : parameters.constraints) {
    if (constraint.kind != "symmetric" || constraint.driven) continue;
    if (constraint.target_ids.size() != 3) continue;

    const std::string& p1_id = constraint.target_ids[0];
    const std::string& p2_id = constraint.target_ids[1];
    const std::string& axis_id = constraint.target_ids[2];

    auto axis_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == axis_id; });
    if (axis_it == parameters.lines.end()) {
      constraint.driven = true;
      continue;
    }

    const auto p1_pos = point_position_of(parameters, p1_id);
    const auto p2_pos = point_position_of(parameters, p2_id);
    if (!p1_pos.has_value() || !p2_pos.has_value()) {
      constraint.driven = true;
      continue;
    }

    const bool p1_fixed = vertex_is_fixed_id(parameters, p1_id);
    const bool p2_fixed = vertex_is_fixed_id(parameters, p2_id);

    const auto reflected_p1 =
        reflect_point_across_line(p1_pos->first, p1_pos->second,
                                  axis_it->start_x, axis_it->start_y,
                                  axis_it->end_x, axis_it->end_y);
    const auto reflected_p2 =
        reflect_point_across_line(p2_pos->first, p2_pos->second,
                                  axis_it->start_x, axis_it->start_y,
                                  axis_it->end_x, axis_it->end_y);
    if (points_match(p2_pos->first, p2_pos->second, reflected_p1.first,
                     reflected_p1.second)) {
      continue;  // already symmetric
    }
    if (p1_fixed && p2_fixed) {
      constraint.driven = true;
      continue;
    }
    // Compromise convention: each point moves halfway toward its mirror
    // image.  The reflection is an isometry, so a one-sided drive can
    // never be distinguished from its mirror — equal splitting keeps
    // both points live (dragging either side of the pair spreads or
    // squeezes the pair symmetrically) and never reverts a drag.
    if (p1_fixed) {
      propagate_connected_point_move(parameters, p2_id, reflected_p1.first,
                                     reflected_p1.second);
      continue;
    }
    if (p2_fixed) {
      propagate_connected_point_move(parameters, p1_id, reflected_p2.first,
                                     reflected_p2.second);
      continue;
    }
    const double mid_x = (p1_pos->first + reflected_p2.first) / 2.0;
    const double mid_y = (p1_pos->second + reflected_p2.second) / 2.0;
    const double mid2_x = (p2_pos->first + reflected_p1.first) / 2.0;
    const double mid2_y = (p2_pos->second + reflected_p1.second) / 2.0;
    propagate_connected_point_move(parameters, p1_id, mid_x, mid_y);
    propagate_connected_point_move(parameters, p2_id, mid2_x, mid2_y);
  }
}

void enforce_midpoint_constraints(SketchFeatureParameters& parameters) {
  for (auto& constraint : parameters.constraints) {
    if (constraint.kind != "midpoint" || constraint.driven) continue;
    if (constraint.target_ids.size() != 2) continue;

    const std::string& vertex_id = constraint.target_ids[0];
    const std::string& line_id = constraint.target_ids[1];

    auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == line_id; });
    if (line_it == parameters.lines.end()) {
      constraint.driven = true;
      continue;
    }

    const double mx = (line_it->start_x + line_it->end_x) / 2.0;
    const double my = (line_it->start_y + line_it->end_y) / 2.0;
    const auto pos = point_position_of(parameters, vertex_id);
    if (!pos.has_value()) {
      constraint.driven = true;
      continue;
    }
    if (points_match(pos->first, pos->second, mx, my)) continue;

    if (vertex_is_fixed_id(parameters, vertex_id)) {
      constraint.driven = true;
      continue;
    }
    propagate_connected_point_move(parameters, vertex_id, mx, my);
  }
}

void enforce_tangent_pair_constraints(SketchFeatureParameters& parameters) {
  for (auto& constraint : parameters.constraints) {
    if (constraint.driven) continue;
    if (constraint.kind != "tangent_circle_circle" &&
        constraint.kind != "tangent_arc_arc") {
      continue;
    }
    if (constraint.target_ids.size() != 2) continue;

    const auto first = curve_center_radius(parameters, constraint.target_ids[0]);
    const auto second = curve_center_radius(parameters, constraint.target_ids[1]);
    if (!first.has_value() || !second.has_value()) {
      constraint.driven = true;
      continue;
    }

    const double cx1 = std::get<0>(first.value());
    const double cy1 = std::get<1>(first.value());
    const double r1 = std::get<2>(first.value());
    const double cx2 = std::get<0>(second.value());
    const double cy2 = std::get<1>(second.value());
    const double r2 = std::get<2>(second.value());
    const bool internal = constraint.value > 0.5;
    const double target_dist = internal ? std::abs(r1 - r2) : r1 + r2;

    const double dx = cx2 - cx1;
    const double dy = cy2 - cy1;
    const double dist = std::hypot(dx, dy);
    if (std::abs(dist - target_dist) < 1e-6) continue;
    if (dist < 1e-12) {
      constraint.driven = true;
      continue;
    }

    // Move the second curve's center along the center-to-center
    // direction; when it is fixed, move the first instead.  Circle and
    // arc centers are written directly — propagate_connected_point_move
    // only walks lines and would not move a curve center.
    const bool first_is_circle =
        std::get<3>(first.value()) == "circle";
    const bool second_is_circle =
        std::get<3>(second.value()) == "circle";
    const std::string first_center = first_is_circle
        ? circle_center_vertex_id_for(parameters, constraint.target_ids[0])
        : require_arc(parameters, constraint.target_ids[0]).center_vertex_id;
    const std::string second_center = second_is_circle
        ? circle_center_vertex_id_for(parameters, constraint.target_ids[1])
        : require_arc(parameters, constraint.target_ids[1]).center_vertex_id;

    auto move_curve_center = [&](const std::string& entity_id, bool is_circle,
                                 const std::string& center_vid,
                                 double nx, double ny) {
      if (is_circle) {
        for (auto& circle : parameters.circles) {
          if (circle.id == entity_id) {
            circle.center_x = nx;
            circle.center_y = ny;
          }
        }
      } else {
        for (auto& arc : parameters.arcs) {
          if (arc.id == entity_id) {
            const double shift_x = nx - arc.center_x;
            const double shift_y = ny - arc.center_y;
            arc.center_x = nx;
            arc.center_y = ny;
            arc.start_x += shift_x;
            arc.start_y += shift_y;
            arc.end_x += shift_x;
            arc.end_y += shift_y;
          }
        }
      }
      for (auto& vertex : parameters.vertices) {
        if (vertex.id == center_vid) {
          vertex.x = nx;
          vertex.y = ny;
        }
      }
    };

    const double ux = dx / dist;
    const double uy = dy / dist;
    if (!vertex_is_fixed_id(parameters, second_center)) {
      move_curve_center(constraint.target_ids[1], second_is_circle,
                        second_center, cx1 + ux * target_dist,
                        cy1 + uy * target_dist);
    } else if (!vertex_is_fixed_id(parameters, first_center)) {
      move_curve_center(constraint.target_ids[0], first_is_circle,
                        first_center, cx2 - ux * target_dist,
                        cy2 - uy * target_dist);
    } else {
      constraint.driven = true;
    }
  }
}

void enforce_constraint_completion(SketchFeatureParameters& parameters) {
  // Symmetric / midpoint / tangent pairs are point-anchored, run them
  // first; collinear relation walking runs per line like the other
  // line-direction enforcement passes.
  enforce_symmetric_constraints(parameters);
  enforce_midpoint_constraints(parameters);
  enforce_tangent_pair_constraints(parameters);
  for (const auto& line : parameters.lines) {
    enforce_collinear_relations(parameters, line.id);
  }
}
