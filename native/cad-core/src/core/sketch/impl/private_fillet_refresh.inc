// Generic enforcement for fillets with at least one arc operand
// (line-arc / arc-arc).  Mirrors the v1 line-line contract: the
// corner is re-derived from the operands' current geometry, the
// tangent circle is re-solved, and the trim points / generated arc
// are written back.  On any failure (missing operand, corner gone,
// radius no longer fits) the cached geometry is left untouched.
void enforce_sketch_fillet_arc(SketchFeatureParameters& parameters,
                               SketchFillet& fillet) {
  const bool a_is_line = !fillet.line_a_id.empty();
  const bool b_is_line = !fillet.line_b_id.empty();

  // Locate the operands; drop silently when one is gone (the sibling
  // cleanup paths remove fillets whose operands are deleted, so a
  // missing operand here means the record is about to go away).
  const auto line_a_it = a_is_line
                             ? std::find_if(parameters.lines.begin(),
                                            parameters.lines.end(),
                                            [&](const SketchLine& line) {
                                              return line.id ==
                                                     fillet.line_a_id;
                                            })
                             : parameters.lines.end();
  const auto line_b_it = b_is_line
                             ? std::find_if(parameters.lines.begin(),
                                            parameters.lines.end(),
                                            [&](const SketchLine& line) {
                                              return line.id ==
                                                     fillet.line_b_id;
                                            })
                             : parameters.lines.end();
  const auto arc_a_it = a_is_line
                            ? parameters.arcs.end()
                            : std::find_if(parameters.arcs.begin(),
                                           parameters.arcs.end(),
                                           [&](const SketchArc& arc) {
                                             return arc.id ==
                                                    fillet.arc_a_id;
                                           });
  const auto arc_b_it = b_is_line
                            ? parameters.arcs.end()
                            : std::find_if(parameters.arcs.begin(),
                                           parameters.arcs.end(),
                                           [&](const SketchArc& arc) {
                                             return arc.id ==
                                                    fillet.arc_b_id;
                                           });
  if ((a_is_line && line_a_it == parameters.lines.end()) ||
      (b_is_line && line_b_it == parameters.lines.end()) ||
      (!a_is_line && arc_a_it == parameters.arcs.end()) ||
      (!b_is_line && arc_b_it == parameters.arcs.end())) {
    return;
  }

  // Which endpoint of each operand holds the trim point (and which
  // holds the far end).
  const bool a_at_start =
      a_is_line ? (line_a_it->start_vertex_id == fillet.trim_a_vertex_id)
                : (arc_a_it->start_vertex_id == fillet.trim_a_vertex_id);
  const bool a_at_end =
      a_is_line ? (line_a_it->end_vertex_id == fillet.trim_a_vertex_id)
                : (arc_a_it->end_vertex_id == fillet.trim_a_vertex_id);
  const bool b_at_start =
      b_is_line ? (line_b_it->start_vertex_id == fillet.trim_b_vertex_id)
                : (arc_b_it->start_vertex_id == fillet.trim_b_vertex_id);
  const bool b_at_end =
      b_is_line ? (line_b_it->end_vertex_id == fillet.trim_b_vertex_id)
                : (arc_b_it->end_vertex_id == fillet.trim_b_vertex_id);
  if ((!a_at_start && !a_at_end) || (!b_at_start && !b_at_end)) {
    return;
  }

  // Far endpoint coords.  For arcs prefer any line sharing the far
  // vertex id — this pass runs before the line→arc propagation pass,
  // so an arc's cached far coords can lag a line drag on the shared
  // vertex.
  const auto far_coords = [&](bool is_line, bool at_start,
                              const auto& entity_it,
                              const std::string& far_vertex_id)
      -> std::pair<double, double> {
    if (is_line) {
      return at_start
                 ? std::pair{entity_it->end_x, entity_it->end_y}
                 : std::pair{entity_it->start_x, entity_it->start_y};
    }
    for (const auto& line : parameters.lines) {
      if (line.start_vertex_id == far_vertex_id) {
        return std::pair{line.start_x, line.start_y};
      }
      if (line.end_vertex_id == far_vertex_id) {
        return std::pair{line.end_x, line.end_y};
      }
    }
    return at_start ? std::pair{entity_it->end_x, entity_it->end_y}
                    : std::pair{entity_it->start_x, entity_it->start_y};
  };
  const std::string far_a_vertex_id =
      a_at_start ? (a_is_line ? line_a_it->end_vertex_id
                              : arc_a_it->end_vertex_id)
                 : (a_is_line ? line_a_it->start_vertex_id
                              : arc_a_it->start_vertex_id);
  const std::string far_b_vertex_id =
      b_at_start ? (b_is_line ? line_b_it->end_vertex_id
                              : arc_b_it->end_vertex_id)
                 : (b_is_line ? line_b_it->start_vertex_id
                              : arc_b_it->start_vertex_id);
  const auto far_a = a_is_line
                         ? far_coords(true, a_at_start, line_a_it,
                                      far_a_vertex_id)
                         : far_coords(false, a_at_start, arc_a_it,
                                      far_a_vertex_id);
  const auto far_b = b_is_line
                         ? far_coords(true, b_at_start, line_b_it,
                                      far_b_vertex_id)
                         : far_coords(false, b_at_start, arc_b_it,
                                      far_b_vertex_id);

  // Re-derive the virtual corner:
  //   line-arc  → unclamped line×circle intersection nearest the
  //               cached corner,
  //   arc-arc   → circle×circle intersection nearest the cached
  //               corner.
  // The line carrier runs through the line's current endpoints (the
  // trim point lies on it, so far→trim spans the carrier).
  double corner_x = 0.0;
  double corner_y = 0.0;
  bool corner_found = false;
  if (!a_is_line && !b_is_line) {
    const auto points = fillet_circle_circle_intersections(
        arc_a_it->center_x, arc_a_it->center_y, arc_a_it->radius,
        arc_b_it->center_x, arc_b_it->center_y, arc_b_it->radius);
    for (const auto& p : points) {
      if (!corner_found ||
          std::hypot(p.first - fillet.corner_x, p.second - fillet.corner_y) <
              std::hypot(corner_x - fillet.corner_x,
                         corner_y - fillet.corner_y)) {
        corner_x = p.first;
        corner_y = p.second;
        corner_found = true;
      }
    }
  } else {
    // Line operand vs arc operand (order-independent).
    const auto* line_op = a_is_line ? &*line_a_it : &*line_b_it;
    // The arc is the *other* side: when A is the line the arc is B.
    const auto* arc_op = a_is_line ? &*arc_b_it : &*arc_a_it;
    const bool line_at_start = a_is_line ? a_at_start : b_at_start;
    const double far_x = line_at_start ? line_op->end_x : line_op->start_x;
    const double far_y = line_at_start ? line_op->end_y : line_op->start_y;
    const double trim_x =
        line_at_start ? line_op->start_x : line_op->end_x;
    const double trim_y =
        line_at_start ? line_op->start_y : line_op->end_y;
    const double dx = trim_x - far_x;
    const double dy = trim_y - far_y;
    const double len = std::hypot(dx, dy);
    if (len > kMinimumSketchDimensionValue) {
      // |far + t·(trim-far) - C|² = R²  →  quadratic in t (unclamped).
      const double ux = dx / len;
      const double uy = dy / len;
      const double px = far_x - arc_op->center_x;
      const double py = far_y - arc_op->center_y;
      const double b = 2.0 * (px * ux + py * uy);
      const double c =
          px * px + py * py - arc_op->radius * arc_op->radius;
      const double disc = b * b - 4.0 * c;
      if (disc >= 0.0) {
        const double root_disc = std::sqrt(disc);
        const double t0 = 0.5 * (-b - root_disc);
        const double t1 = 0.5 * (-b + root_disc);
        // Roots are in units of the normalized direction u.
        const double x0 = far_x + t0 * ux;
        const double y0 = far_y + t0 * uy;
        const double x1 = far_x + t1 * ux;
        const double y1 = far_y + t1 * uy;
        const double d0 =
            std::hypot(x0 - fillet.corner_x, y0 - fillet.corner_y);
        const double d1 =
            std::hypot(x1 - fillet.corner_x, y1 - fillet.corner_y);
        corner_x = d0 <= d1 ? x0 : x1;
        corner_y = d0 <= d1 ? y0 : y1;
        corner_found = true;
      }
    }
  }
  if (!corner_found) {
    return;
  }

  // Build the solver operands from the live geometry and re-solve.
  const auto operand_a =
      a_is_line ? fillet_line_operand(corner_x, corner_y, far_a.first,
                                      far_a.second)
                : fillet_arc_operand(*arc_a_it, corner_x, corner_y,
                                     far_a.first, far_a.second, a_at_start);
  const auto operand_b =
      b_is_line ? fillet_line_operand(corner_x, corner_y, far_b.first,
                                      far_b.second)
                : fillet_arc_operand(*arc_b_it, corner_x, corner_y,
                                     far_b.first, far_b.second, b_at_start);
  if (!operand_a.has_value() || !operand_b.has_value()) {
    return;
  }
  const auto solution =
      solve_fillet_tangent_circle(*operand_a, *operand_b, fillet.radius);
  if (!solution.has_value()) {
    // Radius no longer fits (or the corner collapsed).  Leave the
    // cached geometry alone — the user may resize the operands back.
    return;
  }

  // Cache the live corner position (see rebuild_sketch_vertices).
  fillet.corner_x = corner_x;
  fillet.corner_y = corner_y;

  // Write the trim points back onto the operands.
  const auto write_back_line = [](SketchLine& line, bool at_start, double x,
                                  double y) {
    if (at_start) {
      line.start_x = x;
      line.start_y = y;
    } else {
      line.end_x = x;
      line.end_y = y;
    }
  };
  const auto write_back_arc = [](SketchArc& arc, bool at_start, double x,
                                 double y) {
    if (at_start) {
      arc.start_x = x;
      arc.start_y = y;
    } else {
      arc.end_x = x;
      arc.end_y = y;
    }
  };
  if (a_is_line) {
    write_back_line(*line_a_it, a_at_start, solution->trim_a_x,
                    solution->trim_a_y);
  } else {
    write_back_arc(*arc_a_it, a_at_start, solution->trim_a_x,
                   solution->trim_a_y);
  }
  if (b_is_line) {
    write_back_line(*line_b_it, b_at_start, solution->trim_b_x,
                    solution->trim_b_y);
  } else {
    write_back_arc(*arc_b_it, b_at_start, solution->trim_b_x,
                   solution->trim_b_y);
  }

  // Update the generated arc.
  const auto arc_it = std::find_if(
      parameters.arcs.begin(), parameters.arcs.end(),
      [&](const SketchArc& arc) { return arc.id == fillet.arc_id; });
  if (arc_it != parameters.arcs.end()) {
    arc_it->start_x = solution->trim_a_x;
    arc_it->start_y = solution->trim_a_y;
    arc_it->end_x = solution->trim_b_x;
    arc_it->end_y = solution->trim_b_y;
    arc_it->center_x = solution->center_x;
    arc_it->center_y = solution->center_y;
    arc_it->radius = fillet.radius;
    arc_it->ccw = solution->ccw;
  }
}

