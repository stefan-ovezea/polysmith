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

TopoDS_Shape make_profile_face(
    const RevolveFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& outer_points,
    const std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  BRepBuilderAPI_MakeFace face_builder(
      make_profile_wire(parameters, outer_points));
  for (const auto& loop : inner_loops) {
    if (loop.size() < 3) {
      continue;
    }
    TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
    hole_wire.Reverse();
    face_builder.Add(hole_wire);
  }

  const TopoDS_Shape face = face_builder.Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build revolve profile face");
  }
  return face;
}

TopoDS_Shape make_profile_face(
    const SweepFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& outer_points,
    const std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  BRepBuilderAPI_MakeFace face_builder(make_profile_wire(parameters, outer_points));
  for (const auto& loop : inner_loops) {
    if (loop.size() < 3) {
      continue;
    }
    TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
    hole_wire.Reverse();
    face_builder.Add(hole_wire);
  }

  const TopoDS_Shape face = face_builder.Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build sweep profile face");
  }
  return face;
}

TopoDS_Shape make_polygon_prism_shape(
    const ExtrudeFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& outer_points,
    const std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  BRepBuilderAPI_MakeFace face_builder(
      make_profile_wire(parameters, outer_points));
  for (const auto& loop : inner_loops) {
    if (loop.size() < 3) {
      continue;
    }
    TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
    hole_wire.Reverse();
    face_builder.Add(hole_wire);
  }

  const TopoDS_Shape face = face_builder.Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build polygon face");
  }

  const gp_Vec direction =
      parameters.plane_frame.has_value()
          ? extrusion_vector(parameters.plane_frame.value(), parameters.depth)
          : extrusion_vector(parameters.plane_id, parameters.depth);

  const TopoDS_Shape shape = BRepPrimAPI_MakePrism(face, direction).Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build polygon extrude");
  }

  return shape;
}

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
