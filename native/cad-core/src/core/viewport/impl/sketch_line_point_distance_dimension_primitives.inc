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
  const WorldPoint start = to_world_point(parameters, point_a.x, point_a.y);
  const WorldPoint end = to_world_point(parameters, point_b.x, point_b.y);
  const WorldPoint label = to_world_point(
      parameters,
      (point_a.x + point_b.x) / 2.0,
      (point_a.y + point_b.y) / 2.0);
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "point_distance",
      .entity_id = point_a.id,
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
      .driven = dimension.driven,
  };
}
