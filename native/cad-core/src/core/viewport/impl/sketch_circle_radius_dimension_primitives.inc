ViewportSketchDimensionPrimitive make_circle_dimension_primitive(
    const SketchCircle &circle, const SketchDimension &dimension,
    const SketchFeatureParameters &parameters, bool is_selected) {
  const WorldPoint center =
      to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint dimension_start = to_world_point(
      parameters, circle.center_x - circle.radius, circle.center_y);
  const WorldPoint dimension_end = to_world_point(
      parameters, circle.center_x + circle.radius, circle.center_y);
  const WorldPoint label = to_world_point(
      parameters, circle.center_x + circle.radius + kCircleDimensionOffset,
      circle.center_y);

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .label = "D " + format_dimension_value(dimension.value * 2.0) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = dimension_start.x,
      .anchor_start_y = dimension_start.y,
      .anchor_start_z = dimension_start.z,
      .anchor_end_x = dimension_end.x,
      .anchor_end_y = dimension_end.y,
      .anchor_end_z = dimension_end.z,
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
      .display_as = dimension.display_as,
  };
}
