void add_sketch_chamfer(FeatureEntry &feature, int chamfer_index,
                        int trim_a_point_index, int trim_b_point_index,
                        int line_index, const std::string &corner_vertex_id,
                        const std::string &line_a_id,
                        const std::string &line_b_id, double distance_a,
                        double distance_b) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch chamfers");
  }
  if (distance_a <= kMinimumSketchDimensionValue ||
      distance_b <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch chamfer distances must be positive");
  }
  if (line_a_id == line_b_id) {
    throw std::runtime_error("Sketch chamfer requires two distinct lines");
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
        "Sketch chamfer: corner point must be shared by both lines");
  }

  // Refuse to chamfer a line that's already part of another chamfer at
  // this corner. Lines chamfered at the *other* end are fine.
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
          "Sketch chamfer: this corner is already chamfered");
    }
  }

  // A corner holding both a fillet and a chamfer is ambiguous — reject
  // the combination outright.
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
          "Sketch chamfer: this corner is already filleted");
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
        "Sketch chamfer: both lines must have non-zero length");
  }
  const double ux_a = out_ax / len_a;
  const double uy_a = out_ay / len_a;
  const double ux_b = out_bx / len_b;
  const double uy_b = out_by / len_b;

  const double cross = ux_a * uy_b - uy_a * ux_b;
  if (std::abs(cross) <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch chamfer: the two lines are parallel or colinear");
  }

  // The trim distance is simply the requested distance along each
  // outgoing direction — no angle math (unlike the fillet).
  if (distance_a + kMinimumSketchDimensionValue >= len_a ||
      distance_b + kMinimumSketchDimensionValue >= len_b) {
    throw std::runtime_error(
        "Sketch chamfer: distance is too large for at least one of the lines");
  }

  const double trim_a_x = corner_x + distance_a * ux_a;
  const double trim_a_y = corner_y + distance_a * uy_a;
  const double trim_b_x = corner_x + distance_b * ux_b;
  const double trim_b_y = corner_y + distance_b * uy_b;

  // Allocate generated entity ids. Trim points share the same
  // namespace as line endpoints (vertex-N), the chamfer line the same
  // as user lines (line-N). The chamfer record uses chamfer-N.
  const std::string trim_a_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
  const std::string trim_b_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
  const std::string chamfer_line_id = "line-" + std::to_string(line_index);
  const std::string chamfer_id = "chamfer-" + std::to_string(chamfer_index);

  // Mutate the lines: replace their corner-pointing endpoint with the
  // new trim point. Also update the cached coords so they match the
  // trim position right away. The recompute pass refreshes these on
  // every subsequent edit.
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

  // Append the generated chamfer line (its endpoints reference the new
  // trim points) and the chamfer record.
  parameters.lines.push_back(SketchLine{
      .id = chamfer_line_id,
      .start_vertex_id = trim_a_id,
      .end_vertex_id = trim_b_id,
      .start_x = trim_a_x,
      .start_y = trim_a_y,
      .end_x = trim_b_x,
      .end_y = trim_b_y,
      .constraint = std::nullopt,
      .is_construction = false,
  });
  parameters.chamfers.push_back(SketchChamfer{
      .id = chamfer_id,
      .corner_vertex_id = corner_vertex_id,
      .corner_x = corner_x,
      .corner_y = corner_y,
      .line_a_id = line_a_id,
      .line_b_id = line_b_id,
      .trim_a_vertex_id = trim_a_id,
      .trim_b_vertex_id = trim_b_id,
      .chamfer_line_id = chamfer_line_id,
      .distance_a = distance_a,
      .distance_b = distance_b,
  });

  refresh_sketch_derived_state(feature);
}
