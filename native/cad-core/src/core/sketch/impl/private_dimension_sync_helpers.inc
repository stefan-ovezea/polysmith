void sync_line_dimension(SketchFeatureParameters& parameters,
                         const SketchLine& line) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "line_length" && dimension.entity_id == line.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = measure_line_length(line);
  }
}

void sync_circle_dimension(SketchFeatureParameters& parameters,
                           const SketchCircle& circle) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "circle_radius" && dimension.entity_id == circle.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = circle.radius;
  }
}

void sync_angle_dimensions(SketchFeatureParameters& parameters) {
  for (auto& dim : parameters.dimensions) {
    if (dim.kind != "angle") continue;
    if (dim.driven) continue;  // driven dims are synced by sync_driven_dimensions
    if (dim.is_auto && dim.expression.empty()) continue;

    // Re-measure the current angle between the two lines.
    const auto line_a = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == dim.entity_id; });
    const auto line_b = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == dim.secondary_entity_id; });
    if (line_a == parameters.lines.end() || line_b == parameters.lines.end())
      continue;

    // Find shared endpoint.
    const std::array<std::pair<double, double>, 2> a_ends = {{
        {line_a->start_x, line_a->start_y},
        {line_a->end_x, line_a->end_y},
    }};
    const std::array<std::pair<double, double>, 2> b_ends = {{
        {line_b->start_x, line_b->start_y},
        {line_b->end_x, line_b->end_y},
    }};
    int a_pivot = -1, b_pivot = -1;
    for (int i = 0; i < 2 && a_pivot < 0; ++i)
      for (int j = 0; j < 2; ++j)
        if (std::abs(a_ends[i].first - b_ends[j].first) <= 1e-6 &&
            std::abs(a_ends[i].second - b_ends[j].second) <= 1e-6) {
          a_pivot = i; b_pivot = j; break;
        }
    if (a_pivot < 0) continue;

    double px = a_ends[a_pivot].first;
    double py = a_ends[a_pivot].second;
    double a_dx = a_ends[1 - a_pivot].first - px;
    double a_dy = a_ends[1 - a_pivot].second - py;
    double b_dx = b_ends[1 - b_pivot].first - px;
    double b_dy = b_ends[1 - b_pivot].second - py;
    double signed_angle = std::atan2(a_dx * b_dy - a_dy * b_dx,
                                     a_dx * b_dx + a_dy * b_dy);
    double new_val = std::abs(signed_angle);
    if (std::abs(dim.value - new_val) > 0.001) {
      fprintf(stderr, "ANGLE-SYNC: id=%s old=%.4f rad (%.1f°) new=%.4f rad (%.1f°)\n",
              dim.id.c_str(),
              dim.value, dim.value * 180.0 / M_PI,
              new_val, new_val * 180.0 / M_PI);
    }
    dim.value = new_val;
  }
}
