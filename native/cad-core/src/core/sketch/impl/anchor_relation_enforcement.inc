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
