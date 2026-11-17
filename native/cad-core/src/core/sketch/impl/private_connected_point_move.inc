// True when the vertex carries an attachment to a curve (`point_on_circle` /
// `point_on_arc`), so it can only exist ON that curve. `target_ids` is
// {vertex, circle-or-arc} for both kinds. Kept here rather than with the other
// drag-policy helpers because this file sits inside the sketch's anonymous
// namespace, where the only caller (`propagate_connected_point_move`) lives.
bool vertex_is_bound_to_curve(const SketchFeatureParameters& parameters,
                              const std::string& vertex_id) {
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != "point_on_circle" &&
        constraint.kind != "point_on_arc") {
      continue;
    }
    if (!constraint.target_ids.empty() && constraint.target_ids[0] == vertex_id) {
      return true;
    }
  }
  return false;
}

void propagate_connected_point_move(SketchFeatureParameters& parameters,
                                    const std::string& point_id,
                                    double target_x,
                                    double target_y) {
  struct PendingMove {
    std::string vertex_id;
    double to_x;
    double to_y;
  };

  std::deque<PendingMove> frontier = {{
      .vertex_id = point_id,
      .to_x = target_x,
      .to_y = target_y,
  }};
  std::vector<std::string> visited_points;

  while (!frontier.empty()) {
    const auto move = frontier.front();
    frontier.pop_front();

    const auto current_point_position = find_point_position(parameters, move.vertex_id);
    if (vertex_is_fixed(parameters, move.vertex_id) &&
        current_point_position.has_value() &&
        !points_match(std::get<0>(current_point_position.value()),
                      std::get<1>(current_point_position.value()),
                      move.to_x,
                      move.to_y)) {
      continue;
    }

    const bool already_visited = std::any_of(
        visited_points.begin(),
        visited_points.end(),
        [&](const auto& visited) { return visited == move.vertex_id; });
    if (already_visited) {
      continue;
    }
    visited_points.push_back(move.vertex_id);

    // Spline poles: the move re-fits the cached pole; the curve
    // follows through the refresh sync (rebuild + vertex writeback).
    for (auto& spline : parameters.splines) {
      for (size_t i = 0; i < spline.pole_vertex_ids.size(); ++i) {
        if (spline.pole_vertex_ids[i] == move.vertex_id) {
          spline.pole_xs[i] = move.to_x;
          spline.pole_ys[i] = move.to_y;
        }
      }
    }

    std::vector<LineEndpointRef> connected_endpoints;
    for (size_t line_index = 0; line_index < parameters.lines.size(); ++line_index) {
      const auto& line = parameters.lines[line_index];
      if (line.start_vertex_id == move.vertex_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = true,
        });
      }
      if (line.end_vertex_id == move.vertex_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = false,
        });
      }
    }

    for (const auto& endpoint_ref : connected_endpoints) {
      auto& line = parameters.lines[endpoint_ref.line_index];
      const bool moved_start = endpoint_ref.is_start;
      const bool moved_end = !endpoint_ref.is_start;
      const double previous_other_x =
          moved_start ? line.end_x : line.start_x;
      const double previous_other_y =
          moved_start ? line.end_y : line.start_y;
      // The corner's own displacement, needed by the relation branch below.
      const double previous_moved_x = endpoint_x(line, endpoint_ref.is_start);
      const double previous_moved_y = endpoint_y(line, endpoint_ref.is_start);

      set_endpoint(line, endpoint_ref.is_start, move.to_x, move.to_y);

      // For a bare endpoint drag, keep the opposite endpoint anchored
      // and clamp the moved endpoint along the H/V axis. A shared corner is
      // authoritative: carry the opposite endpoint across the same axis so a
      // rectangle follows the corner instead of snapping back to stale
      // coordinates.
      //
      // A point ANCHORED to another line is a different thing: it belongs to
      // its host, not to this line, and it is expected to ride the host. The
      // far end is not the geometry being edited, so it stays where it is and
      // the line rotates. Carrying it (the old behaviour) dragged the free end
      // of a T-junction's stem along when the HOST was stretched — measured:
      // host (0,0)-(100,0) -> (0,0)-(160,0) with the stem anchored at t=0.3
      // moved the stem's free end from (30,40) to (48,40) instead of leaving
      // it and letting the stem pivot on its anchored point. The axis
      // constraint cannot survive that rotation, so it is dropped rather than
      // clamping the anchored point back off its host (a badge must never
      // outlive the geometry it claims).
      const std::string other_endpoint_point_id =
          endpoint_vertex_id(line, !endpoint_ref.is_start);
      const std::string moved_endpoint_point_id =
          endpoint_vertex_id(line, endpoint_ref.is_start);
      const bool moved_endpoint_anchored =
          vertex_is_anchored_to_line(parameters, moved_endpoint_point_id);
      const bool moved_point_shared = connected_endpoints.size() > 1;
      const bool other_endpoint_fixed =
          vertex_is_fixed(parameters, other_endpoint_point_id);
      const bool other_endpoint_anchored =
          vertex_is_anchored_to_line(parameters, other_endpoint_point_id);

      if (line.constraint == "horizontal") {
        if (moved_endpoint_anchored) {
          if (std::abs(move.to_y - previous_other_y) > kCoincidentTolerance) {
            line.constraint = std::nullopt;
          }
        } else if (moved_point_shared && !other_endpoint_fixed &&
                   !other_endpoint_anchored) {
          if (moved_start) {
            line.end_y = line.start_y;
          } else if (moved_end) {
            line.start_y = line.end_y;
          }
        } else if (!other_endpoint_fixed && !other_endpoint_anchored) {
          // The axis is an INVARIANT, so it is honoured by moving the whole line
          // rather than by clamping the dragged point: the far end takes the
          // moved point's y, which keeps the line horizontal AND lets the drag
          // work in both directions at once — down (the line travels) and along
          // (it stretches). Fusion behaves this way, which the user compared
          // directly to dragging a dimensioned line (2026-11-04).
          //
          // The old code clamped the DRAGGED point onto the far end's y instead,
          // so a diagonal drag silently lost its vertical part; and for a point
          // bound to a circle the clamp pushed it off the curve, which made the
          // sketch unsatisfiable (that is the status=2 failure measured on a
          // horizontal line on a circle).
          if (moved_start) {
            line.end_y = move.to_y;
          } else if (moved_end) {
            line.start_y = move.to_y;
          }
        } else if (!other_endpoint_anchored && moved_start) {
          // The far end cannot move (fixed): fall back to clamping.
          line.start_y = line.end_y;
        } else if (!other_endpoint_anchored && moved_end) {
          line.end_y = line.start_y;
        }
      } else if (line.constraint == "vertical") {
        if (moved_endpoint_anchored) {
          if (std::abs(move.to_x - previous_other_x) > kCoincidentTolerance) {
            line.constraint = std::nullopt;
          }
        } else if (moved_point_shared && !other_endpoint_fixed &&
                   !other_endpoint_anchored) {
          if (moved_start) {
            line.end_x = line.start_x;
          } else if (moved_end) {
            line.start_x = line.end_x;
          }
        } else if (!other_endpoint_fixed && !other_endpoint_anchored) {
          // See the horizontal branch: the line travels along the constrained
          // axis instead of the dragged point being clamped.
          if (moved_start) {
            line.end_x = move.to_x;
          } else if (moved_end) {
            line.start_x = move.to_x;
          }
        } else if (!other_endpoint_anchored && moved_start) {
          line.start_x = line.end_x;
        } else if (!other_endpoint_anchored && moved_end) {
          line.end_x = line.start_x;
        }
      } else if (line_direction_is_related(parameters, line.id) &&
                 (moved_endpoint_anchored || moved_point_shared) &&
                 !other_endpoint_fixed && !other_endpoint_anchored) {
        // The line's angle is pinned by a parallel/perpendicular relation
        // instead of by an axis constraint, so carry the far endpoint the same
        // way: translate the whole line by the part of the corner's movement
        // that is perpendicular to its own direction. That keeps both its length
        // and its angle, which is exactly what the relation asks for. Moving
        // only the shared corner would rotate the line; the relation pass would
        // then rotate it back while preserving the stretched length, and a
        // rectangle widened by 20 mm would lose height instead.
        const double direction_x = previous_other_x - previous_moved_x;
        const double direction_y = previous_other_y - previous_moved_y;
        const double direction_length =
            std::sqrt(direction_x * direction_x + direction_y * direction_y);
        if (direction_length > kMinimumSketchDimensionValue) {
          const double normal_x = -direction_y / direction_length;
          const double normal_y = direction_x / direction_length;
          const double carry =
              (move.to_x - previous_moved_x) * normal_x +
              (move.to_y - previous_moved_y) * normal_y;
          if (carry != 0.0) {
            if (moved_start) {
              line.end_x = previous_other_x + carry * normal_x;
              line.end_y = previous_other_y + carry * normal_y;
            } else {
              line.start_x = previous_other_x + carry * normal_x;
              line.start_y = previous_other_y + carry * normal_y;
            }
          }
        }
      }

      validate_line(line.start_x, line.start_y, line.end_x, line.end_y, "moving connected points");

      const double next_other_x = moved_start ? line.end_x : line.start_x;
      const double next_other_y = moved_start ? line.end_y : line.start_y;
      if (!points_match(previous_other_x,
                        previous_other_y,
                        next_other_x,
                        next_other_y)) {
        frontier.push_back({
            .vertex_id = endpoint_vertex_id(line, !endpoint_ref.is_start),
            .to_x = next_other_x,
            .to_y = next_other_y,
        });
      }
    }
  }
}
