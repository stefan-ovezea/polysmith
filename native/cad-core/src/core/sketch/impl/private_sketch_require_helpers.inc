SketchDimension& require_dimension(SketchFeatureParameters& parameters,
                                   const std::string& dimension_id) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });

  if (dimension_it == parameters.dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  return *dimension_it;
}

SketchLine& require_line(SketchFeatureParameters& parameters,
                         const std::string& line_id) {
  const auto line_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == parameters.lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  return *line_it;
}

SketchCircle& require_circle(SketchFeatureParameters& parameters,
                             const std::string& circle_id) {
  const auto circle_it = std::find_if(
      parameters.circles.begin(),
      parameters.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });

  if (circle_it == parameters.circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  return *circle_it;
}

SketchPoint& require_point(SketchFeatureParameters& parameters,
                           const std::string& point_id) {
  const auto point_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& p) { return p.id == point_id; });

  if (point_it == parameters.points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  return *point_it;
}
