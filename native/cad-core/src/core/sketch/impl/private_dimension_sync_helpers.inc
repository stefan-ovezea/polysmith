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
      // Recompute virtual pivot from current line positions so the arc
      // center and reference line track geometry after drags / solves.
      {
        double a_dx_inf = a_ends[1].first - a_ends[0].first;
        double a_dy_inf = a_ends[1].second - a_ends[0].second;
        double b_dx_inf = b_ends[1].first - b_ends[0].first;
        double b_dy_inf = b_ends[1].second - b_ends[0].second;
        double det = a_dx_inf * b_dy_inf - a_dy_inf * b_dx_inf;
        if (std::abs(det) > 1e-12) {
          double t = ((b_ends[0].first - a_ends[0].first) * b_dy_inf -
                      (b_ends[0].second - a_ends[0].second) * b_dx_inf) / det;
          px = a_ends[0].first + t * a_dx_inf;
          py = a_ends[0].second + t * a_dy_inf;
          dim.pivot_x = px;
          dim.pivot_y = py;
        } else {
          px = *dim.pivot_x;
          py = *dim.pivot_y;
        }
      }
      // Pick the pair closest to the stored angle so the read-back
      // preserves the quadrant the user selected.
      {
        auto unit = [](double x, double y, double px, double py) {
          double dx = x - px, dy = y - py;
          double len = std::sqrt(dx * dx + dy * dy);
          if (len < 1e-12) return std::pair<double, double>{1.0, 0.0};
          return std::pair<double, double>{dx / len, dy / len};
        };
        std::pair<double, double> au[2] = {
            unit(a_ends[0].first, a_ends[0].second, px, py),
            unit(a_ends[1].first, a_ends[1].second, px, py)};
        std::pair<double, double> bu[2] = {
            unit(b_ends[0].first, b_ends[0].second, px, py),
            unit(b_ends[1].first, b_ends[1].second, px, py)};
        auto ad_def = dir_away(px, py, a_ends[0].first, a_ends[0].second,
                                       a_ends[1].first, a_ends[1].second);
        auto bd_def = dir_away(px, py, b_ends[0].first, b_ends[0].second,
                                       b_ends[1].first, b_ends[1].second);
        double stored_abs = std::abs(dim.value);
        double best_diff = 1e9;
        for (int ai = 0; ai < 2; ++ai) {
          for (int bi = 0; bi < 2; ++bi) {
            double cross = au[ai].first * bu[bi].second - au[ai].second * bu[bi].first;
            double dot   = au[ai].first * bu[bi].first + au[ai].second * bu[bi].second;
            double measured = std::abs(std::atan2(cross, dot));
            double diff = std::abs(measured - stored_abs);
            bool is_def =
                (au[ai].first == ad_def.first && au[ai].second == ad_def.second) &&
                (bu[bi].first == bd_def.first && bu[bi].second == bd_def.second);
            if (is_def) diff -= 1e-12;
            if (diff < best_diff) {
              best_diff = diff;
              a_dx = au[ai].first; a_dy = au[ai].second;
              b_dx = bu[bi].first; b_dy = bu[bi].second;
            }
          }
        }
      }
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

    double cross = a_dx * b_dy - a_dy * b_dx;
    double dot   = a_dx * b_dx + a_dy * b_dy;
    double signed_angle = std::atan2(cross, dot);

    // For virtual-pivot dimensions, when the lines are collinear
    // (cross ≈ 0) both "away-from-pivot" directions may collapse
    // to the same unit vector, making the signed-angle read-back
    // ambiguous — atan2 reports 0 when the stored value may be ±π.
    // Preserve the stored angle so the dimension label and
    // subsequent edits use the correct reference.
    if (dim.pivot_x.has_value() && dim.pivot_y.has_value() &&
        std::abs(cross) < 1e-9) {
      continue;
    }

    dim.value = signed_angle;
  }
}
