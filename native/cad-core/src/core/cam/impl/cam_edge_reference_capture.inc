// ── Edge capture ──────────────────────────────────────────────────
//
// Witness data for re-identifying an edge after topology changes.
// Lines capture endpoints + length + tangent.  Full circles also
// capture the circle witness (center/axis/radius) — a closed rim has
// start == end, so the circle witness is what makes it identifiable.
// Partial arcs are rejected: a drilling rim must be a closed circle.
// The circle math mirrors compute_edge_geometry (edge_geometry.cpp),
// including its full-circle-vs-arc endpoint-distance test.

std::optional<CamEdgeReference> capture_edge_reference(
    const std::string& bodyId,
    const TopoDS_Shape& bodyShape,
    int edgeIndex,
    const std::string& label) {
  if (bodyShape.IsNull() || edgeIndex < 0) {
    return std::nullopt;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
  TopExp::MapShapes(bodyShape, TopAbs_EDGE, edgeMap);
  const int oneBased = edgeIndex + 1;
  if (oneBased < 1 || oneBased > edgeMap.Extent()) {
    return std::nullopt;
  }

  const TopoDS_Edge edge = TopoDS::Edge(edgeMap(oneBased));
  if (edge.IsNull()) {
    return std::nullopt;
  }

  // First / last vertex give the endpoints (and the arc boundary for a
  // partial-circle edge).
  TopoDS_Vertex firstVertex;
  TopoDS_Vertex lastVertex;
  TopExp::Vertices(edge, firstVertex, lastVertex, /*CumOri=*/true);
  if (firstVertex.IsNull() || lastVertex.IsNull()) {
    return std::nullopt;
  }
  const gp_Pnt start = BRep_Tool::Pnt(firstVertex);
  const gp_Pnt endPoint = BRep_Tool::Pnt(lastVertex);

  CamEdgeReference ref;
  ref.bodyId = bodyId;
  ref.label = label;
  ref.startPoint = {start.X(), start.Y(), start.Z()};
  ref.endPoint = {endPoint.X(), endPoint.Y(), endPoint.Z()};

  const double dx = endPoint.X() - start.X();
  const double dy = endPoint.Y() - start.Y();
  const double dz = endPoint.Z() - start.Z();
  ref.length = std::sqrt(dx * dx + dy * dy + dz * dz);

  try {
    BRepAdaptor_Curve curve(edge);
    const GeomAbs_CurveType curveType = curve.GetType();

    if (curveType == GeomAbs_Line) {
      // Unit tangent along the line direction (start to end).
      if (ref.length > 1e-9) {
        ref.tangent = {dx / ref.length, dy / ref.length, dz / ref.length};
      }
      return ref;
    }

    if (curveType == GeomAbs_Circle) {
      // A closed circular edge collapses both endpoints to the same
      // point — that is how OCCT marks "this curve is a full circle".
      // Partial arcs keep a measurable endpoint distance and are
      // rejected (they are not stable drilling inputs).
      const double radius = curve.Circle().Radius();
      const double tolerance = std::max(1e-7, radius * 1e-7);
      if (ref.length > tolerance) {
        return std::nullopt;
      }
      const gp_Pnt center = curve.Circle().Location();
      const gp_Dir axis = curve.Circle().Axis().Direction();
      ref.center = std::array<double, 3>{center.X(), center.Y(), center.Z()};
      ref.axis = std::array<double, 3>{axis.X(), axis.Y(), axis.Z()};
      ref.radius = radius;
      return ref;
    }
  } catch (const std::exception&) {
    // Fall through to rejection.
  }

  // Unsupported curve type (ellipse, bspline, ...).
  return std::nullopt;
}
