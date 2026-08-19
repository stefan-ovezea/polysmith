// Compute a world-space point from a plane frame's local (u, v) coordinates
// plus an offset along the plane's normal.
FaceOutlinePoint plane_to_world(const PlaneFrame& frame,
                                double u,
                                double v,
                                double w) {
  return FaceOutlinePoint{
      .x = frame.origin_x + frame.x_axis_x * u + frame.y_axis_x * v +
           frame.normal_x * w,
      .y = frame.origin_y + frame.x_axis_y * u + frame.y_axis_y * v +
           frame.normal_y * w,
      .z = frame.origin_z + frame.x_axis_z * u + frame.y_axis_z * v +
           frame.normal_z * w,
  };
}

FaceOutlinePoint to_outline_point(const gp_Pnt& point) {
  return FaceOutlinePoint{.x = point.X(), .y = point.Y(), .z = point.Z()};
}

std::vector<TopoDS_Edge> ordered_wire_edges(const TopoDS_Wire& wire) {
  std::vector<TopoDS_Edge> ordered_edges;
  for (BRepTools_WireExplorer explorer(wire); explorer.More();
       explorer.Next()) {
    ordered_edges.push_back(TopoDS::Edge(explorer.Current()));
  }
  return ordered_edges;
}

std::optional<FaceOutlineCircle> circle_from_edge(const TopoDS_Edge& edge) {
  try {
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Circle) {
      return std::nullopt;
    }
    const gp_Circ circle = curve.Circle();
    const gp_Pnt center = circle.Location();
    const gp_Dir axis = circle.Axis().Direction();
    return FaceOutlineCircle{
        .center = FaceOutlinePoint{
            .x = center.X(), .y = center.Y(), .z = center.Z()},
        .axis = FaceOutlinePoint{.x = axis.X(), .y = axis.Y(), .z = axis.Z()},
        .radius = circle.Radius(),
    };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<FaceOutlineCircle> circle_from_wire(const TopoDS_Wire& wire) {
  const auto ordered_edges = ordered_wire_edges(wire);
  if (ordered_edges.size() != 1) {
    return std::nullopt;
  }
  return circle_from_edge(ordered_edges.front());
}

std::vector<FaceOutlinePoint> sample_circle_edge_loop(const TopoDS_Edge& edge) {
  std::vector<FaceOutlinePoint> points;
  try {
    const auto circle_outline = circle_from_edge(edge);
    if (!circle_outline.has_value()) {
      return points;
    }
    BRepAdaptor_Curve curve(edge);
    constexpr int kSegments = 64;
    points.reserve(kSegments);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    for (int index = 0; index < kSegments; ++index) {
      const double t =
          first + (last - first) * static_cast<double>(index) /
                      static_cast<double>(kSegments);
      points.push_back(to_outline_point(curve.Value(t)));
    }
  } catch (const std::exception&) {
    points.clear();
  }
  return points;
}

std::vector<FaceOutlinePoint> sample_wire_loop(const TopoDS_Wire& wire) {
  const auto ordered_edges = ordered_wire_edges(wire);
  if (ordered_edges.empty()) {
    return {};
  }

  if (ordered_edges.size() == 1) {
    auto circle_points = sample_circle_edge_loop(ordered_edges.front());
    if (!circle_points.empty()) {
      return circle_points;
    }
  }

  std::vector<FaceOutlinePoint> points;
  points.reserve(ordered_edges.size());
  for (const auto& edge : ordered_edges) {
    if (edge.IsNull()) {
      continue;
    }
    TopoDS_Vertex first_vertex;
    TopoDS_Vertex last_vertex;
    TopExp::Vertices(edge, first_vertex, last_vertex,
                     /*CumOri=*/true);
    if (first_vertex.IsNull()) {
      continue;
    }
    points.push_back(to_outline_point(BRep_Tool::Pnt(first_vertex)));
  }
  return points;
}

// True when (a, b, c) are collinear and b lies strictly between a and
// c. Used to collapse collinear runs in wire outlines — converted
// meshes split every original edge into many collinear triangle edges.
bool outline_collinear_mergeable(const FaceOutlinePoint& a,
                                 const FaceOutlinePoint& b,
                                 const FaceOutlinePoint& c) {
  const double abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
  const double bcx = c.x - b.x, bcy = c.y - b.y, bcz = c.z - b.z;
  const double cx = aby * bcz - abz * bcy;
  const double cy = abz * bcx - abx * bcz;
  const double cz = abx * bcy - aby * bcx;
  const double cross_sq = cx * cx + cy * cy + cz * cz;
  // Exact collinearity up to 1e-8 (mm-scale coordinates).
  if (cross_sq > 1e-16) {
    return false;
  }
  return abx * bcx + aby * bcy + abz * bcz > 0.0;
}

// Merge collinear runs in a closed outline loop (the point list is
// open — the projector closes it). Iterates to a fixpoint so runs
// spanning the list seam also collapse.
void merge_collinear_outline(std::vector<FaceOutlinePoint>& corners) {
  bool merged = true;
  while (merged && corners.size() > 3) {
    merged = false;
    const size_t n = corners.size();
    for (size_t i = 0; i < n; ++i) {
      const FaceOutlinePoint& a = corners[(i + n - 1) % n];
      const FaceOutlinePoint& b = corners[i];
      const FaceOutlinePoint& c = corners[(i + 1) % n];
      if (outline_collinear_mergeable(a, b, c)) {
        corners.erase(corners.begin() + static_cast<std::ptrdiff_t>(i));
        merged = true;
        break;
      }
    }
  }
}

std::vector<std::vector<FaceOutlinePoint>> inner_wire_loops(
    const TopoDS_Face& face,
    const TopoDS_Wire& outer_wire) {
  std::vector<std::vector<FaceOutlinePoint>> loops;
  for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More();
       explorer.Next()) {
    const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
    if (wire.IsNull() || wire.IsSame(outer_wire)) {
      continue;
    }
    auto points = sample_wire_loop(wire);
    if (points.size() >= 3) {
      loops.push_back(std::move(points));
    }
  }
  return loops;
}

std::optional<std::vector<FaceOutlineCircle>> inner_wire_circles(
    const TopoDS_Face& face,
    const TopoDS_Wire& outer_wire) {
  std::vector<FaceOutlineCircle> circles;
  for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More();
       explorer.Next()) {
    const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
    if (wire.IsNull() || wire.IsSame(outer_wire)) {
      continue;
    }
    const auto circle = circle_from_wire(wire);
    if (!circle.has_value()) {
      return std::nullopt;
    }
    circles.push_back(circle.value());
  }
  return circles;
}
