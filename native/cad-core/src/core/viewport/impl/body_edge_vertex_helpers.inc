// Sample a single OCCT edge into a flat polyline (x0, y0, z0, x1, ...).
// Straight segments stay 2-point; everything else uses
// GCPnts_QuasiUniformDeflection with a small fixed deflection so curves
// look smooth in the viewport without ballooning the wire payload.
void sample_edge(const TopoDS_Edge& edge,
                 std::vector<double>& points,
                 std::string& kind) {
  points.clear();
  kind = "curve";

  if (edge.IsNull()) {
    return;
  }

  // BRepAdaptor_Curve internally pulls the curve from the edge and
  // applies the edge's location, so the sampled points are already in
  // world space relative to the body.
  BRepAdaptor_Curve adaptor;
  try {
    adaptor.Initialize(edge);
  } catch (const std::exception&) {
    return;
  }

  const GeomAbs_CurveType curve_type = adaptor.GetType();
  if (curve_type == GeomAbs_Line) {
    kind = "line";
    const double first = adaptor.FirstParameter();
    const double last = adaptor.LastParameter();
    const gp_Pnt start = adaptor.Value(first);
    const gp_Pnt end = adaptor.Value(last);
    points.push_back(start.X());
    points.push_back(start.Y());
    points.push_back(start.Z());
    points.push_back(end.X());
    points.push_back(end.Y());
    points.push_back(end.Z());
    return;
  }

  if (curve_type == GeomAbs_Circle) {
    kind = "circle";
  }

  // Generic curve sampler. The deflection of 0.05mm matches the
  // body_compiler tessellation budget closely enough that selectable
  // edges hug the rendered solid surface.
  try {
    GCPnts_QuasiUniformDeflection sampler(adaptor, /*Deflection=*/0.05);
    if (!sampler.IsDone() || sampler.NbPoints() < 2) {
      // Fall back to the curve's parametric endpoints so the edge is
      // at least pickable as a chord.
      const double first = adaptor.FirstParameter();
      const double last = adaptor.LastParameter();
      const gp_Pnt start = adaptor.Value(first);
      const gp_Pnt end = adaptor.Value(last);
      points.push_back(start.X());
      points.push_back(start.Y());
      points.push_back(start.Z());
      points.push_back(end.X());
      points.push_back(end.Y());
      points.push_back(end.Z());
      return;
    }
    for (int i = 1; i <= sampler.NbPoints(); ++i) {
      const gp_Pnt p = sampler.Value(i);
      points.push_back(p.X());
      points.push_back(p.Y());
      points.push_back(p.Z());
    }
  } catch (const std::exception&) {
    points.clear();
  }
}

bool edge_curve_type_is(const TopoDS_Edge& edge, GeomAbs_CurveType type) {
  try {
    BRepAdaptor_Curve curve(edge);
    return curve.GetType() == type;
  } catch (const std::exception&) {
    return false;
  }
}

bool is_full_circle_edge(const TopoDS_Edge& edge) {
  try {
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Circle) {
      return false;
    }
    const double span = std::abs(curve.LastParameter() - curve.FirstParameter());
    if (span >= 2.0 * kPi - 1e-4) {
      return true;
    }
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last, /*CumOri=*/false);
    if (first.IsNull() || last.IsNull()) {
      return false;
    }
    return BRep_Tool::Pnt(first).Distance(BRep_Tool::Pnt(last)) <= 1e-5;
  } catch (const std::exception&) {
    return false;
  }
}

// FacetEdgeFilter (core/viewport/facet_edge_filter.h) drops
// triangulation seams between coplanar faces — see the header for the
// rationale (payload flood on faceted converted bodies).

// Precomputed seam topology for one body shape, replacing the
// historical per-shape probes:
//  - the seam-line probe scanned EVERY face for EVERY edge (O(E×F)),
//    and
//  - the circle-seam probe rebuilt the body's edge map for EVERY
//    vertex and then scanned all edges (O(V×E)).
// A STEP-imported PCB assembly (~78k edges, ~156k vertices, ~15k
// faces) spent ~30 minutes in those two probes before the viewport
// event went out. The incidence-based build below walks the topology
// once and produces the same sets.
struct BodySeamAnalysis {
  // Edges that are straight lines AND close a face (the lateral seam
  // of a cylinder, the tube seam of a revolved surface).
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>
      seam_line_edges;
  // Vertices whose EVERY incident edge is a seam line or a full
  // circle (a cylinder's seam endpoints and rim vertices) — excluded
  // from the pickable vertex set.
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>
      seam_circle_vertices;
};

