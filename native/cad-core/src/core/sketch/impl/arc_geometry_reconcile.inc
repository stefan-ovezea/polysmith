// ── Keep an arc's parametric shape consistent with its own endpoints ──────
//
// An arc stores a centre, a radius and a ccw flag, but its endpoints are
// ordinary SHARED vertices — welded to the lines a trim left joined to it.
// Those two representations can drift apart: a drag moves a shared endpoint,
// the vertex table follows, `sync_entity_coords_from_vertices` writes the new
// positions into the arc cache, and `radius` keeps its old value. Nothing
// reconciled them, so the renderer — which samples at `radius` — drew the arc
// somewhere its endpoints were not ("it enlarges but disconnects the line";
// the reported document had radius 80 with endpoint distances 103.65 and
// 219.79, a chord of 307 mm that no r=80 circle can span).
//
// The endpoints win: they are the real, shared geometry. Given both endpoints
// and the arc's span there is exactly one circle, so the centre and radius are
// re-derived from them and the span is preserved.
//
//   chord = |end - start|,  h = |sweep| / 2
//   radius = chord / (2 sin h)
//   centre = midpoint ± perpendicular(chord) * radius * cos h
//
// The ± side is the one that reproduces the arc's own sweep direction, so the
// arc keeps bulging the way it did.
//
// Skipped when the radius is genuinely authoritative: a driving `arc_radius`
// dimension owns it (that path re-derives the endpoints instead), and derived
// arcs (fillets, projections, text) are rebuilt by their own passes.
void reconcile_arc_geometry_from_endpoints(SketchFeatureParameters& parameters) {
  constexpr double kTolerance = 1.0e-6;

  const auto has_driving_radius_dimension = [&](const std::string& arc_id) {
    for (const auto& dimension : parameters.dimensions) {
      if (dimension.kind != "arc_radius" || dimension.driven) continue;
      if (dimension.entity_id == arc_id) return true;
    }
    return false;
  };

  for (auto& arc : parameters.arcs) {
    if (arc.generated_by.has_value()) continue;
    if (has_driving_radius_dimension(arc.id)) continue;

    const double start_radius = std::hypot(arc.start_x - arc.center_x,
                                           arc.start_y - arc.center_y);
    const double end_radius =
        std::hypot(arc.end_x - arc.center_x, arc.end_y - arc.center_y);
    if (std::abs(start_radius - arc.radius) <= kTolerance &&
        std::abs(end_radius - arc.radius) <= kTolerance) {
      continue;  // consistent — the common case
    }

    const double chord_x = arc.end_x - arc.start_x;
    const double chord_y = arc.end_y - arc.start_y;
    const double chord = std::hypot(chord_x, chord_y);
    if (chord <= kTolerance) continue;  // degenerate; leave it alone

    // The span as currently observed — the shape to preserve.
    const double target_sweep = arc_sweep_angle(arc);
    const double half = std::abs(target_sweep) * 0.5;
    const double sin_half = std::sin(half);
    if (sin_half <= 1.0e-9) continue;  // ~0 or ~360 degrees: no unique circle

    const double radius = chord / (2.0 * sin_half);
    const double offset = radius * std::cos(half);
    const double mid_x = (arc.start_x + arc.end_x) * 0.5;
    const double mid_y = (arc.start_y + arc.end_y) * 0.5;
    const double nx = -chord_y / chord;
    const double ny = chord_x / chord;

    const double old_center_x = arc.center_x;
    const double old_center_y = arc.center_y;
    const double candidates[2][2] = {
        {mid_x + nx * offset, mid_y + ny * offset},
        {mid_x - nx * offset, mid_y - ny * offset}};

    bool matched_side = false;
    double best_x = candidates[0][0];
    double best_y = candidates[0][1];
    double best_distance = 0.0;
    for (int side = 0; side < 2; ++side) {
      arc.center_x = candidates[side][0];
      arc.center_y = candidates[side][1];
      arc.radius = radius;
      // Only the correct side reproduces the arc's own sweep (magnitude and
      // direction), because the two sides are mirror images of each other.
      const double sweep = arc_sweep_angle(arc);
      if (std::abs(std::abs(sweep) - std::abs(target_sweep)) <= 1.0e-6 &&
          (sweep >= 0.0) == (target_sweep >= 0.0)) {
        matched_side = true;
        best_x = arc.center_x;
        best_y = arc.center_y;
        break;
      }
      const double distance = std::hypot(candidates[side][0] - old_center_x,
                                         candidates[side][1] - old_center_y);
      if (side == 0 || distance < best_distance) {
        best_distance = distance;
        best_x = candidates[side][0];
        best_y = candidates[side][1];
      }
    }
    // If neither side reproduces the stored span (a nearly flat or nearly full
    // arc, where the two sides coincide), the loop already settled on the
    // candidate closest to the centre the arc had.
    arc.center_x = best_x;
    arc.center_y = best_y;
    arc.radius = radius;
    // The centre is a real vertex owned only by this arc — move it so the
    // point the user sees sits where the arc is actually centred.
    if (!arc.center_vertex_id.empty()) {
      for (auto& vertex : parameters.vertices) {
        if (vertex.id != arc.center_vertex_id) continue;
        vertex.x = arc.center_x;
        vertex.y = arc.center_y;
        break;
      }
    }
  }
}
