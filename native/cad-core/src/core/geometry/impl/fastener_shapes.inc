double estimate_fastener_thread_pitch(const FastenerFeatureParameters& parameters) {
  return parameters.pitch > 0.0
             ? parameters.pitch
             : std::max(parameters.diameter * 0.16, 0.2);
}

double modeled_fastener_thread_depth(const FastenerFeatureParameters& parameters) {
  const double major_radius = parameters.diameter * 0.5;
  if (parameters.minor_diameter > 0.0 &&
      parameters.minor_diameter < parameters.diameter) {
    return std::max((parameters.diameter - parameters.minor_diameter) * 0.5,
                    0.02);
  }
  const double pitch = estimate_fastener_thread_pitch(parameters);
  return std::min({std::max(pitch * 0.38, 0.08), major_radius * 0.22});
}

TopoDS_Shape make_fastener_cylinder(double center_x,
                                    double center_z,
                                    double start_y,
                                    double radius,
                                    double height) {
  if (radius <= 0.0 || height <= 0.0) {
    return TopoDS_Shape{};
  }
  return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(center_x, start_y, center_z),
                                         gp_Dir(0.0, 1.0, 0.0)),
                                  radius,
                                  height)
      .Shape();
}

std::vector<gp_Pnt> sample_fastener_thread_path(double center_x,
                                                double center_z,
                                                double radius,
                                                double pitch,
                                                double length) {
  if (radius <= 0.0 || pitch <= 0.0 || length <= 0.0) {
    return {};
  }
  const int steps = std::max(
      24, std::min(1600, static_cast<int>(std::ceil((length / pitch) * 40.0))));
  std::vector<gp_Pnt> points;
  points.reserve(static_cast<size_t>(steps + 1));
  for (int index = 0; index <= steps; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(steps);
    const double y = length * t;
    const double angle = 2.0 * kPi * (y / pitch);
    points.emplace_back(center_x + std::cos(angle) * radius,
                        y,
                        center_z + std::sin(angle) * radius);
  }
  return points;
}

std::optional<TopoDS_Wire> make_polyline_wire(const std::vector<gp_Pnt>& points) {
  if (points.size() < 2) {
    return std::nullopt;
  }
  BRepBuilderAPI_MakeWire wire_builder;
  for (size_t index = 1; index < points.size(); ++index) {
    if (points[index - 1].Distance(points[index]) <= 1.0e-9) {
      continue;
    }
    const TopoDS_Edge edge =
        BRepBuilderAPI_MakeEdge(points[index - 1], points[index]).Edge();
    if (edge.IsNull()) {
      return std::nullopt;
    }
    wire_builder.Add(edge);
  }
  if (!wire_builder.IsDone()) {
    return std::nullopt;
  }
  return wire_builder.Wire();
}

