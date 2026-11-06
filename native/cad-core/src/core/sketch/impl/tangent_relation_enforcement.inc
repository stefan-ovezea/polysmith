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
    const auto arc_it = std::find_if(
        parameters.arcs.begin(),
        parameters.arcs.end(),
        [&](const SketchArc& arc) {
          return arc.id == relation.second_line_id;
        });
    if (circle_it == parameters.circles.end() &&
        arc_it == parameters.arcs.end()) {
      continue;
    }

    const double sx = line_it->start_x;
    const double sy = line_it->start_y;
    const double cx = circle_it != parameters.circles.end()
                          ? circle_it->center_x : arc_it->center_x;
    const double cy = circle_it != parameters.circles.end()
                          ? circle_it->center_y : arc_it->center_y;
    const double r = circle_it != parameters.circles.end()
                         ? circle_it->radius : arc_it->radius;
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
    // A line that is ALREADY tangent to the circle is left exactly as the solver
    // left it. This pass means "keep the line touching the circle" — the solver
    // expresses it as distance(centre, line) == r, which holds for any end point on
    // the line — so re-projecting the END onto the contact point is a second,
    // stronger rule that silently overrode whatever else pinned the line's length.
    // Measured 2026-11-06 on the user's fully-constrained sketch (two tangent
    // lines, equal length, both ends on one circle, two DRIVING 223.0223 mm length
    // dimensions): every drag moved the apex, the solver satisfied all seven
    // equations, and this pass then re-projected the ends and grew both lines to
    // 233.159 mm — the dimensions were rewritten to match and the drag "changed the
    // length" no matter what the user wanted.
    {
      const double line_dx = line_it->end_x - line_it->start_x;
      const double line_dy = line_it->end_y - line_it->start_y;
      const double line_length = std::hypot(line_dx, line_dy);
      if (line_length > kMinimumSketchDimensionValue) {
        const double perpendicular =
            std::abs(line_dx * (cy - sy) - line_dy * (cx - sx)) / line_length;
        // Tolerance is scale-aware: the solver satisfies the relation to its own
        // numeric tolerance, and sketches run to hundreds of mm here, so an
        // absolute 1e-6 rejected states that are tangent for every practical
        // purpose and the pass then "repaired" a satisfied line.
        const double tolerance = std::max(1e-4, std::abs(r) * 1e-7);
        if (std::abs(perpendicular - r) <= tolerance) {
          continue;
        }
      }
    }
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


// Circle-tangent relations (created by the circle tool's tangent
// modes): the CIRCLE is the slave — its center stays where the user
// placed it and the radius re-derives as the distance from the
// center to each defining line on every refresh. With several lines
// the radius takes the minimum distance so the circle stays inside
// the wedge even after the lines move (exact dual tangency after
// arbitrary edits would require solving the center onto the new
// bisector — deferred).
void enforce_tangent_circle_line_relations(
    SketchFeatureParameters& parameters) {
  // One pass: collect the minimum distance per circle across all of
  // its relations (a straight min-loop would never grow the radius
  // back when the closer line moves away).
  std::map<std::string, double> min_distance;
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != "tangent_circle_line") {
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(), parameters.circles.end(),
        [&](const SketchCircle& c) { return c.id == relation.first_line_id; });
    if (circle_it == parameters.circles.end()) {
      continue;
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& l) { return l.id == relation.second_line_id; });
    if (line_it == parameters.lines.end()) {
      continue;
    }
    const double abx = line_it->end_x - line_it->start_x;
    const double aby = line_it->end_y - line_it->start_y;
    const double length = std::hypot(abx, aby);
    if (length <= kMinimumSketchDimensionValue) {
      continue;
    }
    // Distance from the circle center to the line's support.
    const double cross = std::abs(
        abx * (circle_it->center_y - line_it->start_y) -
        aby * (circle_it->center_x - line_it->start_x));
    const double distance = cross / length;
    const auto [it, inserted] =
        min_distance.emplace(circle_it->id, distance);
    if (!inserted) {
      it->second = std::min(it->second, distance);
    }
  }
  for (auto& circle : parameters.circles) {
    const auto it = min_distance.find(circle.id);
    if (it == min_distance.end() || it->second <= kMinimumSketchDimensionValue) {
      continue;
    }
    circle.radius = it->second;
  }
}