BodySeamAnalysis build_seam_analysis(const TopoDS_Shape& body_shape) {
  BodySeamAnalysis analysis;
  if (body_shape.IsNull()) {
    return analysis;
  }

  // Pass 1 — seam line edges. Iterate each face's own edges once and
  // keep the straight-line edges that close the face. Equivalent to
  // the old per-edge scan of all faces (IsClosed counts the edge's
  // occurrences in the face's wires, orientation-insensitively), but
  // O(face-edge incidences) instead of O(E×F).
  for (TopExp_Explorer face_exp(body_shape, TopAbs_FACE); face_exp.More();
       face_exp.Next()) {
    const TopoDS_Face face = TopoDS::Face(face_exp.Current());
    if (face.IsNull()) {
      continue;
    }
    for (TopExp_Explorer edge_exp(face, TopAbs_EDGE); edge_exp.More();
         edge_exp.Next()) {
      const TopoDS_Edge edge = TopoDS::Edge(edge_exp.Current());
      if (edge.IsNull()) {
        continue;
      }
      bool closed = false;
      try {
        closed = BRep_Tool::IsClosed(edge, face);
      } catch (const std::exception&) {
        continue;
      }
      if (!closed || !edge_curve_type_is(edge, GeomAbs_Line)) {
        continue;
      }
      TopoDS_Edge normalized = edge;
      normalized.Orientation(TopAbs_FORWARD);
      if (!analysis.seam_line_edges.Contains(normalized)) {
        analysis.seam_line_edges.Add(normalized);
      }
    }
  }

  // Pass 2 — circle-seam vertices. Walk each edge's two endpoints
  // once, recording per-vertex whether every incident edge is a seam
  // line or a full circle. A vertex with no incident edge at all is
  // not a seam vertex (same as the old probe's found_incident_edge).
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> vertex_map;
  std::vector<bool> all_incident_seam_like;
  std::vector<bool> has_incident_edge;

  const auto edge_is_seam_like = [&](const TopoDS_Edge& edge) {
    TopoDS_Edge normalized = edge;
    normalized.Orientation(TopAbs_FORWARD);
    return analysis.seam_line_edges.Contains(normalized) ||
           is_full_circle_edge(edge);
  };
  const auto mark_vertex = [&](const TopoDS_Vertex& vertex,
                               bool incident_seam_like) {
    if (vertex.IsNull()) {
      return;
    }
    TopoDS_Vertex normalized = vertex;
    normalized.Orientation(TopAbs_FORWARD);
    int index = -1;
    if (vertex_map.Contains(normalized)) {
      index = vertex_map.FindIndex(normalized);
    } else {
      index = vertex_map.Add(normalized);
      all_incident_seam_like.push_back(incident_seam_like);
      has_incident_edge.push_back(true);
    }
    if (!incident_seam_like) {
      all_incident_seam_like[static_cast<size_t>(index - 1)] = false;
    }
  };
  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (edge.IsNull()) {
      continue;
    }
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last, /*CumOri=*/false);
    const bool seam_like = edge_is_seam_like(edge);
    mark_vertex(first, seam_like);
    mark_vertex(last, seam_like);
  }
  for (int i = 1; i <= vertex_map.Extent(); ++i) {
    if (has_incident_edge[static_cast<size_t>(i - 1)] &&
        all_incident_seam_like[static_cast<size_t>(i - 1)]) {
      analysis.seam_circle_vertices.Add(vertex_map(i));
    }
  }
  return analysis;
}

// Append every unique edge of `body_shape` to `out`, owned by `body_id`.
// Edge ids match the format expected by DocumentManager::select_edge.
// `selected_edge_ids` carries the multi-edge selection set; an edge is
// flagged selected iff its id appears anywhere in the set. We accept a
// vector rather than a hash set because edge selections are O(N <= ~20)
// in practice — the linear scan is cheaper than building a hash set
// per body.
void enumerate_body_edges(const TopoDS_Shape& body_shape,
                          const std::string& body_id,
                          const std::vector<std::string>& selected_edge_ids,
                          const FacetEdgeFilter* facet_filter,
                          std::vector<ViewportEdgePrimitive>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  // Seam topology computed once per body (see build_seam_analysis) —
  // the seam-line probe used to iterate every face per edge (O(E×F)).
  // Faceted STL bodies have no closing edges of closed CAD surfaces,
  // so the analysis is skipped for them.
  const BodySeamAnalysis seam_analysis =
      facet_filter == nullptr ? build_seam_analysis(body_shape)
                              : BodySeamAnalysis{};

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (facet_filter == nullptr) {
      TopoDS_Edge normalized = edge;
      normalized.Orientation(TopAbs_FORWARD);
      if (seam_analysis.seam_line_edges.Contains(normalized)) {
        continue;
      }
    }
    if (facet_filter != nullptr && !facet_filter->keep(edge)) {
      continue;
    }
    ViewportEdgePrimitive primitive{};
    primitive.id = body_id + ":edge:" + std::to_string(i - 1);
    primitive.owner_body_id = body_id;
    sample_edge(edge, primitive.points, primitive.kind);
    // Circle witness for FULL circles only (drilling hole rims).  A
    // closed rim collapses both endpoints to one point, so the sample
    // polyline alone cannot identify it; arcs and lines stay unset —
    // a partial arc has no unique circle witness.
    if (is_full_circle_edge(edge)) {
      try {
        BRepAdaptor_Curve curve(edge);
        const gp_Circ circle = curve.Circle();
        const gp_Pnt center = circle.Location();
        const gp_Dir axis = circle.Axis().Direction();
        primitive.center =
            std::array<double, 3>{center.X(), center.Y(), center.Z()};
        primitive.axis = std::array<double, 3>{axis.X(), axis.Y(), axis.Z()};
        primitive.radius = circle.Radius();
      } catch (const std::exception&) {
        // Leave the witness unset — the edge stays pickable as a rim
        // line, it just gets no hole-dot candidate.
      }
    }
    if (primitive.points.size() < 6) {
      // Degenerate edges (fewer than 2 sample points) cannot be picked
      // or rendered; skip them rather than emit dead entries.
      continue;
    }
    if (facet_filter != nullptr) {
      // Facet bodies: polyline length from the sampled points (exact
      // for straight facet edges, within sampling deflection for
      // curves). BRepGProp integration per edge is the slow path —
      // measured seconds across a few thousand facet edges.
      for (size_t k = 0; k + 3 < primitive.points.size(); k += 3) {
        const double dx = primitive.points[k + 3] - primitive.points[k];
        const double dy = primitive.points[k + 4] - primitive.points[k + 1];
        const double dz = primitive.points[k + 5] - primitive.points[k + 2];
        primitive.length += std::sqrt(dx * dx + dy * dy + dz * dz);
      }
    } else {
      // Exact length via OCCT mass-property integration. We swallow
      // failures here (defaulting to 0) because a rare degenerate edge
      // shouldn't take the whole snapshot down — the UI just shows
      // 0 mm for that edge.
      try {
        GProp_GProps props;
        BRepGProp::LinearProperties(edge, props);
        primitive.length = props.Mass();
      } catch (const std::exception&) {
        primitive.length = 0.0;
      }
    }
    primitive.is_selected =
        std::find(selected_edge_ids.begin(),
                  selected_edge_ids.end(),
                  primitive.id) != selected_edge_ids.end();
    out.push_back(std::move(primitive));
  }
}

