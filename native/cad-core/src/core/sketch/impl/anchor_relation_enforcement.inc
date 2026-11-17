// Pull every midpoint-anchored point to its host line's current
// midpoint, propagating the move through connected lines via the
// existing endpoint-cascade machinery. Safe to call repeatedly:
// when the host line itself doesn't change between calls, the
// computed target equals the current point position so the inner
// propagation is a no-op.
void enforce_midpoint_anchors(SketchFeatureParameters& parameters) {
  for (const auto& anchor : parameters.midpoint_anchors) {
    const auto host_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == anchor.line_id; });
    if (host_it == parameters.lines.end()) {
      continue;
    }
    const double mid_x = (host_it->start_x + host_it->end_x) / 2.0;
    const double mid_y = (host_it->start_y + host_it->end_y) / 2.0;
    propagate_connected_point_move(parameters, anchor.vertex_id, mid_x, mid_y);
  }
}

// Pull every line-anchored point back to its parametric position
// along the host line. Mirrors `enforce_midpoint_anchors` but uses
// the stored `t` instead of always 0.5, so the user can anchor at
// any fraction along the line and have it ride with edits.
void enforce_point_line_anchors(SketchFeatureParameters& parameters) {
  for (const auto& anchor : parameters.point_line_anchors) {
    const auto host_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == anchor.line_id; });
    if (host_it == parameters.lines.end()) {
      continue;
    }
    const double tx =
        host_it->start_x + anchor.t * (host_it->end_x - host_it->start_x);
    const double ty =
        host_it->start_y + anchor.t * (host_it->end_y - host_it->start_y);
    propagate_connected_point_move(parameters, anchor.vertex_id, tx, ty);
  }
}

// Pull every point_on_arc-attached point onto the arc's BOUNDARY and inside its
// SWEEP. A plain arc does NOT participate in the solver (see
// arc_participates_in_solver: only tangent arcs register), so nothing else holds
// the point at the radius — this deterministic pass is the authority, exactly
// like the anchor passes above. It projects the point radially onto the arc at
// its current angle, clamped into the swept span (snapping to the nearest arc
// endpoint when the angle falls outside). Runs after the solver, before the
// constraint/relation enforcement.
void enforce_point_on_arc_constraints(SketchFeatureParameters& parameters) {
  constexpr double kPi = 3.14159265358979323846;
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != "point_on_arc" ||
        constraint.target_ids.size() < 2) {
      continue;
    }
    const auto arc_it = std::find_if(
        parameters.arcs.begin(), parameters.arcs.end(),
        [&](const SketchArc& arc) { return arc.id == constraint.target_ids[1]; });
    if (arc_it == parameters.arcs.end()) continue;

    const auto vertex_it = std::find_if(
        parameters.vertices.begin(), parameters.vertices.end(),
        [&](const SketchVertex& v) {
          return v.id == constraint.target_ids[0];
        });
    if (vertex_it == parameters.vertices.end()) continue;

    // The arc's own endpoints are on it by definition; a point_on_arc that
    // targets one is redundant and would let this pass fight the start/end.
    if (constraint.target_ids[0] == arc_it->start_vertex_id ||
        constraint.target_ids[0] == arc_it->end_vertex_id) {
      continue;
    }

    const double a = std::atan2(vertex_it->y - arc_it->center_y,
                                vertex_it->x - arc_it->center_x);
    const double as = std::atan2(arc_it->start_y - arc_it->center_y,
                                 arc_it->start_x - arc_it->center_x);
    const double ae = std::atan2(arc_it->end_y - arc_it->center_y,
                                 arc_it->end_x - arc_it->center_x);
    // Signed sweep: positive for ccw, negative for cw (matches the solver's
    // arc-angle convention in constraint_solver_entity_mapping.inc).
    double sweep = ae - as;
    if (arc_it->ccw && sweep < 0.0) sweep += 2.0 * kPi;
    if (!arc_it->ccw && sweep > 0.0) sweep -= 2.0 * kPi;

    // Delta of the point from the start angle. Normalise INTO the sweep's
    // direction — [0, 2pi) for ccw, (-2pi, 0] for cw — NOT to [-pi, pi).
    // Folding to [-pi, pi) is wrong for arcs whose sweep exceeds pi: a point
    // more than half a turn along the arc becomes a negative delta and gets
    // clamped back to the START, so it snapped away from where it rode.
    double da = a - as;
    da = std::fmod(da, 2.0 * kPi);
    if (arc_it->ccw) {
      if (da < 0.0) da += 2.0 * kPi;
    } else {
      if (da > 0.0) da -= 2.0 * kPi;
    }

    // Clamp the angular delta into the swept span (ccw: [0, sweep], cw:
    // [sweep, 0]) and project the point onto the boundary at that angle.
    // ALWAYS — not only when outside the sweep — because a plain arc does not
    // participate in the solver, so nothing else holds the point at the radius.
    const double clamped_da =
        std::clamp(da, std::min(0.0, sweep), std::max(0.0, sweep));
    const double target = as + clamped_da;
    const double px = arc_it->center_x + arc_it->radius * std::cos(target);
    const double py = arc_it->center_y + arc_it->radius * std::sin(target);
    if (std::hypot(vertex_it->x - px, vertex_it->y - py) > 1e-9) {
      propagate_connected_point_move(parameters, constraint.target_ids[0], px, py);
    }
  }
}

