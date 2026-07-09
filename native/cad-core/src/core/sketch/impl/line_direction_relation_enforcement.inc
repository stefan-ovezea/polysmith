void enforce_perpendicular_relations(SketchFeatureParameters& parameters,
                                     const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "perpendicular") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_perpendicular_to_reference(
          driven_line, reference_line, parameters);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_vertex_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_vertex_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

void enforce_parallel_relations(SketchFeatureParameters& parameters,
                                const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "parallel") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_parallel_to_reference(driven_line, reference_line, parameters);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_vertex_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_vertex_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}
