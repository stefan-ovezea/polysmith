ViewportSketchDimensionPrimitive make_line_line_distance_dimension_primitive(
    const SketchLine& driven_line,
    const SketchLine& reference_line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double ref_dx = reference_line.end_x - reference_line.start_x;
  const double ref_dy = reference_line.end_y - reference_line.start_y;
  const double ref_len = std::sqrt(ref_dx * ref_dx + ref_dy * ref_dy);
  if (ref_len <= 1e-6) return ViewportSketchDimensionPrimitive{};
  const double ux = ref_dx / ref_len, uy = ref_dy / ref_len;
  const double mid_x = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double mid_y = (driven_line.start_y + driven_line.end_y) / 2.0;
  const double t = (mid_x - reference_line.start_x) * ux + (mid_y - reference_line.start_y) * uy;
  const double foot_x = reference_line.start_x + ux * t;
  const double foot_y = reference_line.start_y + uy * t;

  return make_offset_dimension_primitive(
      foot_x, foot_y, mid_x, mid_y,
      ux * kLineDimensionOffset, uy * kLineDimensionOffset,
      dimension.id, parameters.plane_id, "line_line_distance", driven_line.id,
      format_dimension_label(dimension), is_selected,
      dimension.driven, "", parameters, &dimension);
}

ViewportSketchDimensionPrimitive make_point_distance_dimension_primitive(
    const SketchPoint& point_a,
    const SketchPoint& point_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const bool is_h = (dimension.display_as == "x");
  const bool is_v = (dimension.display_as == "y");

  // H/V dimensions need per-point offsets (not uniform) because the
  // dimension line must be horizontal/vertical at a constant Y/X while
  // the anchor points may have different coordinates.
  auto wp = [&](double x, double y) { return to_world_point(parameters, x, y); };
  if (is_h) {
    double ly = dimension.label_y.has_value() ? *dimension.label_y
                : (point_a.y + point_b.y) / 2.0;
    auto a1 = wp(point_a.x, point_a.y), a2 = wp(point_b.x, point_b.y);
    auto d1 = wp(point_a.x, ly),           d2 = wp(point_b.x, ly);
    auto lb = wp((point_a.x + point_b.x) / 2.0, ly);
    return ViewportSketchDimensionPrimitive{
        .dimension_id = dimension.id, .plane_id = parameters.plane_id,
        .kind = "point_distance", .entity_id = point_a.id,
        .label = format_dimension_label(dimension), .is_selected = is_selected,
        .anchor_start_x = a1.x, .anchor_start_y = a1.y, .anchor_start_z = a1.z,
        .anchor_end_x = a2.x, .anchor_end_y = a2.y, .anchor_end_z = a2.z,
        .dimension_start_x = d1.x, .dimension_start_y = d1.y, .dimension_start_z = d1.z,
        .dimension_end_x = d2.x, .dimension_end_y = d2.y, .dimension_end_z = d2.z,
        .label_x = lb.x, .label_y = lb.y, .label_z = lb.z,
        .driven = dimension.driven, .display_as = dimension.display_as,
    };
  }
  if (is_v) {
    double lx = dimension.label_x.has_value() ? *dimension.label_x
                : (point_a.x + point_b.x) / 2.0;
    auto a1 = wp(point_a.x, point_a.y), a2 = wp(point_b.x, point_b.y);
    auto d1 = wp(lx, point_a.y),           d2 = wp(lx, point_b.y);
    auto lb = wp(lx, (point_a.y + point_b.y) / 2.0);
    return ViewportSketchDimensionPrimitive{
        .dimension_id = dimension.id, .plane_id = parameters.plane_id,
        .kind = "point_distance", .entity_id = point_a.id,
        .label = format_dimension_label(dimension), .is_selected = is_selected,
        .anchor_start_x = a1.x, .anchor_start_y = a1.y, .anchor_start_z = a1.z,
        .anchor_end_x = a2.x, .anchor_end_y = a2.y, .anchor_end_z = a2.z,
        .dimension_start_x = d1.x, .dimension_start_y = d1.y, .dimension_start_z = d1.z,
        .dimension_end_x = d2.x, .dimension_end_y = d2.y, .dimension_end_z = d2.z,
        .label_x = lb.x, .label_y = lb.y, .label_z = lb.z,
        .driven = dimension.driven, .display_as = dimension.display_as,
    };
  }
  // Euclidean: offset perpendicular to point-to-point vector.
  const double dx = point_b.x - point_a.x;
  const double dy = point_b.y - point_a.y;
  const double vlen = std::sqrt(dx * dx + dy * dy);
  double off_x = 0, off_y = kLineDimensionOffset;
  if (vlen > 1e-6) {
    off_x = -dy / vlen * kLineDimensionOffset;
    off_y =  dx / vlen * kLineDimensionOffset;
  }
  return make_offset_dimension_primitive(
      point_a.x, point_a.y, point_b.x, point_b.y,
      off_x, off_y,
      dimension.id, parameters.plane_id, "point_distance", point_a.id,
      format_dimension_label(dimension), is_selected,
      dimension.driven, dimension.display_as, parameters, &dimension);
}