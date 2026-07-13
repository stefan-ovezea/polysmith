// Re-projects each tangent-bound line's end onto the closer of the
// two tangent points from its start to the host circle. The line's
// start, the circle center, and the radius can all change between
// refreshes; recomputing the tangent on every pass keeps the line
// geometrically tangent without us having to know which input
// changed. The branch selection (T₁ vs T₂) uses the current end as
// a tiebreaker so the line doesn't flip across the circle on small
// edits — flips can still happen if the user drags the start
// through the circle interior, but the user-visible result matches
// what they'd expect from a "stickier" tangent.
void enforce_tangent_line_circle_relations(
    SketchFeatureParameters& parameters) {
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != "tangent_line_circle") {
      continue;
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) {
          return line.id == relation.first_line_id;
        });
    if (line_it == parameters.lines.end()) {
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == relation.second_line_id;
        });
    if (circle_it == parameters.circles.end()) {
      continue;
    }

    const double sx = line_it->start_x;
    const double sy = line_it->start_y;
    const double cx = circle_it->center_x;
    const double cy = circle_it->center_y;
    const double r = circle_it->radius;
    const double dx = cx - sx;
    const double dy = cy - sy;
    const double d_squared = dx * dx + dy * dy;
    // Start lies inside (or on) the circle: no real tangent line
    // through it. Skip rather than throw — the user might be
    // mid-drag and the geometry will become valid again. Validation
    // at op-add time prevents creating tangents that are
    // permanently degenerate.
    if (d_squared <= r * r + 1e-12) {
      continue;
    }
    const double d = std::sqrt(d_squared);
    const double tangent_length = std::sqrt(d_squared - r * r);
    // Unit vector from start toward center, plus its left-perp.
    const double ux = dx / d;
    const double uy = dy / d;
    const double vx = -uy;
    const double vy = ux;
    // Tangent point T = S + (L²/d) * u ± (L*r/d) * v. Derivation:
    // (T-C) ⊥ (T-S) and |T-C| = r, so T sits at angle θ off the
    // S→C ray with sin θ = r/d, cos θ = L/d, at distance L from S.
    const double along = (tangent_length * tangent_length) / d;
    const double perp = (tangent_length * r) / d;
    const double t1x = sx + along * ux + perp * vx;
    const double t1y = sy + along * uy + perp * vy;
    const double t2x = sx + along * ux - perp * vx;
    const double t2y = sy + along * uy - perp * vy;

    const double end_x = line_it->end_x;
    const double end_y = line_it->end_y;
    const double dist1_sq =
        (end_x - t1x) * (end_x - t1x) + (end_y - t1y) * (end_y - t1y);
    const double dist2_sq =
        (end_x - t2x) * (end_x - t2x) + (end_y - t2y) * (end_y - t2y);
    const double tx = dist1_sq <= dist2_sq ? t1x : t2x;
    const double ty = dist1_sq <= dist2_sq ? t1y : t2y;

    if (points_match(end_x, end_y, tx, ty)) {
      continue;
    }
    propagate_connected_point_move(
        parameters, line_it->end_vertex_id, tx, ty);
  }
}
