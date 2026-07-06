ViewportSketchLinePrimitive make_sketch_line_primitive(const SketchLine& line,
                                                       const SketchFeatureParameters& parameters,
                                                       bool is_selected) {
  const WorldPoint start = to_world_point(parameters, line.start_x, line.start_y);
  const WorldPoint end = to_world_point(parameters, line.end_x, line.end_y);

  return ViewportSketchLinePrimitive{
      .line_id = line.id,
      .start_point_id = line.start_point_id,
      .end_point_id = line.end_point_id,
      .plane_id = parameters.plane_id,
      .start_x = start.x,
      .start_y = start.y,
      .start_z = start.z,
      .end_x = end.x,
      .end_y = end.y,
      .end_z = end.z,
      .is_selected = is_selected,
      .constraint = line.constraint,
      .is_construction = line.is_construction,
  };
}

ViewportSketchPolygonPrimitive make_sketch_polygon_primitive(
    const SketchPolygon& polygon,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, polygon.center_x, polygon.center_y);

  ViewportSketchPolygonPrimitive primitive{
      .polygon_id = polygon.id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .sides = polygon.sides,
      .mode = polygon.mode,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = polygon.radius,
      .is_selected = is_selected,
      .is_construction = polygon.is_construction,
  };

  // Compute world-space corners
  const int n = polygon.sides;
  // Align first vertex / edge midpoint with the click direction.
  double angle_offset = -M_PI / 2.0;
  if (polygon.mode == "inscribed") {
    angle_offset = std::atan2(polygon.end_y - polygon.center_y, polygon.end_x - polygon.center_x);
  } else if (polygon.mode == "circumscribed") {
    // Click is edge midpoint; rotate so edge 0 sits there.
    angle_offset = std::atan2(polygon.end_y - polygon.center_y, polygon.end_x - polygon.center_x) + M_PI / n;
  } else if (polygon.mode == "edge") {
    angle_offset = std::atan2(polygon.start_y - polygon.center_y, polygon.start_x - polygon.center_x);
  }
  for (int i = 0; i < n; ++i) {
    double angle = angle_offset + 2.0 * M_PI * i / n;
    double local_x = polygon.center_x + polygon.radius * std::cos(angle);
    double local_y = polygon.center_y + polygon.radius * std::sin(angle);
    if (polygon.mode == "circumscribed") {
      double r = polygon.radius / std::cos(M_PI / n);
      local_x = polygon.center_x + r * std::cos(angle);
      local_y = polygon.center_y + r * std::sin(angle);
    }
    const WorldPoint corner = to_world_point(parameters, local_x, local_y);
    primitive.corner_x.push_back(corner.x);
    primitive.corner_y.push_back(corner.y);
    primitive.corner_z.push_back(corner.z);
  }
  return primitive;
}

ViewportSketchCirclePrimitive make_sketch_circle_primitive(
    const SketchCircle& circle,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, circle.center_x, circle.center_y);

  return ViewportSketchCirclePrimitive{
      .circle_id = circle.id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = circle.radius,
      .is_selected = is_selected,
      .is_construction = circle.is_construction,
  };
}

ViewportSketchArcPrimitive make_sketch_arc_primitive(
    const SketchArc& arc,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, arc.center_x, arc.center_y);
  const WorldPoint start = to_world_point(parameters, arc.start_x, arc.start_y);
  const WorldPoint end = to_world_point(parameters, arc.end_x, arc.end_y);

  return ViewportSketchArcPrimitive{
      .arc_id = arc.id,
      .start_point_id = arc.start_point_id,
      .end_point_id = arc.end_point_id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = arc.radius,
      .start_x = start.x,
      .start_y = start.y,
      .start_z = start.z,
      .end_x = end.x,
      .end_y = end.y,
      .end_z = end.z,
      .ccw = arc.ccw,
      .is_selected = is_selected,
      .is_construction = arc.is_construction,
  };
}