TopoDS_Shape make_thread_groove_profile_face(const gp_Pnt& center,
                                             const gp_Vec& tangent,
                                             double shaft_center_x,
                                             double shaft_center_z,
                                             double outer_offset,
                                             double inner_offset,
                                             double half_width) {
  gp_Vec radial(gp_Pnt(shaft_center_x, center.Y(), shaft_center_z), center);
  if (radial.SquareMagnitude() <= 1.0e-18 ||
      tangent.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  radial.Normalize();
  gp_Vec section_width = tangent.Crossed(radial);
  if (section_width.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  section_width.Normalize();

  const gp_Pnt outside_a =
      center.Translated(radial.Multiplied(outer_offset))
          .Translated(section_width.Multiplied(half_width));
  const gp_Pnt outside_b =
      center.Translated(radial.Multiplied(outer_offset))
          .Translated(section_width.Reversed().Multiplied(half_width));
  const gp_Pnt inside_tip =
      center.Translated(radial.Reversed().Multiplied(inner_offset));

  BRepBuilderAPI_MakePolygon profile_polygon;
  profile_polygon.Add(outside_a);
  profile_polygon.Add(outside_b);
  profile_polygon.Add(inside_tip);
  profile_polygon.Close();
  if (!profile_polygon.IsDone()) {
    return TopoDS_Shape{};
  }
  BRepBuilderAPI_MakeFace profile_face(profile_polygon.Wire());
  if (!profile_face.IsDone()) {
    return TopoDS_Shape{};
  }
  return profile_face.Face();
}

TopoDS_Shape build_external_fastener_thread_groove_cutter(
    const FastenerFeatureParameters& parameters,
    double shaft_center_x,
    double shaft_center_z) {
  const double thread_length =
      std::min(std::max(parameters.thread_length, 0.0), parameters.length);
  if (parameters.thread_representation != "modeled" || thread_length <= 0.0) {
    return TopoDS_Shape{};
  }

  const double major_radius = parameters.diameter * 0.5;
  const double pitch = estimate_fastener_thread_pitch(parameters);
  const double thread_depth = modeled_fastener_thread_depth(parameters);
  const double minor_radius =
      parameters.minor_diameter > 0.0 && parameters.minor_diameter < parameters.diameter
          ? parameters.minor_diameter * 0.5
          : std::max(major_radius - thread_depth, major_radius * 0.72);
  const double helix_radius = (major_radius + minor_radius) * 0.5;
  const double outer_offset = major_radius - helix_radius + thread_depth * 0.35;
  const double inner_offset = helix_radius - minor_radius;
  const double half_width = std::min(pitch * 0.30, major_radius * 0.26);
  const std::vector<gp_Pnt> path_points =
      sample_fastener_thread_path(shaft_center_x,
                                  shaft_center_z,
                                  helix_radius,
                                  pitch,
                                  thread_length);
  const auto spine = make_polyline_wire(path_points);
  if (!spine.has_value() || path_points.size() < 2) {
    return TopoDS_Shape{};
  }

  const gp_Vec tangent(path_points[0], path_points[1]);
  if (tangent.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  const TopoDS_Shape profile_face =
      make_thread_groove_profile_face(path_points.front(),
                                      tangent,
                                      shaft_center_x,
                                      shaft_center_z,
                                      outer_offset,
                                      inner_offset,
                                      half_width);
  if (profile_face.IsNull()) {
    return TopoDS_Shape{};
  }

  try {
    BRepOffsetAPI_MakePipe pipe(spine.value(), profile_face);
    pipe.Build();
    if (!pipe.IsDone()) {
      return TopoDS_Shape{};
    }
    return pipe.Shape();
  } catch (const Standard_Failure&) {
    return TopoDS_Shape{};
  }
}

double triangular_thread_wave(double phase) {
  const double wrapped = phase - std::floor(phase);
  return 1.0 - std::abs(wrapped * 2.0 - 1.0);
}

TopoDS_Face make_polygon_face(const std::vector<gp_Pnt>& points) {
  BRepBuilderAPI_MakePolygon polygon;
  for (const auto& point : points) {
    polygon.Add(point);
  }
  polygon.Close();
  if (!polygon.IsDone()) {
    return TopoDS_Face{};
  }
  BRepBuilderAPI_MakeFace face(polygon.Wire());
  if (!face.IsDone()) {
    return TopoDS_Face{};
  }
  return face.Face();
}

TopoDS_Shape build_faceted_fastener_threaded_shaft(
    const FastenerFeatureParameters& parameters,
    double shaft_center_x,
    double shaft_center_z) {
  const double thread_length =
      std::min(std::max(parameters.thread_length, 0.0), parameters.length);
  if (parameters.thread_representation != "modeled" || thread_length <= 0.0) {
    return TopoDS_Shape{};
  }

  const double major_radius = parameters.diameter * 0.5;
  const double pitch = estimate_fastener_thread_pitch(parameters);
  double minor_radius =
      parameters.minor_diameter > 0.0 && parameters.minor_diameter < parameters.diameter
          ? parameters.minor_diameter * 0.5
          : major_radius - modeled_fastener_thread_depth(parameters);
  minor_radius = std::max(minor_radius, major_radius * 0.68);
  if (pitch <= 0.0 || minor_radius <= 0.0 || minor_radius >= major_radius) {
    return TopoDS_Shape{};
  }

  const int radial_segments = 72;
  const int axial_segments =
      std::max(24, std::min(1600, static_cast<int>(
                                      std::ceil((thread_length / pitch) * 28.0))));
  std::vector<std::vector<gp_Pnt>> rings;
  rings.reserve(static_cast<size_t>(axial_segments + 1));
  for (int y_index = 0; y_index <= axial_segments; ++y_index) {
    const double y =
        thread_length * static_cast<double>(y_index) /
        static_cast<double>(axial_segments);
    std::vector<gp_Pnt> ring;
    ring.reserve(radial_segments);
    for (int theta_index = 0; theta_index < radial_segments; ++theta_index) {
      const double theta =
          2.0 * kPi * static_cast<double>(theta_index) /
          static_cast<double>(radial_segments);
      const double phase =
          (y / pitch) -
          (theta / (2.0 * kPi));
      const double wave = triangular_thread_wave(phase);
      const double radius =
          minor_radius + (major_radius - minor_radius) * wave;
      ring.emplace_back(shaft_center_x + std::cos(theta) * radius,
                        y,
                        shaft_center_z + std::sin(theta) * radius);
    }
    rings.push_back(std::move(ring));
  }

  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);
  for (int y_index = 0; y_index < axial_segments; ++y_index) {
    for (int theta_index = 0; theta_index < radial_segments; ++theta_index) {
      const int next_theta = (theta_index + 1) % radial_segments;
      const TopoDS_Face first = make_polygon_face({
          rings[y_index][theta_index],
          rings[y_index][next_theta],
          rings[y_index + 1][next_theta],
      });
      if (!first.IsNull()) {
        builder.Add(compound, first);
      }
      const TopoDS_Face second = make_polygon_face({
          rings[y_index][theta_index],
          rings[y_index + 1][next_theta],
          rings[y_index + 1][theta_index],
      });
      if (!second.IsNull()) {
        builder.Add(compound, second);
      }
    }
  }

  const gp_Pnt bottom_center(shaft_center_x, 0.0, shaft_center_z);
  const gp_Pnt top_center(shaft_center_x, thread_length, shaft_center_z);
  for (int theta_index = 0; theta_index < radial_segments; ++theta_index) {
    const int next_theta = (theta_index + 1) % radial_segments;
    const TopoDS_Face bottom = make_polygon_face({
        bottom_center,
        rings[0][next_theta],
        rings[0][theta_index],
    });
    if (!bottom.IsNull()) {
      builder.Add(compound, bottom);
    }
    const TopoDS_Face top = make_polygon_face({
        top_center,
        rings[axial_segments][theta_index],
        rings[axial_segments][next_theta],
    });
    if (!top.IsNull()) {
      builder.Add(compound, top);
    }
  }

  return compound;
}

TopoDS_Shape make_shape_compound(const std::vector<TopoDS_Shape>& shapes) {
  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);
  bool added = false;
  for (const auto& shape : shapes) {
    if (!shape.IsNull()) {
      builder.Add(compound, shape);
      added = true;
    }
  }
  return added ? TopoDS_Shape(compound) : TopoDS_Shape{};
}

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
  return BRepPrimAPI_MakePrism(face.Face(), gp_Vec(0.0, -depth, 0.0)).Shape();
}

