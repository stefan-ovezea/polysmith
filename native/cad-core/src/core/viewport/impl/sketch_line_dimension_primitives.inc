ViewportSketchDimensionPrimitive make_line_dimension_primitive(
    const SketchLine &line, const SketchDimension &dimension,
    const SketchFeatureParameters &parameters, bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  double normal_x = length > 0.0 ? -dy / length : 0.0;
  double normal_y = length > 0.0 ? dx / length : 1.0;

  // Flip the normal so it points *away* from the centroid of the rest
  // of the sketch. For a rectangle (or any closed profile) this puts
  // every edge's dimension line on the outside of the shape.
  const double midpoint_x = (line.start_x + line.end_x) / 2.0;
  const double midpoint_y = (line.start_y + line.end_y) / 2.0;
  const SketchCentroid2D centroid = compute_sketch_centroid_2d(parameters);
  const double to_outside_x =
      centroid.has_value ? midpoint_x - centroid.x : 0.0;
  const double to_outside_y =
      centroid.has_value ? midpoint_y - centroid.y : 0.0;
  const double to_outside_mag =
      std::sqrt(to_outside_x * to_outside_x + to_outside_y * to_outside_y);
  if (centroid.has_value) {
    if (normal_x * to_outside_x + normal_y * to_outside_y < 0.0) {
      normal_x = -normal_x;
      normal_y = -normal_y;
    }
  }

  // After the centroid flip, ensure the length dimension sits on the
  // opposite side of the line from the angle arc — but only as a
  // tiebreaker when the centroid is ambiguous (the line's midpoint
  // is near the centroid, i.e. isolated line or single entity).
  // When the centroid is well-defined (multi-entity sketch), let
  // the centroid-aware flip handle collision avoidance.
  // The angle arc sweeps from the +X reference (horizontal right)
  // to the line direction. For dy > 0 (up-right line), the arc is at
  // sin(θ) ≤ sin(line_angle) → arc is BELOW the line → normal already
  // above → no flip needed.
  // For dy < 0 (down-right line), the arc is at sin(θ) ≥ sin(line_angle)
  // → arc is ABOVE the line → normal also points above → flip to below.
  if (dy < 0.0 && to_outside_mag < 0.5) {
    normal_x = -normal_x;
    normal_y = -normal_y;
  }

  const WorldPoint anchor_start =
      to_world_point(parameters, line.start_x, line.start_y);
  const WorldPoint anchor_end =
      to_world_point(parameters, line.end_x, line.end_y);
  const WorldPoint dimension_start =
      to_world_point(parameters, line.start_x + normal_x * kLineDimensionOffset,
                     line.start_y + normal_y * kLineDimensionOffset);
  const WorldPoint dimension_end =
      to_world_point(parameters, line.end_x + normal_x * kLineDimensionOffset,
                     line.end_y + normal_y * kLineDimensionOffset);
  const WorldPoint label = to_world_point(
      parameters, midpoint_x + normal_x * (kLineDimensionOffset + 1.5),
      midpoint_y + normal_y * (kLineDimensionOffset + 1.5));

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_length",
      .entity_id = line.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
      .driven = dimension.driven,
  };
}

ViewportSketchDimensionPrimitive make_line_angle_dimension_primitive(
    const SketchLine &line, const SketchDimension &dimension,
    const SketchFeatureParameters &parameters, bool is_selected) {
  // Render the line-angle dimension as an angle arc from the horizontal
  // reference (+X axis) to the line's direction, pivoted at the line's
  // start point. This makes it visually distinct from a line_length
  // dimension — the UI renders angle kinds as arcs with arrowheads.
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);

  // Reference ray: horizontal right (+X axis).
  const double ref_ux = 1.0;
  const double ref_uy = 0.0;

  // Line direction ray (from start to end).
  double line_ux = 1.0;
  double line_uy = 0.0;
  if (length > 0.0) {
    line_ux = dx / length;
    line_uy = dy / length;
  }

  const double pivot_x = line.start_x;
  const double pivot_y = line.start_y;

  // Bisector direction (sum of unit vectors).
  double bx = ref_ux + line_ux;
  double by = ref_uy + line_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) {
    bx = -ref_uy;
    by = ref_ux;
    blen = 1.0;
  }
  const double bisector_ux = bx / blen;
  const double bisector_uy = by / blen;

  const double kArcRadius = std::max(8.0, std::min(length, 500.0));
  constexpr double kAnchorRadius = 4.0;
  const double kLabelRadius = kArcRadius;

  const WorldPoint anchor_start = to_world_point(
      parameters, pivot_x + ref_ux * kArcRadius, pivot_y + ref_uy * kArcRadius);
  const WorldPoint anchor_end =
      to_world_point(parameters, pivot_x + line_ux * kAnchorRadius,
                     pivot_y + line_uy * kAnchorRadius);
  const WorldPoint dimension_start = to_world_point(
      parameters, pivot_x + ref_ux * kArcRadius, pivot_y + ref_uy * kArcRadius);
  const WorldPoint dimension_end =
      to_world_point(parameters, pivot_x + line_ux * kArcRadius,
                     pivot_y + line_uy * kArcRadius);
  const WorldPoint label =
      to_world_point(parameters, pivot_x + bisector_ux * kLabelRadius,
                     pivot_y + bisector_uy * kLabelRadius);

  // Compute arc sweep: from reference direction to line direction.
  // Use cross-product sign to determine CCW vs CW.
  const double sweep_cross = ref_ux * line_uy - ref_uy * line_ux;
  const bool arc_ccw = sweep_cross >= 0.0;
  const double arc_start_angle = std::atan2(ref_uy, ref_ux);
  const double arc_end_angle = std::atan2(line_uy, line_ux);

  // Compute reference line endpoints: from pivot along reference direction.
  const WorldPoint ref_line_start =
      to_world_point(parameters, pivot_x, pivot_y);
  const WorldPoint ref_line_end = to_world_point(
      parameters, pivot_x + ref_ux * kArcRadius, pivot_y + ref_uy * kArcRadius);

  // Arc center in world space (the pivot).
  const WorldPoint arc_center_world =
      to_world_point(parameters, pivot_x, pivot_y);

  const double degrees = dimension.value * 180.0 / 3.14159265358979323846;
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_angle",
      .entity_id = line.id,
      .label = format_dimension_value(degrees) + "\xc2\xb0",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,

      // Angle arc geometry
      .arc_center_x = arc_center_world.x,
      .arc_center_y = arc_center_world.y,
      .arc_center_z = arc_center_world.z,
      .arc_radius = kArcRadius,
      .arc_start_angle = arc_start_angle,
      .arc_end_angle = arc_end_angle,
      .arc_ccw = arc_ccw,

      // Reference line (along reference direction from pivot)
      .ref_line_start_x = ref_line_start.x,
      .ref_line_start_y = ref_line_start.y,
      .ref_line_start_z = ref_line_start.z,
      .ref_line_end_x = ref_line_end.x,
      .ref_line_end_y = ref_line_end.y,
      .ref_line_end_z = ref_line_end.z,
      .driven = dimension.driven,
  };
}
