void sync_line_dimension(SketchFeatureParameters& parameters,
                         const SketchLine& line) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "line_length" && dimension.entity_id == line.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = measure_line_length(line);
  }
}

void sync_circle_dimension(SketchFeatureParameters& parameters,
                           const SketchCircle& circle) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "circle_radius" && dimension.entity_id == circle.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = circle.radius;
  }
}
