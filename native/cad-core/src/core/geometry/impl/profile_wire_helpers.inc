struct CircleLoop {
  double center_x;
  double center_y;
  double radius;
};

std::optional<CircleLoop> detect_circle_loop(
    const std::vector<SketchProfilePoint>& points) {
  if (points.size() < 16) {
    return std::nullopt;
  }

  double center_x = 0.0;
  double center_y = 0.0;
  for (const auto& point : points) {
    center_x += point.x;
    center_y += point.y;
  }
  center_x /= static_cast<double>(points.size());
  center_y /= static_cast<double>(points.size());

  double radius = 0.0;
  for (const auto& point : points) {
    const double dx = point.x - center_x;
    const double dy = point.y - center_y;
    radius += std::sqrt(dx * dx + dy * dy);
  }
  radius /= static_cast<double>(points.size());
  if (radius <= 0.0) {
    return std::nullopt;
  }

  const double tolerance = std::max(0.05, radius * 0.01);
  for (const auto& point : points) {
    const double dx = point.x - center_x;
    const double dy = point.y - center_y;
    const double distance = std::sqrt(dx * dx + dy * dy);
    if (std::abs(distance - radius) > tolerance) {
      return std::nullopt;
    }
  }

  return CircleLoop{.center_x = center_x, .center_y = center_y, .radius = radius};
}

std::optional<TopoDS_Wire> make_circle_wire(
    const ExtrudeFeatureParameters& parameters,
    const CircleLoop& circle) {
  const gp_Pnt center =
      to_world_point(parameters, circle.center_x, circle.center_y);
  const gp_Circ curve(gp_Ax2(center, profile_wire_normal(parameters)),
                      circle.radius);
  BRepBuilderAPI_MakeEdge edge_builder(curve);
  if (!edge_builder.IsDone()) {
    return std::nullopt;
  }
  BRepBuilderAPI_MakeWire wire_builder(edge_builder.Edge());
  if (!wire_builder.IsDone()) {
    return std::nullopt;
  }
  return wire_builder.Wire();
}

TopoDS_Wire make_profile_wire(
    const ExtrudeFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& points) {
  if (const auto circle = detect_circle_loop(points)) {
    if (const auto wire = make_circle_wire(parameters, circle.value())) {
      return wire.value();
    }
  }

  BRepBuilderAPI_MakePolygon polygon_builder;
  for (const auto& point : points) {
    polygon_builder.Add(to_world_point(parameters, point.x, point.y));
  }
  polygon_builder.Close();

  if (!polygon_builder.IsDone()) {
    throw std::runtime_error("Failed to build polygon wire");
  }
  return polygon_builder.Wire();
}

TopoDS_Wire make_profile_wire(
    const LoftSectionParameters& section,
    const std::vector<SketchProfilePoint>& points) {
  if (const auto circle = detect_circle_loop(points)) {
    const gp_Pnt center =
        to_world_point(section, circle->center_x, circle->center_y);
    const gp_Circ curve(gp_Ax2(center, profile_wire_normal(section)),
                        circle->radius);
    BRepBuilderAPI_MakeEdge edge_builder(curve);
    if (edge_builder.IsDone()) {
      BRepBuilderAPI_MakeWire wire_builder(edge_builder.Edge());
      if (wire_builder.IsDone()) {
        return wire_builder.Wire();
      }
    }
  }

  BRepBuilderAPI_MakePolygon polygon_builder;
  for (const auto& point : points) {
    polygon_builder.Add(to_world_point(section, point.x, point.y));
  }
  polygon_builder.Close();

  if (!polygon_builder.IsDone()) {
    throw std::runtime_error("Failed to build loft profile wire");
  }
  return polygon_builder.Wire();
}

TopoDS_Wire make_profile_wire(
    const RevolveFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& points) {
  if (const auto circle = detect_circle_loop(points)) {
    const gp_Pnt center =
        to_world_point(parameters, circle->center_x, circle->center_y);
    const gp_Circ curve(gp_Ax2(center, profile_wire_normal(parameters)),
                        circle->radius);
    BRepBuilderAPI_MakeEdge edge_builder(curve);
    if (edge_builder.IsDone()) {
      BRepBuilderAPI_MakeWire wire_builder(edge_builder.Edge());
      if (wire_builder.IsDone()) {
        return wire_builder.Wire();
      }
    }
  }

  BRepBuilderAPI_MakePolygon polygon_builder;
  for (const auto& point : points) {
    polygon_builder.Add(to_world_point(parameters, point.x, point.y));
  }
  polygon_builder.Close();

  if (!polygon_builder.IsDone()) {
    throw std::runtime_error("Failed to build revolve profile wire");
  }
  return polygon_builder.Wire();
}

TopoDS_Wire make_profile_wire(
    const SweepFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& points) {
  if (const auto circle = detect_circle_loop(points)) {
    const gp_Pnt center =
        to_world_point(parameters, circle->center_x, circle->center_y);
    const gp_Circ curve(gp_Ax2(center, profile_wire_normal(parameters)),
                        circle->radius);
    BRepBuilderAPI_MakeEdge edge_builder(curve);
    if (edge_builder.IsDone()) {
      BRepBuilderAPI_MakeWire wire_builder(edge_builder.Edge());
      if (wire_builder.IsDone()) {
        return wire_builder.Wire();
      }
    }
  }

  BRepBuilderAPI_MakePolygon polygon_builder;
  for (const auto& point : points) {
    polygon_builder.Add(to_world_point(parameters, point.x, point.y));
  }
  polygon_builder.Close();

  if (!polygon_builder.IsDone()) {
    throw std::runtime_error("Failed to build sweep profile wire");
  }
  return polygon_builder.Wire();
}
