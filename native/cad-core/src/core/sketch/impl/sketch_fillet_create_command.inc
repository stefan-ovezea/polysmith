void add_sketch_fillet(FeatureEntry &feature, int fillet_index,
                       int trim_a_point_index, int trim_b_point_index,
                       int arc_index, const std::string &corner_vertex_id,
                       const std::string &line_a_id,
                       const std::string &line_b_id, double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch fillets");
  }
  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch fillets must have a positive radius");
  }
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
}
