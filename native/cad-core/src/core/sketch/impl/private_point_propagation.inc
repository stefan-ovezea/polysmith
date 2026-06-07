std::vector<std::string> collect_line_ids_for_point(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  std::vector<std::string> line_ids;

  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id || line.end_point_id == point_id) {
      line_ids.push_back(line.id);
    }
  }

  return line_ids;
}

void replace_point_id_references(SketchFeatureParameters& parameters,
                                 const std::string& from_point_id,
                                 const std::string& to_point_id) {
  if (from_point_id == to_point_id) {
    return;
  }

  const bool merged_fixed =
      point_is_fixed(parameters, from_point_id) || point_is_fixed(parameters, to_point_id);

  for (auto& line : parameters.lines) {
    if (line.start_point_id == from_point_id) {
      line.start_point_id = to_point_id;
    }
    if (line.end_point_id == from_point_id) {
      line.end_point_id = to_point_id;
    }
  }

  if (SketchPoint* target_point = find_sketch_point(parameters, to_point_id)) {
    target_point->is_fixed = merged_fixed;
  }
}

void set_endpoint(SketchLine& line, bool is_start, double x, double y) {
  if (is_start) {
    line.start_x = x;
    line.start_y = y;
    return;
  }

  line.end_x = x;
  line.end_y = y;
}

void set_endpoint_with_constraint(SketchLine& line,
                                  bool is_start,
                                  double x,
                                  double y) {
  set_endpoint(line, is_start, x, y);

  if (line.constraint == "horizontal") {
    if (is_start) {
      line.end_y = y;
    } else {
      line.start_y = y;
    }
    return;
  }

  if (line.constraint == "vertical") {
    if (is_start) {
      line.end_x = x;
    } else {
      line.start_x = x;
    }
  }
}

void restore_fixed_line_endpoints(SketchFeatureParameters& parameters,
                                  SketchLine& line,
                                  double previous_start_x,
                                  double previous_start_y,
                                  double previous_end_x,
                                  double previous_end_y) {
  if (point_is_fixed(parameters, line.start_point_id)) {
    line.start_x = previous_start_x;
    line.start_y = previous_start_y;
  }

  if (point_is_fixed(parameters, line.end_point_id)) {
    line.end_x = previous_end_x;
    line.end_y = previous_end_y;
  }
}

void snap_line_endpoints_to_coincident_geometry(
    SketchFeatureParameters& parameters,
    SketchLine& line,
    bool snap_start = true) {
  if (snap_start) {
    const auto snapped_start =
        find_coincident_endpoint(parameters, line.id, line.start_x, line.start_y);
    if (snapped_start.has_value() &&
        !point_is_fixed(parameters, line.start_point_id)) {
      const bool can_snap_start =
          !((line.constraint == "horizontal" &&
             point_is_fixed(parameters, line.end_point_id) &&
             !nearly_equal(std::get<2>(snapped_start.value()), line.end_y)) ||
            (line.constraint == "vertical" &&
             point_is_fixed(parameters, line.end_point_id) &&
             !nearly_equal(std::get<1>(snapped_start.value()), line.end_x)));
      if (can_snap_start) {
        const std::string& snapped_point_id = std::get<0>(snapped_start.value());
        const std::string current_point_id = line.start_point_id;
        if (!current_point_id.empty() && current_point_id != snapped_point_id) {
          replace_point_id_references(parameters, current_point_id, snapped_point_id);
        }
        line.start_point_id = snapped_point_id;
        set_endpoint_with_constraint(line,
                                     true,
                                     std::get<1>(snapped_start.value()),
                                     std::get<2>(snapped_start.value()));
      }
    }
  }

  const auto snapped_end =
      find_coincident_endpoint(parameters, line.id, line.end_x, line.end_y);
  if (snapped_end.has_value() &&
      !point_is_fixed(parameters, line.end_point_id)) {
    const bool can_snap_end =
        !((line.constraint == "horizontal" &&
           point_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<2>(snapped_end.value()), line.start_y)) ||
          (line.constraint == "vertical" &&
           point_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<1>(snapped_end.value()), line.start_x)));
    if (can_snap_end) {
      const std::string& snapped_point_id = std::get<0>(snapped_end.value());
      const std::string current_point_id = line.end_point_id;
      if (!current_point_id.empty() && current_point_id != snapped_point_id) {
        replace_point_id_references(parameters, current_point_id, snapped_point_id);
      }
      line.end_point_id = snapped_point_id;
      set_endpoint_with_constraint(line,
                                   false,
                                   std::get<1>(snapped_end.value()),
                                   std::get<2>(snapped_end.value()));
    }
  }
}

void propagate_connected_point_move(SketchFeatureParameters& parameters,
                                    const std::string& point_id,
                                    double target_x,
                                    double target_y) {
  struct PendingMove {
    std::string point_id;
    double to_x;
    double to_y;
  };

  std::deque<PendingMove> frontier = {{
      .point_id = point_id,
      .to_x = target_x,
      .to_y = target_y,
  }};
  std::vector<std::string> visited_points;

  while (!frontier.empty()) {
    const auto move = frontier.front();
    frontier.pop_front();

    const auto current_point_position = find_point_position(parameters, move.point_id);
    if (point_is_fixed(parameters, move.point_id) &&
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
        [&](const auto& visited) { return visited == move.point_id; });
    if (already_visited) {
      continue;
    }
    visited_points.push_back(move.point_id);

    std::vector<LineEndpointRef> connected_endpoints;
    for (size_t line_index = 0; line_index < parameters.lines.size(); ++line_index) {
      const auto& line = parameters.lines[line_index];
      if (line.start_point_id == move.point_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = true,
        });
      }
      if (line.end_point_id == move.point_id) {
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

      // When a line has a horizontal or vertical constraint and only
      // one endpoint is being dragged, keep the other endpoint anchored
      // and snap the dragged endpoint along the constraint axis.  This
      // lets the line stretch / shorten while staying aligned, rather
      // than rigidly translating the whole line (the old behaviour).
      const std::string other_endpoint_point_id =
          endpoint_point_id(line, !endpoint_ref.is_start);
      const bool other_endpoint_anchored =
          point_is_anchored_to_line(parameters, other_endpoint_point_id);

      if (line.constraint == "horizontal" && !other_endpoint_anchored) {
        if (moved_start) {
          line.start_y = line.end_y;
        } else if (moved_end) {
          line.end_y = line.start_y;
        }
      } else if (line.constraint == "vertical" && !other_endpoint_anchored) {
        if (moved_start) {
          line.start_x = line.end_x;
        } else if (moved_end) {
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
            .point_id = endpoint_point_id(line, !endpoint_ref.is_start),
            .to_x = next_other_x,
            .to_y = next_other_y,
        });
      }
    }
  }
}

// Enforce H/V constraints on every line. Runs during refresh so
// constraint badges always match the actual geometry. If enforcement
// would collapse a line to zero length, the constraint is cleared
// instead — geometry is never destroyed to satisfy a constraint.
//
// Endpoints are set directly on the line without propagation through
// shared points — reconcile_shared_point_positions (called later in
// refresh) handles shared-point consistency. Propagation would
// cascade destructively through connected constrained lines (e.g. a