void enforce_sketch_fillets(SketchFeatureParameters& parameters) {
  for (auto& fillet : parameters.fillets) {
    // Arc-operand fillets (line-arc / arc-arc) take the generic path
    // above; the v1 line-line body below is unchanged.
    if (!fillet.arc_a_id.empty() || !fillet.arc_b_id.empty()) {
      enforce_sketch_fillet_arc(parameters, fillet);
      continue;
    }
    const auto line_a_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == fillet.line_a_id; });
    const auto line_b_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == fillet.line_b_id; });
    if (line_a_it == parameters.lines.end() ||
        line_b_it == parameters.lines.end()) {
      continue;
    }

    // Identify which end of each line is the trim point. `is_start_a`
    // / `is_start_b` capture this so we know which fields to update
    // when we push the new trim coords back onto the line.
    const bool is_start_a = line_a_it->start_vertex_id == fillet.trim_a_vertex_id;
    const bool is_end_a = line_a_it->end_vertex_id == fillet.trim_a_vertex_id;
    if (!is_start_a && !is_end_a) {
      continue;
    }
    const bool is_start_b = line_b_it->start_vertex_id == fillet.trim_b_vertex_id;
    const bool is_end_b = line_b_it->end_vertex_id == fillet.trim_b_vertex_id;
    if (!is_start_b && !is_end_b) {
      continue;
    }

    const double trim_a_x = is_start_a ? line_a_it->start_x : line_a_it->end_x;
    const double trim_a_y = is_start_a ? line_a_it->start_y : line_a_it->end_y;
    const double far_a_x = is_start_a ? line_a_it->end_x : line_a_it->start_x;
    const double far_a_y = is_start_a ? line_a_it->end_y : line_a_it->start_y;
    const double trim_b_x = is_start_b ? line_b_it->start_x : line_b_it->end_x;
    const double trim_b_y = is_start_b ? line_b_it->start_y : line_b_it->end_y;
    const double far_b_x = is_start_b ? line_b_it->end_x : line_b_it->start_x;
    const double far_b_y = is_start_b ? line_b_it->end_y : line_b_it->start_y;

    // Direction A: from far_a toward (and past) the virtual corner.
    // Currently the line ends at trim_a, but the corner is further
    // along the same direction.
    const double dir_ax = trim_a_x - far_a_x;
    const double dir_ay = trim_a_y - far_a_y;
    const double dir_bx = trim_b_x - far_b_x;
    const double dir_by = trim_b_y - far_b_y;
    const double len_a = std::hypot(dir_ax, dir_ay);
    const double len_b = std::hypot(dir_bx, dir_by);
    if (len_a <= kMinimumSketchDimensionValue ||
        len_b <= kMinimumSketchDimensionValue) {
      continue;
    }

    // Virtual corner = intersection of (far_a + t*dir_a) and
    // (far_b + s*dir_b). Solve 2x2 system using the cross-product
    // form. Reject parallel lines (cross ~= 0).
    const double cross = dir_ax * dir_by - dir_ay * dir_bx;
    if (std::abs(cross) <= kMinimumSketchDimensionValue) {
      continue;
    }
    const double dx = far_b_x - far_a_x;
    const double dy = far_b_y - far_a_y;
    const double t = (dx * dir_by - dy * dir_bx) / cross;
    const double corner_x = far_a_x + t * dir_ax;
    const double corner_y = far_a_y + t * dir_ay;

    // Cache the live corner position on the fillet so the next
    // `rebuild_sketch_vertices` can re-emit the corner point with
    // current coords. See `rebuild_sketch_vertices` for why.
    fillet.corner_x = corner_x;
    fillet.corner_y = corner_y;

    // Outgoing unit directions from the corner toward each far end.
    const double out_ax = far_a_x - corner_x;
    const double out_ay = far_a_y - corner_y;
    const double out_bx = far_b_x - corner_x;
    const double out_by = far_b_y - corner_y;
    const double out_a_len = std::hypot(out_ax, out_ay);
    const double out_b_len = std::hypot(out_bx, out_by);
    if (out_a_len <= kMinimumSketchDimensionValue ||
        out_b_len <= kMinimumSketchDimensionValue) {
      continue;
    }
    const double ux_a = out_ax / out_a_len;
    const double uy_a = out_ay / out_a_len;
    const double ux_b = out_bx / out_b_len;
    const double uy_b = out_by / out_b_len;

    // Half-angle between the two outgoing directions. theta in
    // (0, pi). Reject configurations that have collapsed (parallel
    // case is caught above; this catches the colinear-but-opposite
    // case where dot ≈ -1 and tan(theta/2) blows up — which would
    // give an infinite trim distance).
    const double dot = ux_a * ux_b + uy_a * uy_b;
    const double clamped_dot = std::max(-1.0, std::min(1.0, dot));
    const double theta = std::acos(clamped_dot);
    if (theta <= kMinimumSketchDimensionValue ||
        theta >= 3.141592653589793 - kMinimumSketchDimensionValue) {
      continue;
    }
    const double half_theta = theta * 0.5;
    const double trim_distance = fillet.radius / std::tan(half_theta);
    if (trim_distance + kMinimumSketchDimensionValue >= out_a_len ||
        trim_distance + kMinimumSketchDimensionValue >= out_b_len) {
      // Trim no longer fits on at least one of the lines. Leave the
      // cached geometry alone for now; the user may resize the
      // lines back into a valid range.
      continue;
    }

    const double new_trim_a_x = corner_x + trim_distance * ux_a;
    const double new_trim_a_y = corner_y + trim_distance * uy_a;
    const double new_trim_b_x = corner_x + trim_distance * ux_b;
    const double new_trim_b_y = corner_y + trim_distance * uy_b;

    // Arc center sits along the angle bisector at distance
    // r / sin(theta/2). The bisector direction is the normalized
    // sum of the two outgoing unit vectors.
    const double bisector_x = ux_a + ux_b;
    const double bisector_y = uy_a + uy_b;
    const double bisector_len = std::hypot(bisector_x, bisector_y);
    if (bisector_len <= kMinimumSketchDimensionValue) {
      continue;
    }
    const double center_distance = fillet.radius / std::sin(half_theta);
    const double arc_center_x =
        corner_x + center_distance * (bisector_x / bisector_len);
    const double arc_center_y =
        corner_y + center_distance * (bisector_y / bisector_len);

    // CCW: positive cross product (start - center) x (end - center)
    // means the sweep from start to end is CCW. Mirrors the
    // convention `add_sketch_arc` already follows.
    const double arc_cross =
        (new_trim_a_x - arc_center_x) * (new_trim_b_y - arc_center_y) -
        (new_trim_a_y - arc_center_y) * (new_trim_b_x - arc_center_x);
    const bool arc_ccw = arc_cross > 0.0;

    // Push back to the line cache.
    if (is_start_a) {
      line_a_it->start_x = new_trim_a_x;
      line_a_it->start_y = new_trim_a_y;
    } else {
      line_a_it->end_x = new_trim_a_x;
      line_a_it->end_y = new_trim_a_y;
    }
    if (is_start_b) {
      line_b_it->start_x = new_trim_b_x;
      line_b_it->start_y = new_trim_b_y;
    } else {
      line_b_it->end_x = new_trim_b_x;
      line_b_it->end_y = new_trim_b_y;
    }

    // Update the generated arc.
    const auto arc_it = std::find_if(
        parameters.arcs.begin(),
        parameters.arcs.end(),
        [&](const SketchArc& arc) { return arc.id == fillet.arc_id; });
    if (arc_it != parameters.arcs.end()) {
      arc_it->start_x = new_trim_a_x;
      arc_it->start_y = new_trim_a_y;
      arc_it->end_x = new_trim_b_x;
      arc_it->end_y = new_trim_b_y;
      arc_it->center_x = arc_center_x;
      arc_it->center_y = arc_center_y;
      arc_it->radius = fillet.radius;
      arc_it->ccw = arc_ccw;
    }
  }
}

// Forward-declared in `sketch_feature.h` so the document.cpp Project
// tool path can re-run the recompute pipeline after appending to
// `projected_points`. Defined below in the public namespace; the
// closing brace of the anonymous namespace is right above this
