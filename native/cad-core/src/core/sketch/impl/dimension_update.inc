#include "core/sketch/impl/dimension_angle_update.inc"

void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value,
                             std::optional<std::string> expression) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch dimensions");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& dimension = require_dimension(parameters, dimension_id);

  // Angle dimensions use signed radian values that can be small or
  // negative — the generic >0 check does not apply.
  if (dimension.kind != "angle" && dimension.kind != "line_angle") {
    if (value <= kMinimumSketchDimensionValue) {
      throw std::runtime_error("Sketch dimensions must be greater than zero");
    }
  }

  // Driven (reference-only) dimensions show the measured value but do
  // not drive geometry. Silently ignore edit attempts.
  if (dimension.driven) {
    return;
  }

  // Store expression if provided — cleared when expression is nullopt
  // (plain numeric update).
  dimension.expression = expression.value_or("");

  // Snapshot all stored label positions and their anchor midpoints
  // before any geometry change, so we can shift them by the same
  // delta after the solver moves geometry.
  struct LabelPre { std::string id; double lx, ly; double mx, my; };
  std::vector<LabelPre> pre_labels;
  auto find_pt = [&](const std::string& pid) -> std::pair<double,double> {
    for (const auto& l : parameters.lines) {
      if (l.start_point_id == pid) return {l.start_x, l.start_y};
      if (l.end_point_id == pid) return {l.end_x, l.end_y};
    }
    for (const auto& c : parameters.circles) {
      if ("point-circle-" + c.id + "-center" == pid)
        return {c.center_x, c.center_y};
    }
    for (const auto& p : parameters.points) {
      if (p.id == pid) return {p.x, p.y};
    }
    return {0.0, 0.0};
  };
  for (const auto& dim : parameters.dimensions) {
    if (!dim.label_x.has_value() || !dim.label_y.has_value()) continue;
    double mx = 0, my = 0;
    bool found = false;
    if (dim.kind == "line_length" || dim.kind == "line_line_distance") {
      for (const auto& l : parameters.lines) {
        if (l.id == dim.entity_id) {
          mx = (l.start_x + l.end_x) / 2.0;
          my = (l.start_y + l.end_y) / 2.0;
          found = true; break;
        }
      }
    } else if (dim.kind == "point_distance" ||
               dim.kind == "circle_center_distance") {
      auto pa = find_pt(dim.entity_id);
      auto pb = find_pt(dim.secondary_entity_id);
      mx = (pa.first + pb.first) / 2.0;
      my = (pa.second + pb.second) / 2.0;
      found = true;
    }
    if (found) pre_labels.push_back({dim.id, *dim.label_x, *dim.label_y, mx, my});
  }

  auto sync_all_labels = [&]() {
    for (auto& dim : parameters.dimensions) {
      if (!dim.label_x.has_value()) continue;
      for (const auto& pre : pre_labels) {
        if (pre.id != dim.id) continue;
        double mx = 0, my = 0;
        bool found = false;
        if (dim.kind == "line_length" || dim.kind == "line_line_distance") {
          for (const auto& l : parameters.lines) {
            if (l.id == dim.entity_id) {
              mx = (l.start_x + l.end_x) / 2.0;
              my = (l.start_y + l.end_y) / 2.0;
              found = true; break;
            }
          }
        } else if (dim.kind == "point_distance" ||
                   dim.kind == "circle_center_distance") {
          auto pa = find_pt(dim.entity_id);
          auto pb = find_pt(dim.secondary_entity_id);
          mx = (pa.first + pb.first) / 2.0;
          my = (pa.second + pb.second) / 2.0;
          found = true;
        }
        if (found) {
          dim.label_x = pre.lx + (mx - pre.mx);
          dim.label_y = pre.ly + (my - pre.my);
        }
        break;
      }
    }
  };

  // Helper: after a dimension edit, check whether the solver detected
  // over-constraint or simply failed to converge.  When either occurs,
  // fall back to driven (reference-only).  With DogLeg as the default
  // solver, failure is a strong signal — DogLeg rarely fails on
  // under-constrained sketches.
  //
  // solver_dofs == -1 means the solver hasn't run yet (default value);
  // it must not be mistaken for over-constrained (dofs < 0).
  auto fallback_to_driven = [&]() {
    auto& p = *feature.sketch_parameters;
    bool solver_ran = p.solver_dofs != -1;
    if (solver_ran &&
        (p.solver_conflicting_count > 0 ||
         p.solver_redundant_count > 0 ||
         p.solver_dofs < 0 ||
         !p.solver_ok)) {
      dimension.driven = true;
      refresh_sketch_derived_state(feature);
      return true;
    }
    return false;
  };

  if (dimension.kind == "line_length") {
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == dimension.entity_id; });

    if (line_it == parameters.lines.end()) {
      throw std::runtime_error("Sketch line not found for dimension: " + dimension_id);
    }

    const double previous_start_x = line_it->start_x;
    const double previous_start_y = line_it->start_y;
    const double previous_end_x = line_it->end_x;
    const double previous_end_y = line_it->end_y;

    drive_line_length_respecting_fixed_points(parameters, *line_it, value);
    snap_line_endpoints_to_coincident_geometry(parameters, *line_it);
    if (!points_match(previous_start_x,
                      previous_start_y,
                      line_it->start_x,
                      line_it->start_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->start_point_id,
                                     line_it->start_x,
                                     line_it->start_y);
    }
    if (!points_match(previous_end_x,
                      previous_end_y,
                      line_it->end_x,
                      line_it->end_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->end_point_id,
                                     line_it->end_x,
                                     line_it->end_y);
    }
    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_it->id);
    enforce_perpendicular_relations(parameters, line_it->id);
    enforce_parallel_relations(parameters, line_it->id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "circle_radius") {
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == dimension.entity_id;
        });

    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for dimension: " + dimension_id);
    }

    circle_it->radius = value;
    sync_circle_dimension(parameters, *circle_it);
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "angle") {
    // Angle dimensions are created as auto (display-only).  When the
    // user explicitly edits the value, clear the auto flag so the
    // solver constraint activates and prevents sync from overwriting
    // the edit on the next refresh.
    dimension.is_auto = false;
    // Snapshot line coordinates before the geometric edit so we can
    // roll back if the solver detects over-constraint afterward.
    struct LineSnapshot { double sx, sy, ex, ey; };
    std::vector<LineSnapshot> pre_snapshot;
    pre_snapshot.reserve(parameters.lines.size());
    for (const auto& l : parameters.lines) {
      pre_snapshot.push_back({l.start_x, l.start_y, l.end_x, l.end_y});
    }
    update_angle_dimension(feature, parameters, dimension, value);
    if (fallback_to_driven() && pre_snapshot.size() == parameters.lines.size()) {
      // Restore pre-edit positions — the angle constraint was rejected
      // so the geometry rotation is invalid.
      for (size_t i = 0; i < pre_snapshot.size(); ++i) {
        parameters.lines[i].start_x = pre_snapshot[i].sx;
        parameters.lines[i].start_y = pre_snapshot[i].sy;
        parameters.lines[i].end_x   = pre_snapshot[i].ex;
        parameters.lines[i].end_y   = pre_snapshot[i].ey;
      }
      // Re-sync so everything is consistent.
      refresh_sketch_derived_state(feature);
      sync_all_labels();
    }
    sync_all_labels();
    return;
  }

  if (dimension.kind == "line_angle") {
    // Line-angle dimension: absolute angle from the positive X axis
    // (horizontal). Rotate the line about its START point, preserving
    // the current length.
    auto& line = require_line(parameters, dimension.entity_id);

    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double current_length = std::sqrt(dx * dx + dy * dy);
    if (current_length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Line-angle dimension requires a line with non-zero length");
    }
    // Preserve the sign quadrant: the user enters a magnitude but the
    // line keeps its current orientation.  Mirrors the angle-between-lines
    // variant just above.
    const double current_angle = std::atan2(dy, dx);
    const double target = (current_angle < 0.0) ? -value : value;

    const double new_end_x = line.start_x + std::cos(target) * current_length;
    const double new_end_y = line.start_y + std::sin(target) * current_length;

    const double previous_end_x = line.end_x;
    const double previous_end_y = line.end_y;

    line.end_x = new_end_x;
    line.end_y = new_end_y;

    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    // Propagate the moved endpoint through any coincident points.
    if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    dimension.value = target;
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "circle_center_distance") {
    auto& driven_circle = require_circle(parameters, dimension.entity_id);
    const auto& reference_circle =
        require_circle(parameters, dimension.secondary_entity_id);
    const double dx = driven_circle.center_x - reference_circle.center_x;
    const double dy = driven_circle.center_y - reference_circle.center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Circle-center distance requires distinct circle centers");
    }
    driven_circle.center_x = reference_circle.center_x + (dx / length) * value;
    driven_circle.center_y = reference_circle.center_y + (dy / length) * value;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "circle_line_distance") {
    auto& circle = require_circle(parameters, dimension.entity_id);
    const auto& line = require_line(parameters, dimension.secondary_entity_id);
    const double current_signed = signed_circle_line_distance(circle, line);
    const double direction = current_signed < 0.0 ? -1.0 : 1.0;
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double normal_x = -dy / length;
    const double normal_y = dx / length;
    const double delta = direction * value - current_signed;
    circle.center_x += normal_x * delta;
    circle.center_y += normal_y * delta;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "line_line_distance") {
    auto& driven_line = require_line(parameters, dimension.entity_id);
    const auto& reference_line =
        require_line(parameters, dimension.secondary_entity_id);
    const double current_signed =
        signed_line_line_distance(driven_line, reference_line);
    const double direction = current_signed < 0.0 ? -1.0 : 1.0;
    const double dx = reference_line.end_x - reference_line.start_x;
    const double dy = reference_line.end_y - reference_line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double normal_x = -dy / length;
    const double normal_y = dx / length;
    const double delta = direction * value - current_signed;
    driven_line.start_x += normal_x * delta;
    driven_line.start_y += normal_y * delta;
    driven_line.end_x += normal_x * delta;
    driven_line.end_y += normal_y * delta;
    sync_all_line_dimensions(parameters);
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "point_distance") {
    auto& point_a = require_point(parameters, dimension.entity_id);
    auto& point_b = require_point(parameters, dimension.secondary_entity_id);
    const double dx = point_b.x - point_a.x;
    const double dy = point_b.y - point_a.y;

    if (dimension.display_as == "x") {
      // Horizontal distance: move point_b along X only.
      const double sign = (dx >= 0) ? 1.0 : -1.0;
      point_b.x = point_a.x + sign * value;
    } else if (dimension.display_as == "y") {
      // Vertical distance: move point_b along Y only.
      const double sign = (dy >= 0) ? 1.0 : -1.0;
      point_b.y = point_a.y + sign * value;
    } else {
      // Euclidean — existing behaviour.
      const double current_length = std::sqrt(dx * dx + dy * dy);
      if (current_length <= kMinimumSketchDimensionValue) {
        throw std::runtime_error(
            "Point-distance dimension requires distinct points");
      }
      point_b.x = point_a.x + (dx / current_length) * value;
      point_b.y = point_a.y + (dy / current_length) * value;
    }
    dimension.value = value;
    dimension.driven = false;  // reset — may be set below
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  if (dimension.kind == "polygon_radius") {
    const auto polygon_it = std::find_if(
        parameters.polygons.begin(),
        parameters.polygons.end(),
        [&](const SketchPolygon& polygon) {
          return polygon.id == dimension.entity_id;
        });

    if (polygon_it == parameters.polygons.end()) {
      throw std::runtime_error("Sketch polygon not found for dimension: " + dimension_id);
    }

    polygon_it->radius = value;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    fallback_to_driven();
    sync_all_labels();
    return;
  }

  throw std::runtime_error("Unsupported sketch dimension kind: " + dimension.kind);
}
