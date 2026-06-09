TopoDS_Shape make_plugin_box_at(double x,
                                double y,
                                double z,
                                double width,
                                double depth,
                                double height) {
  return BRepPrimAPI_MakeBox(gp_Pnt(x, z, y), width, height, depth).Shape();
}

TopoDS_Shape make_plugin_cylinder_at(double x,
                                     double y,
                                     double z,
                                     double radius,
                                     double height) {
  const gp_Ax2 axis(gp_Pnt(x, z, y), gp_Dir(0.0, 1.0, 0.0));
  return BRepPrimAPI_MakeCylinder(axis, radius, height).Shape();
}

gp_Pnt make_plugin_point(double x, double y, double z) {
  return gp_Pnt(x, z, y);
}

TopoDS_Wire make_plugin_rounded_rectangle_wire_at(double x,
                                                  double y,
                                                  double z,
                                                  double width,
                                                  double depth,
                                                  double radius) {
  const double r = std::max(
      0.0,
      std::min(radius, std::min(width, depth) / 2.0));
  if (r <= 0.0) {
    return BRepBuilderAPI_MakePolygon(make_plugin_point(x, y, z),
                                      make_plugin_point(x + width, y, z),
                                      make_plugin_point(x + width,
                                                        y + depth,
                                                        z),
                                      make_plugin_point(x, y + depth, z),
                                      true)
        .Wire();
  }

  const double s = r / std::sqrt(2.0);
  const gp_Pnt bottom_left = make_plugin_point(x + r, y, z);
  const gp_Pnt bottom_right = make_plugin_point(x + width - r, y, z);
  const gp_Pnt right_bottom = make_plugin_point(x + width, y + r, z);
  const gp_Pnt right_top = make_plugin_point(x + width, y + depth - r, z);
  const gp_Pnt top_right = make_plugin_point(x + width - r, y + depth, z);
  const gp_Pnt top_left = make_plugin_point(x + r, y + depth, z);
  const gp_Pnt left_top = make_plugin_point(x, y + depth - r, z);
  const gp_Pnt left_bottom = make_plugin_point(x, y + r, z);

  BRepBuilderAPI_MakeWire wire;
  wire.Add(BRepBuilderAPI_MakeEdge(bottom_left, bottom_right).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(
               GC_MakeArcOfCircle(bottom_right,
                                  make_plugin_point(x + width - r + s,
                                                    y + r - s,
                                                    z),
                                  right_bottom)
                   .Value())
               .Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(right_bottom, right_top).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(
               GC_MakeArcOfCircle(right_top,
                                  make_plugin_point(x + width - r + s,
                                                    y + depth - r + s,
                                                    z),
                                  top_right)
                   .Value())
               .Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(top_right, top_left).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(
               GC_MakeArcOfCircle(top_left,
                                  make_plugin_point(x + r - s,
                                                    y + depth - r + s,
                                                    z),
                                  left_top)
                   .Value())
               .Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(left_top, left_bottom).Edge());
  wire.Add(BRepBuilderAPI_MakeEdge(
               GC_MakeArcOfCircle(left_bottom,
                                  make_plugin_point(x + r - s, y + r - s, z),
                                  bottom_left)
                   .Value())
               .Edge());
  return wire.Wire();
}

TopoDS_Shape cut_plugin_shape(TopoDS_Shape base, const TopoDS_Shape& cutter) {
  if (base.IsNull() || cutter.IsNull()) {
    return base;
  }
  TopoDS_Shape next = BRepAlgoAPI_Cut(base, cutter).Shape();
  return next.IsNull() ? base : next;
}

TopoDS_Shape fuse_plugin_shape(TopoDS_Shape base, const TopoDS_Shape& addition) {
  if (base.IsNull()) {
    return addition;
  }
  if (addition.IsNull()) {
    return base;
  }
  TopoDS_Shape next = BRepAlgoAPI_Fuse(base, addition).Shape();
  return next.IsNull() ? base : next;
}