ViewportSketchVertexPrimitive make_sketch_point_primitive(
    const SketchVertex& point,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint world = to_world_point(parameters, point.x, point.y);

  return ViewportSketchVertexPrimitive{
      .point_id = point.id,
      .vertex_id = point.vertex_id,
      .plane_id = parameters.plane_id,
      .kind = point.kind,
      .position_x = world.x,
      .position_y = world.y,
      .position_z = world.z,
      .is_fixed = point.is_fixed,
      .is_selected = is_selected,
      .geometry_owner_ids = point.geometry_owner_ids,
      .is_projected = point.is_projected,
      .source_type = point.source_type,
      .source_feature_id = point.source_feature_id,
      .source_edge_id = point.source_edge_id,
  };
}

#include "core/viewport/impl/sketch_dimension_primitives.inc"

ViewportSketchConstraintPrimitive make_line_constraint_primitive(
    const SketchLine& line,
    const std::string& plane_id,
    const std::string& kind,
    const std::string& label,
    bool is_selected,
    const std::optional<std::string>& related_entity_id = std::nullopt,
    double badge_offset_multiplier = 1.0,
    bool driven = false) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double normal_x = length > 0.0 ? -dy / length : 0.0;
  const double normal_y = length > 0.0 ? dx / length : 1.0;
  const double offset = kConstraintBadgeOffset * badge_offset_multiplier;
  const WorldPoint position = to_world_point(
      plane_id,
      (line.start_x + line.end_x) / 2.0 + normal_x * offset,
      (line.start_y + line.end_y) / 2.0 + normal_y * offset);

  auto prim = ViewportSketchConstraintPrimitive{
      .constraint_id =
          (kind == "equal_length" || kind == "perpendicular") &&
                  related_entity_id.has_value()
              ? "constraint-" + kind + "-" + line.id + "-" + related_entity_id.value()
              : "constraint-" + kind + "-" + line.id,
      .plane_id = plane_id,
      .kind = kind,
      .entity_id = line.id,
      .related_entity_id = related_entity_id,
      .label = label,
      .is_selected = is_selected,
      .position_x = position.x,
      .position_y = position.y,
      .position_z = position.z,
      .driven = driven,
  };
  return prim;
}

ViewportSketchConstraintPrimitive make_point_constraint_primitive(
    const SketchVertex& point,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint position = to_world_point(
      parameters, point.x, point.y, kSketchPlaneOffset + kConstraintBadgeOffset);

  return ViewportSketchConstraintPrimitive{
      .constraint_id = "constraint-fixed-" + point.id,
      .plane_id = parameters.plane_id,
      .kind = "fixed",
      .entity_id = point.id,
      .related_entity_id = std::nullopt,
      .label = "FIX",
      .is_selected = is_selected,
      .position_x = position.x,
      .position_y = position.y,
      .position_z = position.z,
  };
}

ViewportSketchProfilePrimitive make_rectangle_profile_primitive(
    const PolygonSketchProfile& profile, bool is_selected) {
  return ViewportSketchProfilePrimitive{
      .profile_id = profile.id,
      .plane_id = profile.plane_id,
      .plane_frame = profile.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(profile.plane_frame.value()))
                         : std::nullopt,
      .profile_kind = "polygon",
      .profile_points = profile.points,
      .inner_loops = profile.inner_loops,
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .is_selected = is_selected,
  };
}

ViewportSketchProfilePrimitive make_circle_profile_primitive(
    const CircleSketchProfile& profile, bool is_selected) {
  return ViewportSketchProfilePrimitive{
      .profile_id = profile.id,
      .plane_id = profile.plane_id,
      .plane_frame = profile.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(profile.plane_frame.value()))
                         : std::nullopt,
      .profile_kind = "circle",
      .profile_points = {},
      .inner_loops = {},
      .start_x = profile.center_x,
      .start_y = profile.center_y,
      .width = 0.0,
      .height = 0.0,
      .radius = profile.radius,
      .is_selected = is_selected,
  };
}
