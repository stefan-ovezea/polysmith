std::optional<ThreadAxis> resolve_thread_axis(
    const DocumentState& document,
    const std::string& source_id,
    const std::unordered_map<std::string, TopoDS_Shape>& body_shapes) {
  if (const auto parsed = parse_edge_id(source_id); parsed.has_value()) {
    const auto body_it = body_shapes.find(parsed->body_id);
    if (body_it == body_shapes.end() || body_it->second.IsNull()) {
      return std::nullopt;
    }
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
    TopExp::MapShapes(body_it->second, TopAbs_EDGE, edge_map);
    const int one_based = parsed->index + 1;
    if (one_based < 1 || one_based > edge_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(one_based));
    if (edge.IsNull()) {
      return std::nullopt;
    }
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
      return std::nullopt;
    }
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last, /*CumOri=*/true);
    if (first.IsNull() || last.IsNull()) {
      return std::nullopt;
    }
    const gp_Pnt start = BRep_Tool::Pnt(first);
    const gp_Pnt end = BRep_Tool::Pnt(last);
    const gp_Vec span(start, end);
    if (span.SquareMagnitude() <= 1.0e-18) {
      return std::nullopt;
    }
    return ThreadAxis{
        .start = start,
        .direction = gp_Dir(span),
        .length = span.Magnitude(),
    };
  }

  const auto axis = resolve_construction_axis_source(document, source_id);
  if (!axis.has_value()) {
    return std::nullopt;
  }
  const gp_Pnt start(axis->start_x, axis->start_y, axis->start_z);
  const gp_Pnt end(axis->end_x, axis->end_y, axis->end_z);
  const gp_Vec span(start, end);
  if (span.SquareMagnitude() <= 1.0e-18) {
    return std::nullopt;
  }
  return ThreadAxis{
      .start = start,
      .direction = gp_Dir(span),
      .length = span.Magnitude(),
  };
}

