ViewportSketchDimensionPrimitive make_line_line_distance_dimension_primitive(
    const SketchLine& driven_line,
    const SketchLine& reference_line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double ref_dx = reference_line.end_x - reference_line.start_x;
  const double ref_dy = reference_line.end_y - reference_line.start_y;
  const double ref_length = std::sqrt(ref_dx * ref_dx + ref_dy * ref_dy);
  if (ref_length <= 1e-6) {
    return ViewportSketchDimensionPrimitive{};
  }
  const double ux = ref_dx / ref_length;
  const double uy = ref_dy / ref_length;
  const double midpoint_x = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double midpoint_y = (driven_line.start_y + driven_line.end_y) / 2.0;
  const double t =
      (midpoint_x - reference_line.start_x) * ux +
      (midpoint_y - reference_line.start_y) * uy;
  const double foot_x = reference_line.start_x + ux * t;
  const double foot_y = reference_line.start_y + uy * t;
  const WorldPoint start = to_world_point(parameters, foot_x, foot_y);
  const WorldPoint end = to_world_point(parameters, midpoint_x, midpoint_y);
  const WorldPoint label = to_world_point(
      parameters,
      (foot_x + midpoint_x) / 2.0,
      (foot_y + midpoint_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_line_distance",
      .entity_id = driven_line.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double label_x = (foot_x + midpoint_x) / 2.0;
    const double label_y = (foot_y + midpoint_y) / 2.0;
    const double raw_offset_x = *dimension.label_x - label_x;
    const double raw_offset_y = *dimension.label_y - label_y;
    const double along = raw_offset_x * ux + raw_offset_y * uy;
    const double offset_x = ux * along;
    const double offset_y = uy * along;
    const WorldPoint shifted_start =
        to_world_point(parameters, foot_x + offset_x, foot_y + offset_y);
    const WorldPoint shifted_end =
        to_world_point(parameters, midpoint_x + offset_x, midpoint_y + offset_y);
    const WorldPoint shifted_label =
        to_world_point(parameters, label_x + offset_x, label_y + offset_y);
    primitive.dimension_start_x = shifted_start.x;
    primitive.dimension_start_y = shifted_start.y;
    primitive.dimension_start_z = shifted_start.z;
    primitive.dimension_end_x = shifted_end.x;
    primitive.dimension_end_y = shifted_end.y;
    primitive.dimension_end_z = shifted_end.z;
    primitive.label_x = shifted_label.x;
    primitive.label_y = shifted_label.y;
    primitive.label_z = shifted_label.z;
  }
  primitive.driven = dimension.driven;
  return primitive;
}

ViewportSketchDimensionPrimitive make_point_distance_dimension_primitive(
    const SketchPoint& point_a,
    const SketchPoint& point_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const bool is_horizontal = (dimension.display_as == "x");
  const bool is_vertical = (dimension.display_as == "y");

  // Default geometry: dimension line runs directly between the two points.
  WorldPoint anchor_start = to_world_point(parameters, point_a.x, point_a.y);
  WorldPoint anchor_end = to_world_point(parameters, point_b.x, point_b.y);
  WorldPoint dim_start = anchor_start;
  WorldPoint dim_end = anchor_end;

  double label_local_x = (point_a.x + point_b.x) / 2.0;
  double label_local_y = (point_a.y + point_b.y) / 2.0;

  if (is_horizontal) {
    // Horizontal dimension line at the label's Y position, spanning
    // point_a.x to point_b.x.  Extension lines drop vertically from
    // each point to the dimension line.
    const double default_label_y = label_local_y;
    const double ly = dimension.label_y.has_value()
        ? *dimension.label_y
        : default_label_y;
    dim_start = to_world_point(parameters, point_a.x, ly);
    dim_end   = to_world_point(parameters, point_b.x, ly);
    label_local_x = (point_a.x + point_b.x) / 2.0;
    label_local_y = ly;
  } else if (is_vertical) {
    // Vertical dimension line at the label's X position, spanning
    // point_a.y to point_b.y.  Extension lines run horizontally from
    // each point to the dimension line.
    const double default_label_x = label_local_x;
    const double lx = dimension.label_x.has_value()
        ? *dimension.label_x
        : default_label_x;
    dim_start = to_world_point(parameters, lx, point_a.y);
    dim_end   = to_world_point(parameters, lx, point_b.y);
    label_local_x = lx;
    label_local_y = (point_a.y + point_b.y) / 2.0;
  } else {
    // Euclidean: label offset is projected onto the perpendicular of
    // the point-to-point vector (existing behaviour).
    if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
      const double dx = point_b.x - point_a.x;
      const double dy = point_b.y - point_a.y;
      const double length = std::sqrt(dx * dx + dy * dy);
      if (length > 1e-6) {
        const double normal_x = -dy / length;
        const double normal_y = dx / length;
        const double raw_offset_x = *dimension.label_x - label_local_x;
        const double raw_offset_y = *dimension.label_y - label_local_y;
        const double along = raw_offset_x * normal_x + raw_offset_y * normal_y;
        const double offset_x = normal_x * along;
        const double offset_y = normal_y * along;
        anchor_start = to_world_point(parameters,
            point_a.x + offset_x, point_a.y + offset_y);
        anchor_end   = to_world_point(parameters,
            point_b.x + offset_x, point_b.y + offset_y);
        dim_start = anchor_start;
        dim_end   = anchor_end;
        label_local_x += offset_x;
        label_local_y += offset_y;
      }
    }
  }

  const WorldPoint label = to_world_point(parameters, label_local_x, label_local_y);

  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "point_distance",
      .entity_id = point_a.id,
      .label = format_dimension_value(dimension.value) + " mm",
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
      .driven = dimension.driven,
      .display_as = dimension.display_as,
  };
  return primitive;
}
