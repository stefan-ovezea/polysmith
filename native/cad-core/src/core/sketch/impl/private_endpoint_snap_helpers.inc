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
  if (vertex_is_fixed(parameters, line.start_point_id)) {
    line.start_x = previous_start_x;
    line.start_y = previous_start_y;
  }

  if (vertex_is_fixed(parameters, line.end_point_id)) {
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
        !vertex_is_fixed(parameters, line.start_point_id)) {
      const bool can_snap_start =
          !((line.constraint == "horizontal" &&
             vertex_is_fixed(parameters, line.end_point_id) &&
             !nearly_equal(std::get<2>(snapped_start.value()), line.end_y)) ||
            (line.constraint == "vertical" &&
             vertex_is_fixed(parameters, line.end_point_id) &&
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
      !vertex_is_fixed(parameters, line.end_point_id)) {
    const bool can_snap_end =
        !((line.constraint == "horizontal" &&
           vertex_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<2>(snapped_end.value()), line.start_y)) ||
          (line.constraint == "vertical" &&
           vertex_is_fixed(parameters, line.start_point_id) &&
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
