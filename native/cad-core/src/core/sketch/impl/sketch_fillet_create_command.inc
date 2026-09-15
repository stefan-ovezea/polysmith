void add_sketch_fillet(FeatureEntry &feature, int fillet_index,
                       int trim_a_point_index, int trim_b_point_index,
                       int arc_index, const std::string &corner_vertex_id,
                       const std::string &line_a_id,
                       const std::string &line_b_id, double radius,
                       const std::string &arc_a_id,
                       const std::string &arc_b_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch fillets");
  }
  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch fillets must have a positive radius");
  }

  // Operand kind dispatch: each side is exactly one of a line or an
  // arc (line-line fillets — the v1 shape — leave both arc ids empty).
  const bool a_is_line = !line_a_id.empty();
  const bool a_is_arc = !arc_a_id.empty();
  const bool b_is_line = !line_b_id.empty();
  const bool b_is_arc = !arc_b_id.empty();
  if ((a_is_line == a_is_arc) || (b_is_line == b_is_arc)) {
    throw std::runtime_error(
        "Sketch fillet requires exactly one line or arc per side");
  }

  // ── v1 line-line path — unchanged (the body below is the
  //    original implementation, kept verbatim inside this branch).
  //    The generic arc-operand path follows after the branch.
  if (a_is_line && b_is_line) {
  if (line_a_id == line_b_id) {
    throw std::runtime_error("Sketch fillet requires two distinct lines");
  }

  auto &parameters = *feature.sketch_parameters;
  auto &line_a = require_line(parameters, line_a_id);
  auto &line_b = require_line(parameters, line_b_id);

  // Both lines must reference the corner point as one of their
  // endpoints — otherwise the user clicked a point that isn't shared.
  const bool a_at_start = line_a.start_vertex_id == corner_vertex_id;
  const bool a_at_end = line_a.end_vertex_id == corner_vertex_id;
  const bool b_at_start = line_b.start_vertex_id == corner_vertex_id;
  const bool b_at_end = line_b.end_vertex_id == corner_vertex_id;
  if ((!a_at_start && !a_at_end) || (!b_at_start && !b_at_end)) {
    throw std::runtime_error(
        "Sketch fillet: corner point must be shared by both lines");
  }

  // Refuse to fillet a line that's already part of another fillet at
  // this corner — the lines need a clean shared endpoint to start
  // from. Lines filleted at the *other* end are fine.
  for (const auto &existing : parameters.fillets) {
    const bool a_collides =
        (existing.line_a_id == line_a_id || existing.line_b_id == line_a_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    const bool b_collides =
        (existing.line_a_id == line_b_id || existing.line_b_id == line_b_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    if (a_collides || b_collides) {
      throw std::runtime_error(
          "Sketch fillet: this corner is already filleted");
    }
  }

  // A corner holding both a fillet and a chamfer is ambiguous — reject
  // the combination outright (the chamfer create path has the mirrored
  // check).
  for (const auto &existing : parameters.chamfers) {
    const bool a_collides =
        (existing.line_a_id == line_a_id || existing.line_b_id == line_a_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    const bool b_collides =
        (existing.line_a_id == line_b_id || existing.line_b_id == line_b_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    if (a_collides || b_collides) {
      throw std::runtime_error(
          "Sketch fillet: this corner is already chamfered");
    }
  }

  // "Far" endpoint of each line — the one opposite the corner.
  const double far_a_x = a_at_start ? line_a.end_x : line_a.start_x;
  const double far_a_y = a_at_start ? line_a.end_y : line_a.start_y;
  const double far_b_x = b_at_start ? line_b.end_x : line_b.start_x;
  const double far_b_y = b_at_start ? line_b.end_y : line_b.start_y;
  const double corner_x = a_at_start ? line_a.start_x : line_a.end_x;
  const double corner_y = a_at_start ? line_a.start_y : line_a.end_y;

  // Outgoing directions from the corner toward each far endpoint.
  const double out_ax = far_a_x - corner_x;
  const double out_ay = far_a_y - corner_y;
  const double out_bx = far_b_x - corner_x;
  const double out_by = far_b_y - corner_y;
  const double len_a = std::hypot(out_ax, out_ay);
  const double len_b = std::hypot(out_bx, out_by);
  if (len_a <= kMinimumSketchDimensionValue ||
      len_b <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: both lines must have non-zero length");
  }
  const double ux_a = out_ax / len_a;
  const double uy_a = out_ay / len_a;
  const double ux_b = out_bx / len_b;
  const double uy_b = out_by / len_b;

  const double cross = ux_a * uy_b - uy_a * ux_b;
  if (std::abs(cross) <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: the two lines are parallel or colinear");
  }

  const double dot = ux_a * ux_b + uy_a * uy_b;
  const double clamped_dot = std::max(-1.0, std::min(1.0, dot));
  const double theta = std::acos(clamped_dot);
  if (theta <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: the two lines have no measurable angle between them");
  }
  const double half_theta = theta * 0.5;
  const double trim_distance = radius / std::tan(half_theta);
  if (trim_distance + kMinimumSketchDimensionValue >= len_a ||
      trim_distance + kMinimumSketchDimensionValue >= len_b) {
    throw std::runtime_error(
        "Sketch fillet: radius is too large for at least one of the lines");
  }

  // New trim point coordinates and arc center.
  const double trim_a_x = corner_x + trim_distance * ux_a;
  const double trim_a_y = corner_y + trim_distance * uy_a;
  const double trim_b_x = corner_x + trim_distance * ux_b;
  const double trim_b_y = corner_y + trim_distance * uy_b;
  const double bisector_x = ux_a + ux_b;
  const double bisector_y = uy_a + uy_b;
  const double bisector_len = std::hypot(bisector_x, bisector_y);
  // bisector_len > 0 here because we rejected near-180° above.
  const double center_distance = radius / std::sin(half_theta);
  const double arc_center_x =
      corner_x + center_distance * (bisector_x / bisector_len);
  const double arc_center_y =
      corner_y + center_distance * (bisector_y / bisector_len);
  const double arc_cross =
      (trim_a_x - arc_center_x) * (trim_b_y - arc_center_y) -
      (trim_a_y - arc_center_y) * (trim_b_x - arc_center_x);
  const bool arc_ccw = arc_cross > 0.0;

  // Allocate generated entity ids. Trim points share the same
  // namespace as line endpoints (point-N), the arc its own (arc-N).
  // The fillet itself uses fillet-N.
  const std::string trim_a_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
  const std::string trim_b_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
  const std::string arc_id = "arc-" + std::to_string(arc_index);
  const std::string fillet_id = "fillet-" + std::to_string(fillet_index);

  // Mutate the lines: replace their corner-pointing endpoint with
  // the new trim point. Also update the cached coords so they match
  // the trim position right away. The recompute pass will refresh
  // these on every subsequent edit.
  if (a_at_start) {
    line_a.start_vertex_id = trim_a_id;
    line_a.start_x = trim_a_x;
    line_a.start_y = trim_a_y;
  } else {
    line_a.end_vertex_id = trim_a_id;
    line_a.end_x = trim_a_x;
    line_a.end_y = trim_a_y;
  }
  if (b_at_start) {
    line_b.start_vertex_id = trim_b_id;
    line_b.start_x = trim_b_x;
    line_b.start_y = trim_b_y;
  } else {
    line_b.end_vertex_id = trim_b_id;
    line_b.end_x = trim_b_x;
    line_b.end_y = trim_b_y;
  }

  // Append the generated arc (its endpoints reference the new trim
  // points) and the fillet record.
  parameters.arcs.push_back(SketchArc{
      .id = arc_id,
      .start_vertex_id = trim_a_id,
      .end_vertex_id = trim_b_id,
      .center_x = arc_center_x,
      .center_y = arc_center_y,
      .radius = radius,
      .start_x = trim_a_x,
      .start_y = trim_a_y,
      .end_x = trim_b_x,
      .end_y = trim_b_y,
      .ccw = arc_ccw,
  });
  parameters.fillets.push_back(SketchFillet{
      .id = fillet_id,
      .corner_vertex_id = corner_vertex_id,
      .corner_x = corner_x,
      .corner_y = corner_y,
      .line_a_id = line_a_id,
      .line_b_id = line_b_id,
      .trim_a_vertex_id = trim_a_id,
      .trim_b_vertex_id = trim_b_id,
      .arc_id = arc_id,
      .radius = radius,
  });

  refresh_sketch_derived_state(feature);
  return;
  }

  // ── Generic arc-operand path (line-arc / arc-arc) ──────────────
  // Resolves the operands by kind, validates the corner sharing and
  // the away tangents, solves the tangent circle with the shared
  // solver (private_fillet_arc_solver.inc), then mutates the
  // operands and appends the generated arc + record exactly like
  // the v1 path.

  // Distinct operands (same-kind comparison — line and arc ids live
  // in separate namespaces, so cross-kind ids can never collide).
  if (a_is_arc && b_is_arc && arc_a_id == arc_b_id) {
    throw std::runtime_error("Sketch fillet requires two distinct operands");
  }

  auto &parameters = *feature.sketch_parameters;
  SketchLine *line_a =
      a_is_line ? &require_line(parameters, line_a_id) : nullptr;
  SketchLine *line_b =
      b_is_line ? &require_line(parameters, line_b_id) : nullptr;
  SketchArc *arc_a = a_is_arc ? &require_arc(parameters, arc_a_id) : nullptr;
  SketchArc *arc_b = b_is_arc ? &require_arc(parameters, arc_b_id) : nullptr;

  // Both operands must reference the corner point as one of their
  // endpoints — otherwise the user clicked a point that isn't shared.
  // Legacy sketches (projected before the endpoint welding existed)
  // may hold two vertex ids a few microns apart at the corner: accept
  // an endpoint whose cached coords sit within the coincident
  // tolerance of the corner vertex position, and weld it onto the
  // canonical corner id below (permanent — delete then restores both
  // operands to the single vertex).
  bool a_at_start =
      a_is_line ? (line_a->start_vertex_id == corner_vertex_id)
                : (arc_a->start_vertex_id == corner_vertex_id);
  bool a_at_end =
      a_is_line ? (line_a->end_vertex_id == corner_vertex_id)
                : (arc_a->end_vertex_id == corner_vertex_id);
  bool b_at_start =
      b_is_line ? (line_b->start_vertex_id == corner_vertex_id)
                : (arc_b->start_vertex_id == corner_vertex_id);
  bool b_at_end =
      b_is_line ? (line_b->end_vertex_id == corner_vertex_id)
                : (arc_b->end_vertex_id == corner_vertex_id);
  const auto corner_position = [&]() -> std::optional<std::pair<double, double>> {
    for (const auto& vertex : parameters.vertices) {
      if (vertex.id == corner_vertex_id) {
        return std::pair{vertex.x, vertex.y};
      }
    }
    return std::nullopt;
  }();
  const auto near_corner = [&](double x, double y) {
    return corner_position.has_value() &&
           std::abs(x - corner_position->first) <= kCoincidentTolerance &&
           std::abs(y - corner_position->second) <= kCoincidentTolerance;
  };
  const auto resolve_side = [&](bool is_line, bool& at_start, bool& at_end,
                                double sx, double sy, double ex, double ey) {
    (void)is_line;
    if (at_start || at_end) return;
    if (near_corner(sx, sy)) {
      at_start = true;
    } else if (near_corner(ex, ey)) {
      at_end = true;
    }
  };
  resolve_side(a_is_line, a_at_start, a_at_end,
               a_is_line ? line_a->start_x : arc_a->start_x,
               a_is_line ? line_a->start_y : arc_a->start_y,
               a_is_line ? line_a->end_x : arc_a->end_x,
               a_is_line ? line_a->end_y : arc_a->end_y);
  resolve_side(b_is_line, b_at_start, b_at_end,
               b_is_line ? line_b->start_x : arc_b->start_x,
               b_is_line ? line_b->start_y : arc_b->start_y,
               b_is_line ? line_b->end_x : arc_b->end_x,
               b_is_line ? line_b->end_y : arc_b->end_y);
  if ((!a_at_start && !a_at_end) || (!b_at_start && !b_at_end)) {
    throw std::runtime_error(
        "Sketch fillet: corner point must be shared by both entities");
  }

  // Arc operands must be real geometry — construction arcs cannot be
  // filleted (the fillet would not contribute to profiles), and a
  // degenerate radius cannot host a tangent circle.
  if (arc_a != nullptr &&
      (arc_a->is_construction ||
       arc_a->radius <= kMinimumSketchDimensionValue)) {
    throw std::runtime_error(
        "Sketch fillet: construction arcs cannot be filleted");
  }
  if (arc_b != nullptr &&
      (arc_b->is_construction ||
       arc_b->radius <= kMinimumSketchDimensionValue)) {
    throw std::runtime_error(
        "Sketch fillet: construction arcs cannot be filleted");
  }

  // "Far" endpoint of each operand — the one opposite the corner —
  // and the corner coords (read from operand A's corner endpoint;
  // both operands share the vertex).
  const double corner_x =
      a_at_start ? (a_is_line ? line_a->start_x : arc_a->start_x)
                 : (a_is_line ? line_a->end_x : arc_a->end_x);
  const double corner_y =
      a_at_start ? (a_is_line ? line_a->start_y : arc_a->start_y)
                 : (a_is_line ? line_a->end_y : arc_a->end_y);
  const double far_a_x =
      a_at_start ? (a_is_line ? line_a->end_x : arc_a->end_x)
                 : (a_is_line ? line_a->start_x : arc_a->start_x);
  const double far_a_y =
      a_at_start ? (a_is_line ? line_a->end_y : arc_a->end_y)
                 : (a_is_line ? line_a->start_y : arc_a->start_y);
  const double far_b_x =
      b_at_start ? (b_is_line ? line_b->end_x : arc_b->end_x)
                 : (b_is_line ? line_b->start_x : arc_b->start_x);
  const double far_b_y =
      b_at_start ? (b_is_line ? line_b->end_y : arc_b->end_y)
                 : (b_is_line ? line_b->start_y : arc_b->start_y);

  // Weld legacy split endpoints onto the canonical corner id (the
  // resolution above may have matched by position only).  The weld is
  // permanent — enforcement and delete-restore then work off the one
  // shared vertex.
  const auto weld_side = [&](bool is_line, bool at_start, bool at_end,
                             SketchLine* line_op, SketchArc* arc_op) {
    if (is_line) {
      if (at_start && line_op->start_vertex_id != corner_vertex_id) {
        line_op->start_vertex_id = corner_vertex_id;
        line_op->start_x = corner_x;
        line_op->start_y = corner_y;
      } else if (at_end && line_op->end_vertex_id != corner_vertex_id) {
        line_op->end_vertex_id = corner_vertex_id;
        line_op->end_x = corner_x;
        line_op->end_y = corner_y;
      }
    } else {
      if (at_start && arc_op->start_vertex_id != corner_vertex_id) {
        arc_op->start_vertex_id = corner_vertex_id;
        arc_op->start_x = corner_x;
        arc_op->start_y = corner_y;
      } else if (at_end && arc_op->end_vertex_id != corner_vertex_id) {
        arc_op->end_vertex_id = corner_vertex_id;
        arc_op->end_x = corner_x;
        arc_op->end_y = corner_y;
      }
    }
  };
  weld_side(a_is_line, a_at_start, a_at_end, line_a, arc_a);
  weld_side(b_is_line, b_at_start, b_at_end, line_b, arc_b);

  // Refuse to fillet an operand that's already part of another
  // fillet at this corner (extended to arc operands).  Operands
  // filleted at the *other* end are fine.
  for (const auto &existing : parameters.fillets) {
    const bool a_collides =
        (existing.line_a_id == line_a_id || existing.line_b_id == line_a_id ||
         existing.arc_a_id == arc_a_id || existing.arc_b_id == arc_a_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    const bool b_collides =
        (existing.line_a_id == line_b_id || existing.line_b_id == line_b_id ||
         existing.arc_a_id == arc_b_id || existing.arc_b_id == arc_b_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    if (a_collides || b_collides) {
      throw std::runtime_error(
          "Sketch fillet: this corner is already filleted");
    }
  }

  // A corner holding both a fillet and a chamfer is ambiguous —
  // reject the combination (the line ids carry the line side of a
  // mixed fillet; chamfers are line-line so arc ids can't collide).
  for (const auto &existing : parameters.chamfers) {
    const bool a_collides =
        (existing.line_a_id == line_a_id || existing.line_b_id == line_a_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    const bool b_collides =
        (existing.line_a_id == line_b_id || existing.line_b_id == line_b_id) &&
        (existing.trim_a_vertex_id == corner_vertex_id ||
         existing.trim_b_vertex_id == corner_vertex_id);
    if (a_collides || b_collides) {
      throw std::runtime_error(
          "Sketch fillet: this corner is already chamfered");
    }
  }

  // Build the solver operands from the live geometry.
  const auto operand_a =
      a_is_line
          ? fillet_line_operand(corner_x, corner_y, far_a_x, far_a_y)
          : fillet_arc_operand(*arc_a, corner_x, corner_y, far_a_x, far_a_y,
                               a_at_start);
  const auto operand_b =
      b_is_line
          ? fillet_line_operand(corner_x, corner_y, far_b_x, far_b_y)
          : fillet_arc_operand(*arc_b, corner_x, corner_y, far_b_x, far_b_y,
                               b_at_start);
  if (!operand_a.has_value() || !operand_b.has_value()) {
    throw std::runtime_error(
        "Sketch fillet: operands are degenerate at this corner");
  }

  // Away directions at the corner: a line points toward its far end;
  // an arc follows its walk tangent into the body.  Parallel away
  // directions mean the entities are tangent at the corner — there
  // is no corner to round.
  const double away_a_x =
      a_is_line ? operand_a->away_x
                : fillet_arc_away_tangent(*operand_a).value().first;
  const double away_a_y =
      a_is_line ? operand_a->away_y
                : fillet_arc_away_tangent(*operand_a).value().second;
  const double away_b_x =
      b_is_line ? operand_b->away_x
                : fillet_arc_away_tangent(*operand_b).value().first;
  const double away_b_y =
      b_is_line ? operand_b->away_y
                : fillet_arc_away_tangent(*operand_b).value().second;
  const double away_cross = away_a_x * away_b_y - away_a_y * away_b_x;
  if (std::abs(away_cross) <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: entities are tangent at this corner");
  }

  const auto solution =
      solve_fillet_tangent_circle(*operand_a, *operand_b, radius);
  if (!solution.has_value()) {
    throw std::runtime_error(
        "Sketch fillet: radius does not fit between the selected entities");
  }

  // Allocate generated entity ids. Trim points share the same
  // namespace as line endpoints (point-N), the arc its own (arc-N).
  // The fillet itself uses fillet-N.
  const std::string trim_a_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
  const std::string trim_b_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
  const std::string arc_id = "arc-" + std::to_string(arc_index);
  const std::string fillet_id = "fillet-" + std::to_string(fillet_index);

  // Mutate the operands: replace their corner-pointing endpoint with
  // the new trim point (line and arc alike).
  if (a_is_line) {
    if (a_at_start) {
      line_a->start_vertex_id = trim_a_id;
      line_a->start_x = solution->trim_a_x;
      line_a->start_y = solution->trim_a_y;
    } else {
      line_a->end_vertex_id = trim_a_id;
      line_a->end_x = solution->trim_a_x;
      line_a->end_y = solution->trim_a_y;
    }
  } else {
    if (a_at_start) {
      arc_a->start_vertex_id = trim_a_id;
      arc_a->start_x = solution->trim_a_x;
      arc_a->start_y = solution->trim_a_y;
    } else {
      arc_a->end_vertex_id = trim_a_id;
      arc_a->end_x = solution->trim_a_x;
      arc_a->end_y = solution->trim_a_y;
    }
  }
  if (b_is_line) {
    if (b_at_start) {
      line_b->start_vertex_id = trim_b_id;
      line_b->start_x = solution->trim_b_x;
      line_b->start_y = solution->trim_b_y;
    } else {
      line_b->end_vertex_id = trim_b_id;
      line_b->end_x = solution->trim_b_x;
      line_b->end_y = solution->trim_b_y;
    }
  } else {
    if (b_at_start) {
      arc_b->start_vertex_id = trim_b_id;
      arc_b->start_x = solution->trim_b_x;
      arc_b->start_y = solution->trim_b_y;
    } else {
      arc_b->end_vertex_id = trim_b_id;
      arc_b->end_x = solution->trim_b_x;
      arc_b->end_y = solution->trim_b_y;
    }
  }

  // Append the generated arc (its endpoints reference the new trim
  // points) and the fillet record.
  parameters.arcs.push_back(SketchArc{
      .id = arc_id,
      .start_vertex_id = trim_a_id,
      .end_vertex_id = trim_b_id,
      .center_x = solution->center_x,
      .center_y = solution->center_y,
      .radius = radius,
      .start_x = solution->trim_a_x,
      .start_y = solution->trim_a_y,
      .end_x = solution->trim_b_x,
      .end_y = solution->trim_b_y,
      .ccw = solution->ccw,
  });
  parameters.fillets.push_back(SketchFillet{
      .id = fillet_id,
      .corner_vertex_id = corner_vertex_id,
      .corner_x = corner_x,
      .corner_y = corner_y,
      .line_a_id = line_a_id,
      .line_b_id = line_b_id,
      .arc_a_id = arc_a_id,
      .arc_b_id = arc_b_id,
      .trim_a_vertex_id = trim_a_id,
      .trim_b_vertex_id = trim_b_id,
      .arc_id = arc_id,
      .radius = radius,
  });

  refresh_sketch_derived_state(feature);
}
