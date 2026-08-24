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

      set_endpoint(line, endpoint_ref.is_start, move.to_x, move.to_y);

      // For a bare endpoint drag, keep the opposite endpoint anchored
      // and clamp the moved endpoint along the H/V axis. When the
      // moved endpoint is a shared corner or an anchor target, it is
      // authoritative: carry the opposite endpoint across the same
      // axis so rectangles and anchored construction lines follow the
      // driving geometry instead of snapping back to stale coordinates.
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
        if ((moved_endpoint_anchored || moved_point_shared) &&
            !other_endpoint_fixed &&
            !other_endpoint_anchored) {
          if (moved_start) {
            line.end_y = line.start_y;
          } else if (moved_end) {
            line.start_y = line.end_y;
          }
        } else if (!other_endpoint_anchored && moved_start) {
          line.start_y = line.end_y;
        } else if (!other_endpoint_anchored && moved_end) {
          line.end_y = line.start_y;
        }
      } else if (line.constraint == "vertical") {
        if ((moved_endpoint_anchored || moved_point_shared) &&
            !other_endpoint_fixed &&
            !other_endpoint_anchored) {
          if (moved_start) {
            line.end_x = line.start_x;
          } else if (moved_end) {
            line.start_x = line.end_x;
          }
        } else if (!other_endpoint_anchored && moved_start) {
          line.start_x = line.end_x;
        } else if (!other_endpoint_anchored && moved_end) {
          line.end_x = line.start_x;
        }
      }

      validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

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