TopoDS_Shape make_hex_socket_cutter(double center_x,
                                    double center_z,
                                    double top_y,
                                    double radius,
                                    double depth) {
  std::vector<gp_Pnt> points;
  points.reserve(6);
  for (int index = 0; index < 6; ++index) {
    const double angle = kPi / 6.0 + (2.0 * kPi * index) / 6.0;
    points.emplace_back(center_x + std::cos(angle) * radius,
                        top_y,
                        center_z + std::sin(angle) * radius);
  }
  return make_fastener_prism_cutter(points, depth);
}

TopoDS_Shape make_drive_slot_cutter(double center_x,
                                    double center_z,
                                    double top_y,
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
                  top_y,
                  center_z + s * u + c * v);
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
                                      double shaft_center_z,
                                      double head_radius,
                                      double head_height) {
  if (shape.IsNull() || parameters.drive_type == "none") {
    return shape;
  }
  const double top_y = parameters.length + head_height + 0.05;
  const double depth = std::max(head_height * 0.42, parameters.diameter * 0.18);
  TopoDS_Shape cutter;
  if (parameters.drive_type == "hex_socket") {
    cutter = make_hex_socket_cutter(shaft_center_x,
                                    shaft_center_z,
                                    top_y,
                                    parameters.diameter * 0.24,
                                    depth + 0.1);
  } else if (parameters.drive_type == "phillips") {
    cutter = fuse_shapes(
        make_drive_slot_cutter(shaft_center_x,
                               shaft_center_z,
                               top_y,
                               head_radius * 1.35,
                               parameters.diameter * 0.16,
                               depth + 0.1,
                               0.0),
        make_drive_slot_cutter(shaft_center_x,
                               shaft_center_z,
                               top_y,
                               head_radius * 1.35,
                               parameters.diameter * 0.16,
                               depth + 0.1,
                               kPi / 2.0));
  }
  if (cutter.IsNull()) {
    return shape;
  }
  try {
    const TopoDS_Shape cut_shape = BRepAlgoAPI_Cut(shape, cutter).Shape();
    return cut_shape.IsNull() ? shape : cut_shape;
  } catch (const Standard_Failure&) {
    return shape;
  }
}

