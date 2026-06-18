void enforce_line_HV_constraints(SketchFeatureParameters& parameters) {
  for (auto& line : parameters.lines) {
    if (!line.constraint.has_value()) continue;

    if (line.constraint.value() == "horizontal") {
      if (nearly_equal(line.start_y, line.end_y)) continue;
      if (points_match(line.start_x, line.start_y, line.end_x, line.start_y)) {
        line.constraint = std::nullopt;
        fprintf(stderr, "WARN cleared horizontal constraint on %s "
                "(would create zero-length line)\n", line.id.c_str());
        continue;
      }
      if (point_is_fixed(parameters, line.end_point_id)) {
        line.start_y = line.end_y;
      } else {
        line.end_y = line.start_y;
      }
    } else if (line.constraint.value() == "vertical") {
      if (nearly_equal(line.start_x, line.end_x)) continue;
      if (points_match(line.start_x, line.start_y, line.start_x, line.end_y)) {
        line.constraint = std::nullopt;
        fprintf(stderr, "WARN cleared vertical constraint on %s "
                "(would create zero-length line)\n", line.id.c_str());
        continue;
      }
      if (point_is_fixed(parameters, line.end_point_id)) {
        line.start_x = line.end_x;
      } else {
        line.end_x = line.start_x;
      }
    }
  }
}

// Seedless wrappers for the relation-enforcement functions so they
// can be called from refresh without knowing a specific line.
void enforce_all_equal_length_relations(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    enforce_equal_length_relations(parameters, line.id);
  }
}
void enforce_all_perpendicular_relations(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    enforce_perpendicular_relations(parameters, line.id);
  }
}
void enforce_all_parallel_relations(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    enforce_parallel_relations(parameters, line.id);
  }
}

void sync_all_line_dimensions(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    sync_line_dimension(parameters, line);
  }
}

void sync_driven_dimensions(SketchFeatureParameters& parameters) {
  for (auto& dimension : parameters.dimensions) {
    if (!dimension.driven) {
      continue;
    }
    try {
    if (dimension.kind == "line_length") {
      const auto& line = require_line(parameters, dimension.entity_id);
      dimension.value = measure_line_length(line);
    } else if (dimension.kind == "circle_radius") {
      const auto& circle = require_circle(parameters, dimension.entity_id);
      dimension.value = circle.radius;
    } else if (dimension.kind == "polygon_radius") {
      const auto polygon_it = std::find_if(
          parameters.polygons.begin(),
          parameters.polygons.end(),
          [&](const SketchPolygon& polygon) {
            return polygon.id == dimension.entity_id;
          });
      if (polygon_it != parameters.polygons.end()) {
        dimension.value = polygon_it->radius;
      }
    } else if (dimension.kind == "line_angle") {
      const auto& line = require_line(parameters, dimension.entity_id);
      const double dx = line.end_x - line.start_x;
      const double dy = line.end_y - line.start_y;
      dimension.value = std::atan2(dy, dx);
    } else if (dimension.kind == "angle") {
      const auto& line_a = require_line(parameters, dimension.entity_id);
      const auto& line_b =
          require_line(parameters, dimension.secondary_entity_id);
      // Find shared endpoint and compute signed angle.
      const std::array<std::pair<double, double>, 2> a_ends = {{
          {line_a.start_x, line_a.start_y},
          {line_a.end_x, line_a.end_y},
      }};
      const std::array<std::pair<double, double>, 2> b_ends = {{
          {line_b.start_x, line_b.start_y},
          {line_b.end_x, line_b.end_y},
      }};
      int a_pivot = -1;
      int b_pivot = -1;
      for (int i = 0; i < 2 && a_pivot < 0; ++i) {
        for (int j = 0; j < 2; ++j) {
          if (std::abs(a_ends[i].first - b_ends[j].first) <=
                  kCoincidentTolerance &&
              std::abs(a_ends[i].second - b_ends[j].second) <=
                  kCoincidentTolerance) {
            a_pivot = i;
            b_pivot = j;
            break;
          }
        }
      }
      if (a_pivot >= 0) {
        const double px = a_ends[a_pivot].first;
        const double py = a_ends[a_pivot].second;
        const double a_dx = a_ends[1 - a_pivot].first - px;
        const double a_dy = a_ends[1 - a_pivot].second - py;
        const double b_dx = b_ends[1 - b_pivot].first - px;
        const double b_dy = b_ends[1 - b_pivot].second - py;
        // Recompute the signed angle from A to B.
        double signed_angle =
            std::atan2(a_dx * b_dy - a_dy * b_dx,
                       a_dx * b_dx + a_dy * b_dy);
        // Preserve reflex for driven dimensions: atan2 returns
        // [-π, π] so reflex values (> π) would be lost.
        constexpr double kDrivenPi = 3.14159265358979323846;
        if (std::abs(dimension.value) > kDrivenPi) {
          double preserved = std::abs(dimension.value);
          dimension.value = signed_angle < 0.0 ? -preserved : preserved;
        } else {
          dimension.value = signed_angle;
        }
      }
    } else if (dimension.kind == "line_line_distance") {
      const auto& driven_line =
          require_line(parameters, dimension.entity_id);
      const auto& reference_line =
          require_line(parameters, dimension.secondary_entity_id);
      dimension.value =
          std::abs(signed_line_line_distance(driven_line, reference_line));
    } else if (dimension.kind == "circle_center_distance") {
      const auto& driven_circle =
          require_circle(parameters, dimension.entity_id);
      const auto& reference_circle =
          require_circle(parameters, dimension.secondary_entity_id);
      dimension.value =
          distance_between_circles(driven_circle, reference_circle);
    } else if (dimension.kind == "circle_line_distance") {
      const auto& circle = require_circle(parameters, dimension.entity_id);
      const auto& line =
          require_line(parameters, dimension.secondary_entity_id);
      dimension.value =
          std::abs(signed_circle_line_distance(circle, line));
    } else if (dimension.kind == "point_distance") {
      const auto point_a_it = std::find_if(
          parameters.points.begin(),
          parameters.points.end(),
          [&](const SketchPoint& p) {
            return p.id == dimension.entity_id;
          });
      const auto point_b_it = std::find_if(
          parameters.points.begin(),
          parameters.points.end(),
          [&](const SketchPoint& p) {
            return p.id == dimension.secondary_entity_id;
          });
      if (point_a_it != parameters.points.end() &&
          point_b_it != parameters.points.end()) {
        const double dx = point_b_it->x - point_a_it->x;
        const double dy = point_b_it->y - point_a_it->y;
        dimension.value = std::sqrt(dx * dx + dy * dy);
      }
    }
    } catch (const std::exception& e) {
      fprintf(stderr, "sync_driven_dimensions: skipping %s '%s': %s\n",
              dimension.kind.c_str(), dimension.id.c_str(), e.what());
      // Un-drive the broken dimension so it won't throw on every
      // subsequent refresh.  The user can delete it from the UI.
      dimension.driven = false;
    }
  }
}
