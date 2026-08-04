TopoDS_Shape make_fastener_prism_cutter(const std::vector<gp_Pnt>& top_loop,
                                        double depth) {
  if (top_loop.size() < 3 || depth <= 0.0) {
    return TopoDS_Shape{};
  }
  BRepBuilderAPI_MakePolygon polygon;
  for (const auto& point : top_loop) {
    polygon.Add(point);
  }
  polygon.Close();
  if (!polygon.IsDone()) {
    return TopoDS_Shape{};
  }
  BRepBuilderAPI_MakeFace face(polygon.Wire());
  if (!face.IsDone()) {
    return TopoDS_Shape{};
  }
  return BRepPrimAPI_MakePrism(face.Face(), gp_Vec(0.0, 0.0, -depth)).Shape();
}

TopoDS_Shape make_hex_socket_cutter(double center_x,
                                    double center_y,
                                    double top_z,
                                    double radius,
                                    double depth) {
  std::vector<gp_Pnt> points;
  points.reserve(6);
  for (int index = 0; index < 6; ++index) {
    const double angle = kPi / 6.0 + (2.0 * kPi * index) / 6.0;
    points.emplace_back(center_x + std::cos(angle) * radius,
                        center_y + std::sin(angle) * radius,
                        top_z);
  }
  return make_fastener_prism_cutter(points, depth);
}

TopoDS_Shape make_drive_slot_cutter(double center_x,
                                    double center_y,
                                    double top_z,
                                    double length,
                                    double width,
                                    double depth,
                                    double angle) {
  const double half_length = length * 0.5;
  const double half_width = width * 0.5;
  const double c = std::cos(angle);
  const double s = std::sin(angle);
  const auto point = [&](double u, double v) {
    return gp_Pnt(center_x + c * u - s * v,
                  center_y + s * u + c * v,
                  top_z);
  };
  return make_fastener_prism_cutter({
                                        point(-half_length, -half_width),
                                        point(half_length, -half_width),
                                        point(half_length, half_width),
                                        point(-half_length, half_width),
                                    },
                                    depth);
}

TopoDS_Shape apply_fastener_drive_cut(const TopoDS_Shape& shape,
                                      const FastenerFeatureParameters& parameters,
                                      double shaft_center_x,
                                      double shaft_center_y,
                                      double head_radius,
                                      double head_height) {
  if (shape.IsNull() || parameters.drive_type == "none") {
    return shape;
  }
  const double top_z = parameters.length + head_height + 0.05;
  const double depth = std::max(head_height * 0.42, parameters.diameter * 0.18);
  TopoDS_Shape cutter;
  if (parameters.drive_type == "hex_socket") {
    cutter = make_hex_socket_cutter(shaft_center_x,
                                    shaft_center_y,
                                    top_z,
                                    parameters.diameter * 0.24,
                                    depth + 0.1);
  } else if (parameters.drive_type == "phillips") {
    cutter = fuse_shapes(
        make_drive_slot_cutter(shaft_center_x,
                               shaft_center_y,
                               top_z,
                               head_radius * 1.35,
                               parameters.diameter * 0.16,
                               depth + 0.1,
                               0.0),
        make_drive_slot_cutter(shaft_center_x,
                               shaft_center_y,
                               top_z,
                               head_radius * 1.35,
                               parameters.diameter * 0.16,
                               depth + 0.1,
                               kPi / 2.0));
  }
  if (cutter.IsNull()) {
    return shape;
  }
  try {
    BRepAlgoAPI_Cut cut_op(shape, cutter);
    cut_op.SimplifyResult();
    const TopoDS_Shape cut_shape = cut_op.Shape();
    return cut_shape.IsNull() ? shape : cut_shape;
  } catch (const Standard_Failure&) {
    return shape;
  } catch (const std::exception&) {
    return shape;
  }
}