std::optional<TopoDS_Wire> make_wire_from_points(
    const std::vector<gp_Pnt>& points) {
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

TopoDS_Shape make_thread_cutter_profile(const gp_Pnt& center,
                                        const gp_Pnt& axis_point,
                                        const gp_Vec& tangent,
                                        const ThreadFeatureParameters& params,
                                        double major_radius,
                                        double minor_radius,
                                        double half_width) {
  gp_Vec radial(axis_point, center);
  if (radial.SquareMagnitude() <= 1.0e-18 ||
      tangent.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  radial.Normalize();
  gp_Vec width = tangent.Crossed(radial);
  if (width.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  width.Normalize();

  const double center_radius = (major_radius + minor_radius) * 0.5;
  const double overshoot = std::max((major_radius - minor_radius) * 0.35, 0.02);
  gp_Pnt base_a;
  gp_Pnt base_b;
  gp_Pnt tip;
  if (params.mode == "internal") {
    const double inner_base = minor_radius - overshoot - center_radius;
    const double outer_tip = major_radius - center_radius;
    base_a = center.Translated(radial.Multiplied(inner_base))
                 .Translated(width.Multiplied(half_width));
    base_b = center.Translated(radial.Multiplied(inner_base))
                 .Translated(width.Reversed().Multiplied(half_width));
    tip = center.Translated(radial.Multiplied(outer_tip));
  } else {
    const double outer_base = major_radius + overshoot - center_radius;
    const double inner_tip = minor_radius - center_radius;
    base_a = center.Translated(radial.Multiplied(outer_base))
                 .Translated(width.Multiplied(half_width));
    base_b = center.Translated(radial.Multiplied(outer_base))
                 .Translated(width.Reversed().Multiplied(half_width));
    tip = center.Translated(radial.Multiplied(inner_tip));
  }

  BRepBuilderAPI_MakePolygon polygon;
  polygon.Add(base_a);
  polygon.Add(base_b);
  polygon.Add(tip);
  polygon.Close();
  if (!polygon.IsDone()) {
    return TopoDS_Shape{};
  }
  BRepBuilderAPI_MakeFace face(polygon.Wire());
  if (!face.IsDone()) {
    return TopoDS_Shape{};
  }
  return face.Face();
}

TopoDS_Shape build_thread_cutter(const ThreadAxis& axis,
                                 const ThreadFeatureParameters& params) {
  if (params.representation != "modeled" || params.pitch <= 0.0 ||
      params.length <= 0.0 || params.major_diameter <= 0.0) {
    return TopoDS_Shape{};
  }
  const double major_radius = params.major_diameter * 0.5;
  double minor_radius = params.minor_diameter > 0.0
                            ? params.minor_diameter * 0.5
                            : major_radius - params.pitch * 0.6;
  minor_radius = std::min(minor_radius, major_radius - 0.02);
  if (minor_radius <= 0.0) {
    return TopoDS_Shape{};
  }

  const double length =
      axis.length > 0.0
          ? std::min(params.length, std::max(0.0, axis.length - params.start_offset))
          : params.length;
  if (length <= 0.0) {
    return TopoDS_Shape{};
  }

  const gp_Vec axis_vec(axis.direction);
  gp_Vec seed(0.0, 1.0, 0.0);
  if (std::abs(axis_vec.Normalized().Dot(seed)) > 0.9) {
    seed = gp_Vec(1.0, 0.0, 0.0);
  }
  gp_Vec u = axis_vec.Crossed(seed);
  if (u.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  u.Normalize();
  gp_Vec v = axis_vec.Crossed(u);
  if (v.SquareMagnitude() <= 1.0e-18) {
    return TopoDS_Shape{};
  }
  v.Normalize();

  const double helix_radius = (major_radius + minor_radius) * 0.5;
  const double direction_sign = params.handedness == "left" ? -1.0 : 1.0;
  const int steps =
      std::max(32, std::min(1800, static_cast<int>(
                                      std::ceil((length / params.pitch) * 48.0))));
  std::vector<gp_Pnt> points;
  points.reserve(static_cast<size_t>(steps + 1));
  for (int index = 0; index <= steps; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(steps);
    const double along = params.start_offset + length * t;
    const double angle = direction_sign * 2.0 * kPi * (along / params.pitch);
    gp_Vec offset = axis_vec.Multiplied(along);
    offset += u.Multiplied(std::cos(angle) * helix_radius);
    offset += v.Multiplied(std::sin(angle) * helix_radius);
    points.push_back(axis.start.Translated(offset));
  }

  const auto wire = make_wire_from_points(points);
  if (!wire.has_value() || points.size() < 2) {
    return TopoDS_Shape{};
  }
  const gp_Vec tangent(points[0], points[1]);
  const gp_Pnt axis_point = axis.start.Translated(
      axis_vec.Multiplied(params.start_offset));
  const double half_width = std::min(params.pitch * 0.30, major_radius * 0.24);
  const TopoDS_Shape profile =
      make_thread_cutter_profile(points.front(),
                                 axis_point,
                                 tangent,
                                 params,
                                 major_radius,
                                 minor_radius,
                                 half_width);
  if (profile.IsNull()) {
    return TopoDS_Shape{};
  }

  try {
    BRepOffsetAPI_MakePipe pipe(wire.value(), profile);
    pipe.Build();
    if (!pipe.IsDone()) {
      return TopoDS_Shape{};
    }
    return pipe.Shape();
  } catch (const Standard_Failure&) {
    return TopoDS_Shape{};
  } catch (const std::exception& e) {
    fprintf(stderr, "[WARN] OCCT pipe build error: %s\n", e.what());
    return TopoDS_Shape{};
  }
}

TopoDS_Shape apply_thread(const TopoDS_Shape& body_shape,
                          const ThreadFeatureParameters& params,
                          const ThreadAxis& axis) {
  const TopoDS_Shape cutter = build_thread_cutter(axis, params);
  if (body_shape.IsNull() || cutter.IsNull()) {
    return body_shape;
  }
  try {
    const TopoDS_Shape result = BRepAlgoAPI_Cut(body_shape, cutter).Shape();
    return result.IsNull() ? body_shape : unify_same_domain(result);
  } catch (const Standard_Failure&) {
    return body_shape;
  } catch (const std::exception& e) {
    fprintf(stderr, "[WARN] OCCT thread cut error: %s\n", e.what());
    return body_shape;
  }
}

// Triangulate `shape` and accumulate world-space vertices/indices/normals
// into `out`. Each face is processed independently so we get per-face
// flat normals which read well as a default "matte solid" look in the
