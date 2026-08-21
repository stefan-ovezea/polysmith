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
  if (ordered_edges.empty()) {
    return std::nullopt;
  }

  // A wire's circle can be split at seam vertices into several circular
  // edges — accept the wire as a full circle when every edge is
  // circular, shares the same center/axis/radius, and the combined
  // parameter span covers 2*pi.
  std::optional<FaceOutlineCircle> reference;
  double total_span = 0.0;
  for (const auto& edge : ordered_edges) {
    const auto circle = circle_from_edge(edge);
    if (!circle.has_value()) {
      return std::nullopt;
    }
    if (!reference.has_value()) {
      reference = circle;
    } else {
      const double center_tolerance = 1e-6;
      if (std::abs(circle->center.x - reference->center.x) > center_tolerance ||
          std::abs(circle->center.y - reference->center.y) > center_tolerance ||
          std::abs(circle->center.z - reference->center.z) > center_tolerance ||
          std::abs(circle->radius - reference->radius) > center_tolerance ||
          std::abs(circle->axis.x - reference->axis.x) > 1e-6 ||
          std::abs(circle->axis.y - reference->axis.y) > 1e-6 ||
          std::abs(circle->axis.z - reference->axis.z) > 1e-6) {
        return std::nullopt;
      }
    }
    try {
      BRepAdaptor_Curve curve(edge);
      total_span += std::abs(curve.LastParameter() - curve.FirstParameter());
    } catch (const std::exception&) {
      return std::nullopt;
    }
  }

  if (std::abs(total_span - 2.0 * M_PI) > 1e-6) {
    return std::nullopt;  // partial circular wire — not a full circle
  }
  return reference;
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

// (Defined below — the arc-aware merge walkers use it.)
bool outline_collinear_mergeable(const FaceOutlinePoint& a,
                                 const FaceOutlinePoint& b,
                                 const FaceOutlinePoint& c);

// Walk a wire's ordered edges and emit one corner per edge plus a
// parallel segment-arc list. Line edges -> straight segments; partial
// circular edges -> arc descriptors (start/end back-filled from the
// adjacent corners after the walk, since wire edges may be reversed
// relative to their underlying curves); any other curve type -> chord
// fallback (single corner, the old behavior). The caller must have
// already ruled out single-circle wires.
void walk_wire_loop(const TopoDS_Wire& wire,
                    std::vector<FaceOutlinePoint>& corners,
                    std::vector<std::optional<FaceOutlineArc>>& segment_arcs) {
  corners.clear();
  segment_arcs.clear();
  const auto ordered_edges = ordered_wire_edges(wire);
  corners.reserve(ordered_edges.size());
  segment_arcs.reserve(ordered_edges.size());
  for (const auto& edge : ordered_edges) {
    if (edge.IsNull()) {
      continue;
    }
    TopoDS_Vertex first_vertex;
    TopoDS_Vertex last_vertex;
    TopExp::Vertices(edge, first_vertex, last_vertex, /*CumOri=*/true);
    if (first_vertex.IsNull()) {
      continue;
    }
    const FaceOutlinePoint corner =
        to_outline_point(BRep_Tool::Pnt(first_vertex));
    corners.push_back(corner);

    std::optional<FaceOutlineArc> arc;
    try {
      BRepAdaptor_Curve curve(edge);
      if (curve.GetType() == GeomAbs_Circle) {
        const auto circle = circle_from_edge(edge);
        if (circle.has_value()) {
          // A full circle inside a multi-edge wire is degenerate —
          // treat as a plain chord.
          const gp_Pnt last_pnt = BRep_Tool::Pnt(last_vertex);
          const double endpoint_distance = std::sqrt(
              (corner.x - last_pnt.X()) * (corner.x - last_pnt.X()) +
              (corner.y - last_pnt.Y()) * (corner.y - last_pnt.Y()) +
              (corner.z - last_pnt.Z()) * (corner.z - last_pnt.Z()));
          const double tolerance =
              std::max(1e-7, circle->radius * 1e-7);
          if (endpoint_distance > tolerance) {
            // The curve midpoint lies ON the geometric arc span —
            // orientation-proof sweep reference.
            const double mid_param =
                0.5 * (curve.FirstParameter() + curve.LastParameter());
            arc = FaceOutlineArc{
                .center = circle->center,
                .axis = circle->axis,
                .radius = circle->radius,
                .mid = to_outline_point(curve.Value(mid_param)),
            };
          }
        }
      }
    } catch (const std::exception&) {
      arc = std::nullopt;
    }
    segment_arcs.push_back(arc);
  }

  // Back-fill arc endpoints from the adjacent corners (closed loop:
  // the arc's end corner always exists).
  const size_t n = corners.size();
  for (size_t i = 0; i < n; ++i) {
    if (segment_arcs[i].has_value()) {
      segment_arcs[i]->start = corners[i];
      segment_arcs[i]->end = corners[(i + 1) % n];
    }
  }
}

// Arc-aware collinear merge: a corner is only mergeable when BOTH of
// its adjacent segments are lines. Merging two lines folds the segment
// list along with the corners so arcs stay aligned with their corners.
void merge_outline_loop(std::vector<FaceOutlinePoint>& corners,
                        std::vector<std::optional<FaceOutlineArc>>& segment_arcs) {
  bool merged = true;
  while (merged && corners.size() > 3) {
    merged = false;
    const size_t n = corners.size();
    for (size_t i = 0; i < n; ++i) {
      const bool previous_is_line = !segment_arcs[(i + n - 1) % n].has_value();
      const bool next_is_line = !segment_arcs[i].has_value();
      if (!previous_is_line || !next_is_line) {
        continue;
      }
      const FaceOutlinePoint& a = corners[(i + n - 1) % n];
      const FaceOutlinePoint& b = corners[i];
      const FaceOutlinePoint& c = corners[(i + 1) % n];
      if (outline_collinear_mergeable(a, b, c)) {
        corners.erase(corners.begin() + static_cast<std::ptrdiff_t>(i));
        segment_arcs.erase(segment_arcs.begin() + static_cast<std::ptrdiff_t>(i));
        merged = true;
        break;
      }
    }
  }
}

// Collect the face's inner wires: full circles become FaceOutlineCircle
// records (exact), everything else walks as a loop with arc identity.
void inner_wire_geometry(
    const TopoDS_Face& face,
    const TopoDS_Wire& outer_wire,
    std::vector<std::vector<FaceOutlinePoint>>& inner_loops,
    std::vector<std::vector<std::optional<FaceOutlineArc>>>& inner_segment_arcs,
    std::vector<FaceOutlineCircle>& inner_circles) {
  inner_loops.clear();
  inner_segment_arcs.clear();
  inner_circles.clear();
  for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More();
       explorer.Next()) {
    const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
    if (wire.IsNull() || wire.IsSame(outer_wire)) {
      continue;
    }
    if (const auto circle = circle_from_wire(wire); circle.has_value()) {
      inner_circles.push_back(circle.value());
      continue;
    }
    std::vector<FaceOutlinePoint> loop_corners;
    std::vector<std::optional<FaceOutlineArc>> loop_arcs;
    walk_wire_loop(wire, loop_corners, loop_arcs);
    if (loop_corners.size() >= 3) {
      inner_loops.push_back(std::move(loop_corners));
      inner_segment_arcs.push_back(std::move(loop_arcs));
    }
  }
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