// Pull every point_on_circle-attached point onto the rim. The solver owns this
// constraint (P2PDistance), so this pass is normally a no-op — but it is the
// authority for the degenerate case the solver CANNOT recover from: a point that
// has been moved onto the circle's CENTRE, where the binding has no direction to
// project along.
//
// Measured 2026-11-04 on the user's document: a point dragged onto the centre sat
// 0.0000 from the centre with r = 38.022 (a 38 mm violation), and from then on
// EVERY refresh failed with `sketch solver failed … status=2` (dofs 3..8, and -3)
// — which also marked dimensions driven (so they stopped driving) and spammed the
// Logs panel. The sketch could never recover, because no drag or dimension edit
// can give the solver a gradient at the centre.
//
// The direction of the repair is the point's own offset from the centre; when the
// point sits ON the centre that offset is undefined, so the owning line's other
// end decides (falling back to +x). Runs after the solver, next to the arc pass.
//
// The position is read through `point_position_of`, NOT the vertex table: during a
// refresh a drag has written the LINE caches and `rebuild_sketch_vertices` has not
// run yet, so the vertex table still holds the pre-drag coordinates. Reading the
// table made this pass a no-op on exactly the case it exists for (measured: the
// pass saw (10,0) while the line had already moved to (0,0), and the sketch stayed
// poisoned).
void enforce_point_on_circle_constraints(SketchFeatureParameters& parameters) {
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != "point_on_circle" ||
        constraint.target_ids.size() < 2) {
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == constraint.target_ids[1];
        });
    if (circle_it == parameters.circles.end()) continue;
    if (circle_it->radius <= kMinimumSketchDimensionValue) continue;

    const auto vertex_it = std::find_if(
        parameters.vertices.begin(),
        parameters.vertices.end(),
        [&](const SketchVertex& vertex) {
          return vertex.id == constraint.target_ids[0];
        });
    if (vertex_it == parameters.vertices.end()) continue;
    // The circle's own centre vertex is not on the rim by definition.
    if (circle_center_vertex_id(*circle_it) == vertex_it->id) continue;

    // Read from the LINE caches first: mid-refresh a drag has written them and the
    // vertex table still holds the pre-drag position (`point_position_of` lives in
    // a file included after this one, hence the local lookup).
    double vx = vertex_it->x;
    double vy = vertex_it->y;
    for (const auto& line : parameters.lines) {
      if (line.start_vertex_id == vertex_it->id) {
        vx = line.start_x;
        vy = line.start_y;
        break;
      }
      if (line.end_vertex_id == vertex_it->id) {
        vx = line.end_x;
        vy = line.end_y;
        break;
      }
    }

    double dx = vx - circle_it->center_x;
    double dy = vy - circle_it->center_y;
    const double distance = std::hypot(dx, dy);
    if (std::abs(distance - circle_it->radius) <= kCoincidentTolerance) {
      continue;
    }
    if (distance <= kMinimumSketchDimensionValue) {
      // Degenerate: the point sits on the centre. Use the owning line's other
      // end as the direction, so the repair lands where the line points.
      dx = 1.0;
      dy = 0.0;
      for (const auto& line : parameters.lines) {
        if (line.start_vertex_id == vertex_it->id) {
          dx = line.end_x - circle_it->center_x;
          dy = line.end_y - circle_it->center_y;
          break;
        }
        if (line.end_vertex_id == vertex_it->id) {
          dx = line.start_x - circle_it->center_x;
          dy = line.start_y - circle_it->center_y;
          break;
        }
      }
      if (std::hypot(dx, dy) <= kMinimumSketchDimensionValue) {
        dx = 1.0;
        dy = 0.0;
      }
    }
    const double length = std::hypot(dx, dy);
    const double px = circle_it->center_x + circle_it->radius * (dx / length);
    const double py = circle_it->center_y + circle_it->radius * (dy / length);
    if (std::hypot(vx - px, vy - py) > 1e-9) {
      propagate_connected_point_move(parameters, vertex_it->id, px, py);
    }
  }
}
