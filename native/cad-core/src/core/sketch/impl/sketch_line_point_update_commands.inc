void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch lines");
  }

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;

  line_it->start_x = start_x;
  line_it->start_y = start_y;
  line_it->end_x = end_x;
  line_it->end_y = end_y;
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void update_sketch_point(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch points");
  }

  auto& parameters = feature.sketch_parameters.value();
  const SketchPoint* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_point(parameters, point_id);

  if (point->kind == "center") {
    // Circle center points: "point-circle-<id>-center"
    {
      const std::string prefix = "point-circle-";
      const std::string suffix = "-center";
      if (point_id.rfind(prefix, 0) == 0 &&
          point_id.size() > prefix.size() + suffix.size() &&
          point_id.substr(point_id.size() - suffix.size()) == suffix) {
        const std::string circle_id =
            point_id.substr(prefix.size(),
                            point_id.size() - prefix.size() - suffix.size());
        const auto circle_it = std::find_if(
            parameters.circles.begin(),
            parameters.circles.end(),
            [&](const SketchCircle& c) { return c.id == circle_id; });
        if (circle_it != parameters.circles.end()) {
          circle_it->center_x = x;
          circle_it->center_y = y;
          refresh_sketch_derived_state(feature);
          return;
        }
      }
    }

    // Arc center points: "point-arc-<id>-center"
    {
      const std::string prefix = "point-arc-";
      const std::string suffix = "-center";
      if (point_id.rfind(prefix, 0) == 0 &&
          point_id.size() > prefix.size() + suffix.size() &&
          point_id.substr(point_id.size() - suffix.size()) == suffix) {
        const std::string arc_id =
            point_id.substr(prefix.size(),
                            point_id.size() - prefix.size() - suffix.size());
        const auto arc_it = std::find_if(
            parameters.arcs.begin(),
            parameters.arcs.end(),
            [&](const SketchArc& a) { return a.id == arc_id; });
        if (arc_it != parameters.arcs.end()) {
          const double dx = x - arc_it->center_x;
          const double dy = y - arc_it->center_y;
          arc_it->center_x = x;
          arc_it->center_y = y;
          arc_it->start_x += dx;
          arc_it->start_y += dy;
          arc_it->end_x += dx;
          arc_it->end_y += dy;
          // Move connected lines that share the arc's endpoints.
          propagate_connected_point_move(parameters, arc_it->start_point_id,
                                         arc_it->start_x, arc_it->start_y);
          propagate_connected_point_move(parameters, arc_it->end_point_id,
                                         arc_it->end_x, arc_it->end_y);
          sync_all_line_dimensions(parameters);
          refresh_sketch_derived_state(feature);
          return;
        }
      }
    }

    throw std::runtime_error("Unsupported sketch center point: " + point_id);
  }

  propagate_connected_point_move(parameters, point_id, x, y);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}
