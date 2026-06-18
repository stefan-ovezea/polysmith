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

#include "core/sketch/impl/angle_direction_helpers.inc"

void sync_angle_dimensions(SketchFeatureParameters& parameters) {
  constexpr double kPi = 3.14159265358979323846;

  for (auto& dim : parameters.dimensions) {
    if (dim.kind != "angle") continue;
    if (dim.driven) continue;

    const auto line_a = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == dim.entity_id; });
    const auto line_b = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == dim.secondary_entity_id; });
    if (line_a == parameters.lines.end() || line_b == parameters.lines.end())
      continue;

    double px, py;

    if (dim.pivot_x.has_value() && dim.pivot_y.has_value()) {
      // Virtual pivot — recompute from current line positions.
      double a_dx = line_a->end_x - line_a->start_x;
      double a_dy = line_a->end_y - line_a->start_y;
      double b_dx = line_b->end_x - line_b->start_x;
      double b_dy = line_b->end_y - line_b->start_y;
      double det = a_dx * b_dy - a_dy * b_dx;
      if (std::abs(det) > 1e-12) {
        double t = ((line_b->start_x - line_a->start_x) * b_dy -
                    (line_b->start_y - line_a->start_y) * b_dx) / det;
        px = line_a->start_x + t * a_dx;
        py = line_a->start_y + t * a_dy;
        dim.pivot_x = px;
        dim.pivot_y = py;
      } else {
        px = *dim.pivot_x;
        py = *dim.pivot_y;
      }
    } else {
      // Shared endpoint — use physical line directions.
      auto a_endpoint = [&](int i) -> std::pair<double, double> {
        return i == 0
            ? std::pair<double, double>{line_a->start_x, line_a->start_y}
            : std::pair<double, double>{line_a->end_x, line_a->end_y};
      };
      auto b_endpoint = [&](int j) -> std::pair<double, double> {
        return j == 0
            ? std::pair<double, double>{line_b->start_x, line_b->start_y}
            : std::pair<double, double>{line_b->end_x, line_b->end_y};
      };
      int a_pivot = -1, b_pivot = -1;
      for (int i = 0; i < 2 && a_pivot < 0; ++i)
        for (int j = 0; j < 2; ++j)
          if (std::abs(a_endpoint(i).first - b_endpoint(j).first) <= 1e-6 &&
              std::abs(a_endpoint(i).second - b_endpoint(j).second) <= 1e-6) {
            a_pivot = i; b_pivot = j; break;
          }
      if (a_pivot < 0) continue;
      auto pt = a_endpoint(a_pivot);
      px = pt.first;
      py = pt.second;
      auto a_pt = a_endpoint(1 - a_pivot);
      auto b_pt = b_endpoint(1 - b_pivot);
      double a_dx = a_pt.first - px, a_dy = a_pt.second - py;
      double b_dx = b_pt.first - px, b_dy = b_pt.second - py;
      double signed_angle = std::atan2(a_dx * b_dy - a_dy * b_dx,
                                       a_dx * b_dx + a_dy * b_dy);
      // Preserve reflex values (> π).
      if (std::abs(dim.value) > kPi) {
        double preserved = std::abs(dim.value);
        dim.value = signed_angle < 0.0 ? -preserved : preserved;
      } else {
        dim.value = signed_angle;
      }
      continue;
    }

    // Virtual-pivot path — use the 4‑pair resolver.
    {
    double abs_val = std::abs(dim.value);
    if (abs_val > kPi) abs_val = 2.0 * kPi - abs_val;
    auto resolved = resolve_angle_directions(*line_a, *line_b, px, py,
                                             abs_val);
    // Collinear guard: when lines have become collinear the direction
    // read-back is ambiguous. Preserve the stored value.
    if (std::abs(resolved.signed_angle) < 1e-9 &&
        std::abs(std::abs(dim.value) - kPi) < 1e-3) {
      continue;
    }
    if (std::abs(dim.value) > kPi) {
      double preserved = std::abs(dim.value);
      dim.value = resolved.signed_angle < 0.0 ? -preserved : preserved;
    } else {
      dim.value = resolved.signed_angle;
    }
    }
  }
}
