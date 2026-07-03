ViewportSketchDimensionPrimitive make_circle_center_distance_dimension_primitive(
    const SketchCircle& driven_circle,
    const SketchCircle& reference_circle,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = driven_circle.center_x - reference_circle.center_x;
  const double dy = driven_circle.center_y - reference_circle.center_y;
  const double len = std::sqrt(dx * dx + dy * dy);
  double off_x = 0, off_y = kLineDimensionOffset;
  if (len > 1e-6) {
    off_x = -dy / len * kLineDimensionOffset;
    off_y =  dx / len * kLineDimensionOffset;
  }
  return make_offset_dimension_primitive(
      reference_circle.center_x, reference_circle.center_y,
      driven_circle.center_x, driven_circle.center_y,
      off_x, off_y,
      dimension.id, parameters.plane_id, "circle_center_distance", driven_circle.id,
      format_dimension_label(dimension), is_selected,
      dimension.driven, "", parameters, &dimension);
}

ViewportSketchDimensionPrimitive make_circle_line_distance_dimension_primitive(
    const SketchCircle& circle,
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double len = std::sqrt(dx * dx + dy * dy);
  const double ux = len > 0.0 ? dx / len : 1.0;
  const double uy = len > 0.0 ? dy / len : 0.0;
  const double t = (circle.center_x - line.start_x) * ux + (circle.center_y - line.start_y) * uy;
  const double foot_x = line.start_x + ux * t;
  const double foot_y = line.start_y + uy * t;

  return make_offset_dimension_primitive(
      foot_x, foot_y, circle.center_x, circle.center_y,
      ux * kLineDimensionOffset, uy * kLineDimensionOffset,
      dimension.id, parameters.plane_id, "circle_line_distance", circle.id,
      format_dimension_label(dimension), is_selected,
      dimension.driven, "", parameters, &dimension);
}