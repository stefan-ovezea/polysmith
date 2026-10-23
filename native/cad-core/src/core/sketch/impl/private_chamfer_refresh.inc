constexpr double kChamferPi = 3.14159265358979323846;

// Distance along line B for a distance+angle chamfer.
//
// `ux_a`/`uy_a` and `ux_b`/`uy_b` are the unit directions leading from the
// corner toward each line's far end. The chamfer line starts `distance_a`
// out along A and leaves at `angle_degrees` to A, tilted toward B; its
// intersection with B is the second trim point.
//
// Returns false when the inputs cannot produce a chamfer there (degenerate
// lines, angle outside (0, interior angle)), in which case the caller keeps
// the previous geometry — the same defer-the-edit policy as the fillet
// radius.
bool chamfer_distance_b_from_angle(double ux_a, double uy_a, double ux_b,
                                   double uy_b, double distance_a,
                                   double angle_degrees, double& distance_b) {
  if (distance_a <= kMinimumSketchDimensionValue) {
    return false;
  }
  if (!(angle_degrees > 0.0) || angle_degrees >= 180.0) {
    return false;
  }

  const double dot = std::max(-1.0, std::min(1.0, ux_a * ux_b + uy_a * uy_b));
  const double interior = std::acos(dot);  // (0, pi)
  const double angle = angle_degrees * kChamferPi / 180.0;
  if (angle >= interior - kMinimumSketchDimensionValue) {
    return false;
  }

  // Perpendicular to A, pointing toward B.
  double vx = ux_b - dot * ux_a;
  double vy = uy_b - dot * uy_a;
  const double v_len = std::hypot(vx, vy);
  if (v_len <= kMinimumSketchDimensionValue) {
    return false;
  }
  vx /= v_len;
  vy /= v_len;

  // Chamfer direction, from the trim point back toward the corner and
  // tilted into the corner's interior: -u_a rotated toward B.
  const double wx = std::cos(angle) * -ux_a + std::sin(angle) * vx;
  const double wy = std::cos(angle) * -uy_a + std::sin(angle) * vy;

  // Solve C + s*u_b = P + t*w with P = C + distance_a*u_a by crossing
  // with w:  s * (u_b x w) = distance_a * (u_a x w).
  const double denom = ux_b * wy - uy_b * wx;
  if (std::abs(denom) <= kMinimumSketchDimensionValue) {
    return false;
  }
  const double numer = ux_a * wy - uy_a * wx;
  const double s = distance_a * numer / denom;
  if (!(s > kMinimumSketchDimensionValue)) {
    return false;
  }
  distance_b = s;
  return true;
}

void enforce_sketch_chamfers(SketchFeatureParameters& parameters) {
  for (auto& chamfer : parameters.chamfers) {
    const auto line_a_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == chamfer.line_a_id; });
    const auto line_b_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == chamfer.line_b_id; });
    if (line_a_it == parameters.lines.end() ||
        line_b_it == parameters.lines.end()) {
      continue;
    }

    // Identify which end of each line is the trim point. `is_start_a`
    // / `is_start_b` capture this so we know which fields to update
    // when we push the new trim coords back onto the line.
    const bool is_start_a = line_a_it->start_vertex_id == chamfer.trim_a_vertex_id;
    const bool is_end_a = line_a_it->end_vertex_id == chamfer.trim_a_vertex_id;
    if (!is_start_a && !is_end_a) {
      continue;
    }
    const bool is_start_b = line_b_it->start_vertex_id == chamfer.trim_b_vertex_id;
    const bool is_end_b = line_b_it->end_vertex_id == chamfer.trim_b_vertex_id;
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

    // Direction A: from far_a toward (and past) the virtual corner —
    // the same virtual-corner construction the fillet pass uses.
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

    // Cache the live corner position on the chamfer so the next
    // `rebuild_sketch_vertices` can re-emit the corner point with
    // current coords (mirrors the fillet's corner cache).
    chamfer.corner_x = corner_x;
    chamfer.corner_y = corner_y;

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

    // Resolve the second distance according to the creation mode. The
    // angle carries the intent for "distance_angle", so it is re-derived
    // here on every recompute instead of trusting the stored value.
    double distance_b = chamfer.distance_b;
    if (chamfer.mode == "equal") {
      distance_b = chamfer.distance_a;
    } else if (chamfer.mode == "distance_angle") {
      if (!chamfer_distance_b_from_angle(ux_a, uy_a, ux_b, uy_b,
                                         chamfer.distance_a,
                                         chamfer.angle_degrees, distance_b)) {
        continue;
      }
    }

    // The trim distance is the stored distance along each outgoing
    // direction.
    if (chamfer.distance_a + kMinimumSketchDimensionValue >= out_a_len ||
        distance_b + kMinimumSketchDimensionValue >= out_b_len) {
      // Trim no longer fits on at least one of the lines. Leave the
      // cached geometry alone for now; the user may resize the lines
      // back into a valid range.
      continue;
    }
    chamfer.distance_b = distance_b;

    const double new_trim_a_x = corner_x + chamfer.distance_a * ux_a;
    const double new_trim_a_y = corner_y + chamfer.distance_a * uy_a;
    const double new_trim_b_x = corner_x + distance_b * ux_b;
    const double new_trim_b_y = corner_y + distance_b * uy_b;

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

    // Update the generated chamfer line.
    const auto chamfer_line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) {
          return line.id == chamfer.chamfer_line_id;
        });
    if (chamfer_line_it != parameters.lines.end()) {
      chamfer_line_it->start_x = new_trim_a_x;
      chamfer_line_it->start_y = new_trim_a_y;
      chamfer_line_it->end_x = new_trim_b_x;
      chamfer_line_it->end_y = new_trim_b_y;
    }
  }
}
