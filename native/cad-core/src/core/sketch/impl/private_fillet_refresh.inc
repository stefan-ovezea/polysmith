void enforce_sketch_fillets(SketchFeatureParameters& parameters) {
  for (auto& fillet : parameters.fillets) {
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
    const bool is_start_a = line_a_it->start_vertex_id == fillet.trim_a_point_id;
    const bool is_end_a = line_a_it->end_vertex_id == fillet.trim_a_point_id;
    if (!is_start_a && !is_end_a) {
      continue;
    }
    const bool is_start_b = line_b_it->start_vertex_id == fillet.trim_b_point_id;
    const bool is_end_b = line_b_it->end_vertex_id == fillet.trim_b_point_id;
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