TopoDS_Shape make_plugin_rounded_box_at(double x,
                                        double y,
                                        double z,
                                        double width,
                                        double depth,
                                        double height,
                                        double radius) {
  const double r = std::max(
      0.0,
      std::min(radius, std::min(width, depth) / 2.0));
  if (r <= 0.0) {
    return make_plugin_box_at(x, y, z, width, depth, height);
  }

  TopoDS_Shape shape =
      make_plugin_box_at(x + r, y, z, width - 2.0 * r, depth, height);
  shape = fuse_plugin_shape(shape,
                            make_plugin_box_at(x,
                                               y + r,
                                               z,
                                               width,
                                               depth - 2.0 * r,
                                               height));
  shape = fuse_plugin_shape(
      shape,
      make_plugin_cylinder_at(x + r, y + r, z, r, height));
  shape = fuse_plugin_shape(
      shape,
      make_plugin_cylinder_at(x + width - r, y + r, z, r, height));
  shape = fuse_plugin_shape(
      shape,
      make_plugin_cylinder_at(x + r, y + depth - r, z, r, height));
  shape = fuse_plugin_shape(
      shape,
      make_plugin_cylinder_at(x + width - r, y + depth - r, z, r, height));
  return shape;
}

TopoDS_Shape make_plugin_tapered_rounded_box_at(double x,
                                                double y,
                                                double z,
                                                double width,
                                                double depth,
                                                double height,
                                                double radius,
                                                double top_width,
                                                double top_depth,
                                                double top_radius) {
  const double resolved_top_width = top_width > 0.0 ? top_width : width;
  const double resolved_top_depth = top_depth > 0.0 ? top_depth : depth;
  const double resolved_top_radius =
      top_radius > 0.0 ? top_radius : radius;
  const double top_x = x + (width - resolved_top_width) / 2.0;
  const double top_y = y + (depth - resolved_top_depth) / 2.0;

  BRepOffsetAPI_ThruSections loft(true, false, 1.0e-6);
  loft.AddWire(make_plugin_rounded_rectangle_wire_at(
      x, y, z, width, depth, radius));
  loft.AddWire(make_plugin_rounded_rectangle_wire_at(top_x,
                                                     top_y,
                                                     z + height,
                                                     resolved_top_width,
                                                     resolved_top_depth,
                                                     resolved_top_radius));
  loft.CheckCompatibility(false);
  loft.Build();
  return loft.Shape();
}

TopoDS_Shape build_plugin_operation_shape(
    const PluginGeometryOperation& operation) {
  if (operation.primitive == "box") {
    return make_plugin_box_at(operation.x,
                              operation.y,
                              operation.z,
                              operation.width,
                              operation.depth,
                              operation.height);
  }
  if (operation.primitive == "rounded_box") {
    return make_plugin_rounded_box_at(operation.x,
                                      operation.y,
                                      operation.z,
                                      operation.width,
                                      operation.depth,
                                      operation.height,
                                      operation.radius);
  }
  if (operation.primitive == "tapered_rounded_box") {
    return make_plugin_tapered_rounded_box_at(operation.x,
                                             operation.y,
                                             operation.z,
                                             operation.width,
                                             operation.depth,
                                             operation.height,
                                             operation.radius,
                                             operation.top_width,
                                             operation.top_depth,
                                             operation.top_radius);
  }
  if (operation.primitive == "cylinder") {
    return make_plugin_cylinder_at(operation.x,
                                   operation.y,
                                   operation.z,
                                   operation.radius,
                                   operation.height);
  }
  return TopoDS_Shape{};
}

TopoDS_Shape build_plugin_feature_shape(
    const PluginFeatureParameters& parameters) {
  validate_plugin_feature_parameters(parameters);

  TopoDS_Shape shape;
  for (const auto& operation : parameters.geometry) {
    const TopoDS_Shape operand = build_plugin_operation_shape(operation);
    if (operation.operation == "subtract") {
      shape = cut_plugin_shape(shape, operand);
    } else {
      shape = fuse_plugin_shape(shape, operand);
    }
  }
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build plugin feature shape");
  }
  return shape;
}
