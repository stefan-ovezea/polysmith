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
      if (vertex_is_fixed(parameters, line.end_vertex_id)) {
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
      if (vertex_is_fixed(parameters, line.end_vertex_id)) {
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
        dimension.value =
            std::atan2(a_dx * b_dy - a_dy * b_dx,
                       a_dx * b_dx + a_dy * b_dy);
      }
    } else if (dimension.kind == "line_line_distance") {
      const auto& driven_line =
          require_line(parameters, dimension.entity_id);
      const auto& reference_line =
          require_line(parameters, dimension.secondary_entity_id);
      dimension.value =
          std::abs(signed_line_line_distance(driven_line, reference_line));
    } else if (dimension.kind == "circle_center_distance") {
      auto get_cc = [&](const std::string& id) -> std::pair<double,double> {
        if (id.rfind("arc-", 0) == 0) {
          const auto& a = require_arc(parameters, id);
          return {a.center_x, a.center_y};
        }
        const auto& c = require_circle(parameters, id);
        return {c.center_x, c.center_y};
      };
      const auto [dcx, dcy] = get_cc(dimension.entity_id);
      const auto [rcx, rcy] = get_cc(dimension.secondary_entity_id);
      const double dx = dcx - rcx, dy = dcy - rcy;
      dimension.value = std::sqrt(dx * dx + dy * dy);
    } else if (dimension.kind == "circle_line_distance") {
      double cx, cy;
      if (dimension.entity_id.rfind("arc-", 0) == 0) {
        const auto& a = require_arc(parameters, dimension.entity_id);
        cx = a.center_x; cy = a.center_y;
      } else {
        const auto& c = require_circle(parameters, dimension.entity_id);
        cx = c.center_x; cy = c.center_y;
      }
      const auto& line =
          require_line(parameters, dimension.secondary_entity_id);
      dimension.value =
          std::abs(-(line.end_y - line.start_y) * (cx - line.start_x) +
                    (line.end_x - line.start_x) * (cy - line.start_y)) /
          std::sqrt((line.end_x - line.start_x) * (line.end_x - line.start_x) +
                    (line.end_y - line.start_y) * (line.end_y - line.start_y));
    } else if (dimension.kind == "point_distance") {
      const auto point_a_it = std::find_if(
          parameters.vertices.begin(),
          parameters.vertices.end(),
          [&](const SketchVertex& p) {
            return p.id == dimension.entity_id;
          });
      const auto point_b_it = std::find_if(
          parameters.vertices.begin(),
          parameters.vertices.end(),
          [&](const SketchVertex& p) {
            return p.id == dimension.secondary_entity_id;
          });
      if (point_a_it != parameters.vertices.end() &&
          point_b_it != parameters.vertices.end()) {
        const double dx = point_b_it->x - point_a_it->x;
        const double dy = point_b_it->y - point_a_it->y;
        // Respect display_as: "x" → horizontal distance, "y" → vertical.
        if (dimension.display_as == "x") {
          dimension.value = std::abs(dx);
        } else if (dimension.display_as == "y") {
          dimension.value = std::abs(dy);
        } else {
          dimension.value = std::sqrt(dx * dx + dy * dy);
        }
      }
    } else if (dimension.kind == "arc_radius") {
      const auto arc_it = std::find_if(
          parameters.arcs.begin(),
          parameters.arcs.end(),
          [&](const SketchArc& arc) { return arc.id == dimension.entity_id; });
      if (arc_it != parameters.arcs.end()) {
        dimension.value = arc_it->radius;
      }
    } else if (dimension.kind == "arc_angle" ||
               dimension.kind == "arc_length") {
      const auto arc_it = std::find_if(
          parameters.arcs.begin(),
          parameters.arcs.end(),
          [&](const SketchArc& arc) { return arc.id == dimension.entity_id; });
      if (arc_it != parameters.arcs.end()) {
        const double raw_sweep = std::atan2(arc_it->end_y - arc_it->center_y,
                                            arc_it->end_x - arc_it->center_x) -
                                 std::atan2(arc_it->start_y - arc_it->center_y,
                                            arc_it->start_x - arc_it->center_x);
        double sweep = raw_sweep;
        if (sweep < 0.0) sweep += 2.0 * 3.14159265358979323846;
        dimension.value =
            dimension.kind == "arc_length" ? arc_it->radius * sweep : sweep;
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

// ---------------------------------------------------------------------
// Deterministic arc-dimension enforcement (solver-free).
//
// Driving arc_radius / arc_angle dimensions are enforced here rather
// than through planegcs: the solver wanders the null space of an
// unanchored arc (free translation + rotation + sweep) even from a
// zero-residual reference, drifting the arc on every solve.  This pass
// runs after the solver and the line→arc endpoint rescue in
// refresh_sketch_derived_state, so dragged endpoints first move the arc
// (via its angles) and the dimension then re-imposes its value:
//
//  - arc_radius: center and the CURRENT endpoint angles stay; endpoints
//    are recomputed on the circle at the dimension value.  Dragging an
//    endpoint therefore rotates the arc about its center.
//  - arc_angle: center, start point, and the current radius stay; the
//    end point is recomputed at the dimension sweep in the arc's ccw
//    direction.
//
// When a fixed vertex would have to move to satisfy the drive, the
// dimension degrades to driven instead (mirrors the solver
// over-constraint fallback).
void enforce_arc_dimensions(SketchFeatureParameters& parameters) {
  for (auto& dim : parameters.dimensions) {
    if (dim.driven) continue;
    if (dim.is_auto && dim.expression.empty()) continue;
    if (dim.kind != "arc_radius" && dim.kind != "arc_angle" &&
        dim.kind != "arc_length") continue;

    auto arc_it = std::find_if(
        parameters.arcs.begin(), parameters.arcs.end(),
        [&](const SketchArc& arc) { return arc.id == dim.entity_id; });
    if (arc_it == parameters.arcs.end()) {
      dim.driven = true;
      continue;
    }
    auto& arc = *arc_it;

    // Slide a candidate endpoint along an H/V-constrained line that
    // shares the endpoint's vertex to the circle intersection nearest
    // the candidate.  Returns nullopt when the circle cannot reach the
    // constraint axis (diameter too small).
    auto resolve_on_constrained_line =
        [&](const std::string& vertex_id, double cand_x,
            double cand_y) -> std::optional<std::pair<double, double>> {
      for (const auto& line : parameters.lines) {
        const bool is_start_vertex = line.start_vertex_id == vertex_id;
        const bool is_end_vertex = line.end_vertex_id == vertex_id;
        if (!is_start_vertex && !is_end_vertex) continue;
        if (!line.constraint.has_value()) continue;
        const double other_y = is_start_vertex ? line.end_y : line.start_y;
        const double other_x = is_start_vertex ? line.end_x : line.start_x;
        if (line.constraint.value() == "horizontal") {
          const double dy = other_y - arc.center_y;
          const double disc = arc.radius * arc.radius - dy * dy;
          if (disc < 0.0) return std::nullopt;
          const double dx = std::sqrt(disc);
          const double x1 = arc.center_x + dx;
          const double x2 = arc.center_x - dx;
          const double x =
              std::abs(x1 - cand_x) < std::abs(x2 - cand_x) ? x1 : x2;
          return std::pair<double, double>{x, other_y};
        }
        if (line.constraint.value() == "vertical") {
          const double dx = other_x - arc.center_x;
          const double disc = arc.radius * arc.radius - dx * dx;
          if (disc < 0.0) return std::nullopt;
          const double dy = std::sqrt(disc);
          const double y1 = arc.center_y + dy;
          const double y2 = arc.center_y - dy;
          const double y =
              std::abs(y1 - cand_y) < std::abs(y2 - cand_y) ? y1 : y2;
          return std::pair<double, double>{other_x, y};
        }
      }
      return std::pair<double, double>{cand_x, cand_y};
    };

    // Positions are read from the arc CACHE: the line→arc rescue pass
    // in refresh_sketch_derived_state keeps shared endpoints fresh after
    // a drag (propagate_connected_point_move only walks lines, and the
    // vertex table is rebuilt only later).  The is_fixed flags come
    // from the vertex table.
    const SketchVertex* start_v = nullptr;
    const SketchVertex* end_v = nullptr;
    for (const auto& v : parameters.vertices) {
      if (v.id == arc.start_vertex_id) start_v = &v;
      if (v.id == arc.end_vertex_id) end_v = &v;
    }
    if (start_v == nullptr || end_v == nullptr) {
      continue;
    }

    if (dim.kind == "arc_radius") {
      if (start_v->is_fixed || end_v->is_fixed) {
        dim.driven = true;
        continue;
      }
      const double start_angle =
          std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
      const double end_angle =
          std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
      arc.radius = dim.value;
      const double cand_start_x =
          arc.center_x + arc.radius * std::cos(start_angle);
      const double cand_start_y =
          arc.center_y + arc.radius * std::sin(start_angle);
      const double cand_end_x = arc.center_x + arc.radius * std::cos(end_angle);
      const double cand_end_y = arc.center_y + arc.radius * std::sin(end_angle);
      const auto resolved_start = resolve_on_constrained_line(
          arc.start_vertex_id, cand_start_x, cand_start_y);
      const auto resolved_end = resolve_on_constrained_line(
          arc.end_vertex_id, cand_end_x, cand_end_y);
      if (!resolved_start.has_value() || !resolved_end.has_value()) {
        // The circle cannot reach an H/V-constrained shared line —
        // degrade to reference-only instead of producing an
        // inconsistent cache.
        dim.driven = true;
        arc.radius = std::hypot(arc.start_x - arc.center_x,
                                arc.start_y - arc.center_y);
        continue;
      }
      arc.start_x = resolved_start->first;
      arc.start_y = resolved_start->second;
      arc.end_x = resolved_end->first;
      arc.end_y = resolved_end->second;
      // Write the recomputed endpoint positions back to the shared
      // vertices so connected geometry (lines) follows.
      propagate_connected_point_move(parameters, arc.start_vertex_id,
                                     arc.start_x, arc.start_y);
      propagate_connected_point_move(parameters, arc.end_vertex_id,
                                     arc.end_x, arc.end_y);
    } else {  // arc_angle / arc_length
      if (end_v->is_fixed) {
        dim.driven = true;
        continue;
      }
      const double radius = std::hypot(arc.start_x - arc.center_x,
                                       arc.start_y - arc.center_y);
      const double start_angle =
          std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
      // Arc length = radius * sweep, so the sweep derives from the
      // stored length and the current radius.
      const double sweep =
          dim.kind == "arc_length" ? dim.value / radius : dim.value;
      const double signed_value = arc.ccw ? sweep : -sweep;
      const double end_angle = start_angle + signed_value;
      arc.radius = radius;
      const double cand_end_x = arc.center_x + radius * std::cos(end_angle);
      const double cand_end_y = arc.center_y + radius * std::sin(end_angle);
      const auto resolved_end = resolve_on_constrained_line(
          arc.end_vertex_id, cand_end_x, cand_end_y);
      if (!resolved_end.has_value() ||
          !points_match(resolved_end->first, resolved_end->second,
                        cand_end_x, cand_end_y)) {
        // The angle-determined end position conflicts with an
        // H/V-constrained shared line — there is no freedom to slide,
        // so degrade to reference-only.
        dim.driven = true;
        continue;
      }
      arc.end_x = resolved_end->first;
      arc.end_y = resolved_end->second;
      propagate_connected_point_move(parameters, arc.end_vertex_id,
                                     arc.end_x, arc.end_y);
    }
  }
}
