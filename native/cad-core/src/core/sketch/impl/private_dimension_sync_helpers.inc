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

    // Re-measure the current angle between the two lines.
    const auto line_a = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == dim.entity_id; });
    const auto line_b = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == dim.secondary_entity_id; });
    if (line_a == parameters.lines.end() || line_b == parameters.lines.end())
      continue;

    const std::array<std::pair<double, double>, 2> a_ends = {{
        {line_a->start_x, line_a->start_y},
        {line_a->end_x, line_a->end_y},
    }};
    const std::array<std::pair<double, double>, 2> b_ends = {{
        {line_b->start_x, line_b->start_y},
        {line_b->end_x, line_b->end_y},
    }};

    double px, py, a_dx, a_dy, b_dx, b_dy;

    // Virtual pivot: use stored intersection point for non-touching lines.
    if (dim.pivot_x.has_value() && dim.pivot_y.has_value()) {
      auto dir_away = [](double pivot_x, double pivot_y,
                         double e0x, double e0y,
                         double e1x, double e1y) -> std::pair<double, double> {
        double d0 = std::sqrt((e0x - pivot_x) * (e0x - pivot_x) +
                              (e0y - pivot_y) * (e0y - pivot_y));
        double d1 = std::sqrt((e1x - pivot_x) * (e1x - pivot_x) +
                              (e1y - pivot_y) * (e1y - pivot_y));
        double tx = d0 > d1 ? e0x : e1x;
        double ty = d0 > d1 ? e0y : e1y;
        double dx = tx - pivot_x, dy = ty - pivot_y;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-12) return {1.0, 0.0};
        return {dx / len, dy / len};
      };
      px = *dim.pivot_x;
      py = *dim.pivot_y;
      auto ad = dir_away(px, py,
                         a_ends[0].first, a_ends[0].second,
                         a_ends[1].first, a_ends[1].second);
      a_dx = ad.first; a_dy = ad.second;
      auto bd = dir_away(px, py,
                         b_ends[0].first, b_ends[0].second,
                         b_ends[1].first, b_ends[1].second);
      b_dx = bd.first; b_dy = bd.second;
    } else {
      // Find shared endpoint.
      int a_pivot = -1, b_pivot = -1;
      for (int i = 0; i < 2 && a_pivot < 0; ++i)
        for (int j = 0; j < 2; ++j)
          if (std::abs(a_ends[i].first - b_ends[j].first) <= 1e-6 &&
              std::abs(a_ends[i].second - b_ends[j].second) <= 1e-6) {
            a_pivot = i; b_pivot = j; break;
          }
      if (a_pivot < 0) continue;

      px = a_ends[a_pivot].first;
      py = a_ends[a_pivot].second;
      a_dx = a_ends[1 - a_pivot].first - px;
      a_dy = a_ends[1 - a_pivot].second - py;
      b_dx = b_ends[1 - b_pivot].first - px;
      b_dy = b_ends[1 - b_pivot].second - py;
    }

    double signed_angle = std::atan2(a_dx * b_dy - a_dy * b_dx,
                                     a_dx * b_dx + a_dy * b_dy);
    dim.value = signed_angle;
  }
}
