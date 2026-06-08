std::pair<double, double> profile_centroid(
    const std::vector<SketchProfilePoint>& points) {
  double x = 0.0;
  double y = 0.0;
  for (const auto& point : points) {
    x += point.x;
    y += point.y;
  }
  const double count = std::max<size_t>(points.size(), 1);
  return {x / count, y / count};
}

std::vector<SketchProfilePoint> radial_offset_loop(
    const std::vector<SketchProfilePoint>& points,
    double offset) {
  if (points.empty() || std::abs(offset) <= 1.0e-9) {
    return points;
  }
  const auto [center_x, center_y] = profile_centroid(points);
  std::vector<SketchProfilePoint> result;
  result.reserve(points.size());
  for (const auto& point : points) {
    const double dx = point.x - center_x;
    const double dy = point.y - center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= 1.0e-9) {
      result.push_back(point);
      continue;
    }
    const double scale = (length + offset) / length;
    result.push_back({center_x + dx * scale, center_y + dy * scale});
  }
  return result;
}

void apply_closed_thin_profile(
    const ExtrudeFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& source_points,
    std::vector<SketchProfilePoint>& outer_points,
    std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  if (!parameters.thin.enabled) {
    outer_points = source_points;
    return;
  }

  const double thickness = parameters.thin.thickness;
  double outer_offset = 0.0;
  double inner_offset = -thickness;
  if (parameters.thin.placement == "center") {
    outer_offset = thickness * 0.5;
    inner_offset = -thickness * 0.5;
  } else if (parameters.thin.placement == "outside") {
    outer_offset = thickness;
    inner_offset = 0.0;
  }

  outer_points = radial_offset_loop(source_points, outer_offset);
  std::vector<SketchProfilePoint> inner = radial_offset_loop(source_points, inner_offset);
  if (inner.size() >= 3) {
    std::reverse(inner.begin(), inner.end());
    inner_loops.push_back(std::move(inner));
  }
}

std::vector<SketchProfilePoint> make_open_thin_wall(
    const ExtrudeFeatureParameters& parameters) {
  const auto& chain = parameters.profile_points;
  if (chain.size() < 2) {
    throw std::runtime_error("Open thin extrude requires at least two points");
  }
  const double thickness = parameters.thin.thickness;
  const double left_offset =
      parameters.thin.placement == "center" ? thickness * 0.5
      : parameters.thin.placement == "outside" ? thickness
                                                : 0.0;
  const double right_offset =
      parameters.thin.placement == "center" ? -thickness * 0.5
      : parameters.thin.placement == "outside" ? 0.0
                                                : -thickness;

  auto offset_point = [&](size_t index, double offset) {
    double tx = 0.0;
    double ty = 0.0;
    if (index > 0) {
      tx += chain[index].x - chain[index - 1].x;
      ty += chain[index].y - chain[index - 1].y;
    }
    if (index + 1 < chain.size()) {
      tx += chain[index + 1].x - chain[index].x;
      ty += chain[index + 1].y - chain[index].y;
    }
    const double length = std::sqrt(tx * tx + ty * ty);
    if (length <= 1.0e-9) {
      return chain[index];
    }
    const double nx = -ty / length;
    const double ny = tx / length;
    return SketchProfilePoint{chain[index].x + nx * offset,
                              chain[index].y + ny * offset};
  };

  std::vector<SketchProfilePoint> wall;
  wall.reserve(chain.size() * 2);
  for (size_t index = 0; index < chain.size(); ++index) {
    wall.push_back(offset_point(index, left_offset));
  }
  for (size_t index = chain.size(); index-- > 0;) {
    wall.push_back(offset_point(index, right_offset));
  }
  return wall;
}
