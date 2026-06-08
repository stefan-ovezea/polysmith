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