// Append every unique vertex of `body_shape` to `out`, owned by `body_id`.
// Vertex ids match the format expected by DocumentManager::select_vertex.
void enumerate_body_vertices(
    const TopoDS_Shape& body_shape,
    const std::string& body_id,
    const std::vector<std::string>& selected_vertex_ids,
    const FacetEdgeFilter* facet_filter,
    std::vector<ViewportVertexPrimitive>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  // Facet filter for mesh-converted bodies: a vertex is emitted only
  // when it ends at least one SEMANTIC edge (see FacetEdgeFilter) —
  // outline corners stay, triangulation nodes on flat regions go.
  // The filter instance is shared with the edge enumeration — its
  // face-normal cache makes this pass cheap.
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>
      kept_vertex_map;
  if (facet_filter != nullptr) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
    TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);
    for (int i = 1; i <= edge_map.Extent(); ++i) {
      const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
      if (edge.IsNull() || !facet_filter->keep(edge)) {
        continue;
      }
      TopoDS_Vertex first;
      TopoDS_Vertex last;
      TopExp::Vertices(edge, first, last, /*CumOri=*/false);
      if (!first.IsNull() && !kept_vertex_map.Contains(first)) {
        kept_vertex_map.Add(first);
      }
      if (!last.IsNull() && !kept_vertex_map.Contains(last)) {
        kept_vertex_map.Add(last);
      }
    }
  }

  // Circle-seam vertices precomputed once per body (see
  // build_seam_analysis) — the old probe rebuilt the whole edge map
  // PER VERTEX (O(V×E), seconds on facet clouds) and only finds seam
  // vertices of closed CAD surfaces, so faceted STL bodies skip it.
  const BodySeamAnalysis seam_analysis =
      facet_filter == nullptr ? build_seam_analysis(body_shape)
                              : BodySeamAnalysis{};

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> vertex_map;
  TopExp::MapShapes(body_shape, TopAbs_VERTEX, vertex_map);

  for (int i = 1; i <= vertex_map.Extent(); ++i) {
    const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_map(i));
    if (vertex.IsNull()) {
      continue;
    }
    if (facet_filter != nullptr) {
      if (!kept_vertex_map.Contains(vertex)) {
        continue;
      }
    } else {
      TopoDS_Vertex normalized = vertex;
      normalized.Orientation(TopAbs_FORWARD);
      if (seam_analysis.seam_circle_vertices.Contains(normalized)) {
        continue;
      }
    }
    gp_Pnt position;
    try {
      position = BRep_Tool::Pnt(vertex);
    } catch (const std::exception&) {
      continue;
    }
    ViewportVertexPrimitive primitive{};
    primitive.id = body_id + ":vertex:" + std::to_string(i - 1);
    primitive.owner_body_id = body_id;
    primitive.x = position.X();
    primitive.y = position.Y();
    primitive.z = position.Z();
    primitive.is_selected =
        std::find(selected_vertex_ids.begin(),
                  selected_vertex_ids.end(),
                  primitive.id) != selected_vertex_ids.end();
    out.push_back(std::move(primitive));
  }
}
