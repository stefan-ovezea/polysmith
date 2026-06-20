ViewportSketchDimensionPrimitive make_circle_center_distance_dimension_primitive(
    const SketchCircle& driven_circle,
    const SketchCircle& reference_circle,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint start =
      to_world_point(parameters, reference_circle.center_x, reference_circle.center_y);
  const WorldPoint end =
      to_world_point(parameters, driven_circle.center_x, driven_circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      (reference_circle.center_x + driven_circle.center_x) / 2.0,
      (reference_circle.center_y + driven_circle.center_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_center_distance",
      .entity_id = driven_circle.id,
      .label = format_dimension_label(dimension),
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
    const double dx = driven_circle.center_x - reference_circle.center_x;
    const double dy = driven_circle.center_y - reference_circle.center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length > 1e-6) {
      const double normal_x = -dy / length;
      const double normal_y = dx / length;
      const double label_x =
          (reference_circle.center_x + driven_circle.center_x) / 2.0;
      const double label_y =
          (reference_circle.center_y + driven_circle.center_y) / 2.0;
      const double raw_offset_x = *dimension.label_x - label_x;
      const double raw_offset_y = *dimension.label_y - label_y;
      const double along = raw_offset_x * normal_x + raw_offset_y * normal_y;
      const double offset_x = normal_x * along;
      const double offset_y = normal_y * along;
      const WorldPoint shifted_start = to_world_point(
          parameters,
          reference_circle.center_x + offset_x,
          reference_circle.center_y + offset_y);
      const WorldPoint shifted_end = to_world_point(
          parameters,
          driven_circle.center_x + offset_x,
          driven_circle.center_y + offset_y);
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
  }
  primitive.driven = dimension.driven;
  primitive.display_as = "";
  return primitive;
}

ViewportSketchDimensionPrimitive make_circle_line_distance_dimension_primitive(
    const SketchCircle& circle,
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double ux = length > 0.0 ? dx / length : 1.0;
  const double uy = length > 0.0 ? dy / length : 0.0;
  const double t =
      ((circle.center_x - line.start_x) * ux +
       (circle.center_y - line.start_y) * uy);
  const double foot_x = line.start_x + ux * t;
  const double foot_y = line.start_y + uy * t;
  const WorldPoint start = to_world_point(parameters, foot_x, foot_y);
  const WorldPoint end = to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      (foot_x + circle.center_x) / 2.0,
      (foot_y + circle.center_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_line_distance",
      .entity_id = circle.id,
      .label = format_dimension_label(dimension),
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
    const double label_x = (foot_x + circle.center_x) / 2.0;
    const double label_y = (foot_y + circle.center_y) / 2.0;
    const double raw_offset_x = *dimension.label_x - label_x;
    const double raw_offset_y = *dimension.label_y - label_y;
    const double along = raw_offset_x * ux + raw_offset_y * uy;
    const double offset_x = ux * along;
    const double offset_y = uy * along;
    const WorldPoint shifted_start =
        to_world_point(parameters, foot_x + offset_x, foot_y + offset_y);
    const WorldPoint shifted_end = to_world_point(
        parameters,
        circle.center_x + offset_x,
        circle.center_y + offset_y);
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
  primitive.display_as = "";
  return primitive;
}
