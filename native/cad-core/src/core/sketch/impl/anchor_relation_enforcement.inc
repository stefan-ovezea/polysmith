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

// Pull every point_on_arc-attached point onto the arc's SWEEP. The solver only
// pins the circumcircle radius (P2PDistance), so it can legally satisfy the
// constraint anywhere on the circle; this pass projects the point onto the
// angular span the arc actually covers, snapping it to the nearest arc endpoint
// when the solve left it on the far half. Runs after the solver, before the
// constraint/relation enforcement, exactly like the anchor passes above.
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

    // Delta of the point from the start angle, normalised to [-pi, pi).
    double da = a - as;
    da = std::fmod(da, 2.0 * kPi);
    if (da > kPi) da -= 2.0 * kPi;
    if (da < -kPi) da += 2.0 * kPi;

    // Within the sweep when `da` sits between 0 and `sweep` (inclusive).
    const bool within = arc_it->ccw ? (da >= 0.0 && da <= sweep)
                                    : (da <= 0.0 && da >= sweep);
    if (within) continue;

    // Outside the sweep: snap to the nearer arc endpoint (the one `da` passed).
    const double target = (arc_it->ccw ? (da < 0.0) : (da > 0.0)) ? as : as + sweep;
    const double px = arc_it->center_x + arc_it->radius * std::cos(target);
    const double py = arc_it->center_y + arc_it->radius * std::sin(target);
    propagate_connected_point_move(parameters, constraint.target_ids[0], px, py);
  }
}
