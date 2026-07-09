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
                                   line_it->start_vertex_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_vertex_id,
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
  const SketchVertex* point = find_sketch_vertex(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_vertex(parameters, point_id);

  if (point->kind == "center") {
    // Circle center: use geometry_owner_ids lookup (Phase 4.2)
    if (const auto* circle = find_circle_for_point(parameters, point_id)) {
      // Need mutable access to update coords
      for (auto& c : parameters.circles) {
        if (c.id == circle->id) {
          c.center_x = x;
          c.center_y = y;
          refresh_sketch_derived_state(feature);
          return;
        }
      }
      throw std::runtime_error("Circle not found for center point: " + point_id);
    }

    // Arc center: use geometry_owner_ids lookup (Phase 4.2)
    if (const auto* arc = find_arc_for_point(parameters, point_id)) {
      for (auto& a : parameters.arcs) {
        if (a.id == arc->id) {
          const double dx = x - a.center_x;
          const double dy = y - a.center_y;
          a.center_x = x;
          a.center_y = y;
          a.start_x += dx;
          a.start_y += dy;
          a.end_x += dx;
          a.end_y += dy;
          // Move connected lines that share the arc's endpoints.
          propagate_connected_point_move(parameters, a.start_vertex_id,
                                         a.start_x, a.start_y);
          propagate_connected_point_move(parameters, a.end_vertex_id,
                                         a.end_x, a.end_y);
          sync_all_line_dimensions(parameters);
          refresh_sketch_derived_state(feature);
          return;
        }
      }
      throw std::runtime_error("Arc not found for center point: " + point_id);
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
