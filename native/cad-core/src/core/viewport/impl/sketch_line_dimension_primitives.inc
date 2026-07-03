// Shared helper: build a ViewportSketchDimensionPrimitive for any linear
// dimension (line_length, point_distance, line_line_distance, etc.).
// The caller provides two anchor points in sketch-local coords and an
// offset direction; the helper handles offset, label placement, label
// position override, and struct population.
static ViewportSketchDimensionPrimitive make_offset_dimension_primitive(
    double anchor_a_x, double anchor_a_y,
    double anchor_b_x, double anchor_b_y,
    double offset_x, double offset_y,
    const std::string& dimension_id,
    const std::string& plane_id,
    const std::string& kind,
    const std::string& entity_id,
    const std::string& label_str,
    bool is_selected,
    bool driven,
    const std::string& display_as,
    const SketchFeatureParameters& parameters,
    const SketchDimension* dimension = nullptr) {

  const double mid_x = (anchor_a_x + anchor_b_x) / 2.0;
  const double mid_y = (anchor_a_y + anchor_b_y) / 2.0;

  double off_x = offset_x;
  double off_y = offset_y;

  // If the dimension has a stored label position, use it to determine
  // the actual offset (respecting the user's drag placement).
  if (dimension && dimension->label_x.has_value() && dimension->label_y.has_value()) {
    const double raw_off_x = *dimension->label_x - mid_x;
    const double raw_off_y = *dimension->label_y - mid_y;
    const double off_len = std::sqrt(offset_x * offset_x + offset_y * offset_y);
    if (off_len > 1e-6) {
      const double ux = offset_x / off_len;
      const double uy = offset_y / off_len;
      const double along = raw_off_x * ux + raw_off_y * uy;
      off_x = ux * along;
      off_y = uy * along;
    }
  }

  auto wp = [&](double x, double y) {
    return to_world_point(parameters, x, y);
  };

  const WorldPoint anchor_start = wp(anchor_a_x, anchor_a_y);
  const WorldPoint anchor_end   = wp(anchor_b_x, anchor_b_y);
  const WorldPoint dim_start    = wp(anchor_a_x + off_x, anchor_a_y + off_y);
  const WorldPoint dim_end      = wp(anchor_b_x + off_x, anchor_b_y + off_y);
  const WorldPoint label        = wp(mid_x + off_x, mid_y + off_y);

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension_id,
      .plane_id = plane_id,
      .kind = kind,
      .entity_id = entity_id,
      .label = label_str,
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dim_start.x,
      .dimension_start_y = dim_start.y,
      .dimension_start_z = dim_start.z,
      .dimension_end_x = dim_end.x,
      .dimension_end_y = dim_end.y,
      .dimension_end_z = dim_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
      .driven = driven,
      .display_as = display_as,
  };
}

ViewportSketchDimensionPrimitive make_line_dimension_primitive(
    const SketchLine &line, const SketchDimension &dimension,
    const SketchFeatureParameters &parameters, bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  double normal_x = length > 0.0 ? -dy / length : 0.0;
  double normal_y = length > 0.0 ? dx / length : 1.0;

  const double midpoint_x = (line.start_x + line.end_x) / 2.0;
  const double midpoint_y = (line.start_y + line.end_y) / 2.0;
  const SketchCentroid2D centroid = compute_sketch_centroid_2d(parameters);
  const double to_outside_x = centroid.has_value ? midpoint_x - centroid.x : 0.0;
  const double to_outside_y = centroid.has_value ? midpoint_y - centroid.y : 0.0;
  const double to_outside_mag = std::sqrt(to_outside_x * to_outside_x + to_outside_y * to_outside_y);
  if (centroid.has_value) {
    if (normal_x * to_outside_x + normal_y * to_outside_y < 0.0) {
      normal_x = -normal_x; normal_y = -normal_y;
    }
  }
  if (dy < 0.0 && to_outside_mag < 0.5) {
    normal_x = -normal_x; normal_y = -normal_y;
  }

  return make_offset_dimension_primitive(
      line.start_x, line.start_y, line.end_x, line.end_y,
      normal_x * kLineDimensionOffset, normal_y * kLineDimensionOffset,
      dimension.id, parameters.plane_id, "line_length", line.id,
      format_dimension_label(dimension), is_selected,
      dimension.driven, "", parameters, &dimension);
}

ViewportSketchDimensionPrimitive make_line_angle_dimension_primitive(
    const SketchLine &line, const SketchDimension &dimension,
    const SketchFeatureParameters &parameters, bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double ref_ux = 1.0, ref_uy = 0.0;
  double line_ux = 1.0, line_uy = 0.0;
  if (length > 0.0) { line_ux = dx / length; line_uy = dy / length; }

  const double pivot_x = line.start_x, pivot_y = line.start_y;
  double bx = ref_ux + line_ux, by = ref_uy + line_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) { bx = -ref_uy; by = ref_ux; blen = 1.0; }
  const double bisector_ux = bx / blen, bisector_uy = by / blen;
  const double kArcRadius = std::max(8.0, std::min(length, 500.0));

  auto wp = [&](double x, double y) { return to_world_point(parameters, x, y); };
  const WorldPoint anchor_start = wp(pivot_x + ref_ux * kArcRadius, pivot_y + ref_uy * kArcRadius);
  const WorldPoint anchor_end   = wp(pivot_x + line_ux * 4.0, pivot_y + line_uy * 4.0);
  const WorldPoint dim_start    = wp(pivot_x + ref_ux * kArcRadius, pivot_y + ref_uy * kArcRadius);
  const WorldPoint dim_end      = wp(pivot_x + line_ux * kArcRadius, pivot_y + line_uy * kArcRadius);
  const WorldPoint label        = wp(pivot_x + bisector_ux * kArcRadius, pivot_y + bisector_uy * kArcRadius);
  const WorldPoint ref_start    = wp(pivot_x, pivot_y);
  const WorldPoint ref_end      = wp(pivot_x + ref_ux * kArcRadius, pivot_y + ref_uy * kArcRadius);
  const WorldPoint arc_ctr      = wp(pivot_x, pivot_y);

  const bool arc_ccw = (ref_ux * line_uy - ref_uy * line_ux) >= 0.0;
  const double degrees = dimension.value * 180.0 / 3.14159265358979323846;
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id, .plane_id = parameters.plane_id,
      .kind = "line_angle", .entity_id = line.id,
      .label = format_dimension_value(degrees) + "\xc2\xb0", .is_selected = is_selected,
      .anchor_start_x = anchor_start.x, .anchor_start_y = anchor_start.y, .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x, .anchor_end_y = anchor_end.y, .anchor_end_z = anchor_end.z,
      .dimension_start_x = dim_start.x, .dimension_start_y = dim_start.y, .dimension_start_z = dim_start.z,
      .dimension_end_x = dim_end.x, .dimension_end_y = dim_end.y, .dimension_end_z = dim_end.z,
      .label_x = label.x, .label_y = label.y, .label_z = label.z,
      .arc_center_x = arc_ctr.x, .arc_center_y = arc_ctr.y, .arc_center_z = arc_ctr.z,
      .arc_radius = kArcRadius, .arc_start_angle = std::atan2(ref_uy, ref_ux),
      .arc_end_angle = std::atan2(line_uy, line_ux), .arc_ccw = arc_ccw,
      .ref_line_start_x = ref_start.x, .ref_line_start_y = ref_start.y, .ref_line_start_z = ref_start.z,
      .ref_line_end_x = ref_end.x, .ref_line_end_y = ref_end.y, .ref_line_end_z = ref_end.z,
      .driven = dimension.driven, .display_as = "",
  };
}