TopoDS_Shape build_fastener_shape(const FastenerFeatureParameters& parameters) {
  if (parameters.diameter <= 0.0 || parameters.length <= 0.0) {
    throw std::runtime_error("Fastener diameter and length must be greater than zero");
  }
  const double shaft_radius = parameters.diameter * 0.5;
  const bool modeled_threads =
      parameters.thread_representation == "modeled" &&
      std::min(std::max(parameters.thread_length, 0.0), parameters.length) > 0.0;
  const double thread_length =
      std::min(std::max(parameters.thread_length, 0.0), parameters.length);
  TopoDS_Shape shape = make_fastener_cylinder(shaft_radius,
                                              shaft_radius,
                                              0.0,
                                              shaft_radius,
                                              parameters.length);
  TopoDS_Shape threaded_shape;
  TopoDS_Shape unthreaded_shank;
  if (modeled_threads) {
    threaded_shape =
        build_faceted_fastener_threaded_shaft(parameters, shaft_radius, shaft_radius);
    if (thread_length < parameters.length) {
      unthreaded_shank = make_fastener_cylinder(shaft_radius,
                                                shaft_radius,
                                                thread_length,
                                                shaft_radius,
                                                parameters.length - thread_length);
    }
    if (!threaded_shape.IsNull()) {
      shape = threaded_shape;
    }
  }
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build fastener shaft");
  }

  const double head_radius =
      parameters.head_type == "hex_bolt" ? parameters.diameter : parameters.diameter * 0.85;
  const double head_height =
      parameters.head_type == "flat" ? parameters.diameter * 0.5 : parameters.diameter * 0.65;
  TopoDS_Shape head;
  if (parameters.head_type == "flat") {
    head = BRepPrimAPI_MakeCone(
               gp_Ax2(gp_Pnt(shaft_radius, parameters.length, shaft_radius),
                      gp_Dir(0.0, 1.0, 0.0)),
               shaft_radius,
               head_radius,
               head_height)
               .Shape();
  } else {
    head = BRepPrimAPI_MakeCylinder(
               gp_Ax2(gp_Pnt(shaft_radius, parameters.length, shaft_radius),
                      gp_Dir(0.0, 1.0, 0.0)),
               head_radius,
               head_height)
               .Shape();
  }
  if (modeled_threads && !threaded_shape.IsNull()) {
    head = apply_fastener_drive_cut(head,
                                    parameters,
                                    shaft_radius,
                                    shaft_radius,
                                    head_radius,
                                    head_height);
    return make_shape_compound({threaded_shape, unthreaded_shank, head});
  }
  shape = fuse_shapes(shape, head);
  return apply_fastener_drive_cut(shape,
                                  parameters,
                                  shaft_radius,
                                  shaft_radius,
                                  head_radius,
                                  head_height);
}
