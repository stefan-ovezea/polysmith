void include_world_point_for_plane(const std::string& plane_id,
                                   double local_x,
                                   double local_y,
                                   double depth,
                                   double& min_x,
                                   double& max_x,
                                   double& min_y,
                                   double& max_y,
                                   double& min_z,
                                   double& max_z) {
  const auto include_point =
      [&](double world_x, double world_y, double world_z) {
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_y = std::min(min_y, world_y);
        max_y = std::max(max_y, world_y);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      };

  if (plane_id == "ref-plane-xy") {
    include_point(local_x, 0.0, local_y);
    include_point(local_x, depth, local_y);
    return;
  }

  if (plane_id == "ref-plane-yz") {
    include_point(0.0, local_x, local_y);
    include_point(depth, local_x, local_y);
    return;
  }

  include_point(local_x, local_y, 0.0);
  include_point(local_x, local_y, depth);
}

void include_world_point_for_frame(
    const PlaneFrame& frame,
    double local_x,
    double local_y,
    double depth,
    double& min_x,
    double& max_x,
    double& min_y,
    double& max_y,
    double& min_z,
    double& max_z) {
  const auto include_point =
      [&](double world_x, double world_y, double world_z) {
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_y = std::min(min_y, world_y);
        max_y = std::max(max_y, world_y);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      };

  const auto to_world = [&](double offset) {
    return WorldPoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y + frame.normal_x * offset,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y + frame.normal_y * offset,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y + frame.normal_z * offset,
    };
  };

  const WorldPoint start = to_world(0.0);
  const WorldPoint end = to_world(depth);
  include_point(start.x, start.y, start.z);
  include_point(end.x, end.y, end.z);
}

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

bool is_nonsemantic_seam_line_edge(const TopoDS_Shape& body_shape,
                                   const TopoDS_Edge& edge) {
  if (body_shape.IsNull() || edge.IsNull() ||
      !edge_curve_type_is(edge, GeomAbs_Line)) {
    return false;
  }
  for (TopExp_Explorer explorer(body_shape, TopAbs_FACE); explorer.More();
       explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(explorer.Current());
    if (face.IsNull()) {
      continue;
    }
    try {
      if (BRep_Tool::IsClosed(edge, face)) {
        return true;
      }
    } catch (const std::exception&) {
      continue;
    }
  }
  return false;
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

bool edge_contains_vertex(const TopoDS_Edge& edge, const TopoDS_Vertex& vertex) {
  TopoDS_Vertex first;
  TopoDS_Vertex last;
  TopExp::Vertices(edge, first, last, /*CumOri=*/false);
  return (!first.IsNull() && first.IsSame(vertex)) ||
         (!last.IsNull() && last.IsSame(vertex));
}

bool is_nonsemantic_circle_seam_vertex(const TopoDS_Shape& body_shape,
                                       const TopoDS_Vertex& vertex) {
  if (body_shape.IsNull() || vertex.IsNull()) {
    return false;
  }
  TopTools_IndexedMapOfShape edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);
  bool found_incident_edge = false;
  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (edge.IsNull() || !edge_contains_vertex(edge, vertex)) {
      continue;
    }
    found_incident_edge = true;
    if (!is_nonsemantic_seam_line_edge(body_shape, edge) &&
        !is_full_circle_edge(edge)) {
      return false;
    }
  }
  return found_incident_edge;
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
                          std::vector<ViewportEdgePrimitive>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  TopTools_IndexedMapOfShape edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (is_nonsemantic_seam_line_edge(body_shape, edge)) {
      continue;
    }
    ViewportEdgePrimitive primitive{};
    primitive.id = body_id + ":edge:" + std::to_string(i - 1);
    primitive.owner_body_id = body_id;
    sample_edge(edge, primitive.points, primitive.kind);
    if (primitive.points.size() < 6) {
      // Degenerate edges (fewer than 2 sample points) cannot be picked
      // or rendered; skip them rather than emit dead entries.
      continue;
    }
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
    std::vector<ViewportVertexPrimitive>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  TopTools_IndexedMapOfShape vertex_map;
  TopExp::MapShapes(body_shape, TopAbs_VERTEX, vertex_map);

  for (int i = 1; i <= vertex_map.Extent(); ++i) {
    const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_map(i));
    if (vertex.IsNull()) {
      continue;
    }
    if (is_nonsemantic_circle_seam_vertex(body_shape, vertex)) {
      continue;
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

// Build a plane frame for `face` by reading the underlying surface's plane
// (when planar) and the face center. For non-planar faces, return a
// representative frame oriented around the face center + face normal at
// that point — picking still works, sketch-on-face is rejected via the
// returned `is_planar` flag.
struct FaceFrameInfo {
  ViewportSolidFace::PlaneFrame frame;
  double center_x;
  double center_y;
  double center_z;
  double normal_x;
  double normal_y;
  double normal_z;
  bool is_planar;
};

FaceFrameInfo derive_face_frame(const TopoDS_Face& face) {
  FaceFrameInfo info{};
  info.is_planar = false;
  // Sensible defaults; these are overwritten below.
  info.frame = ViewportSolidFace::PlaneFrame{
      .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
      .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
      .y_axis_x = 0.0, .y_axis_y = 1.0, .y_axis_z = 0.0,
      .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
  };

  try {
    BRepAdaptor_Surface surface(face);
    const GeomAbs_SurfaceType type = surface.GetType();

    // Compute the face's parametric mid-point so we can sample a
    // representative (point, normal) regardless of surface type.
    const double u_mid = 0.5 * (surface.FirstUParameter() + surface.LastUParameter());
    const double v_mid = 0.5 * (surface.FirstVParameter() + surface.LastVParameter());

    BRepGProp_Face prop(face);
    gp_Pnt center;
    gp_Vec normal;
    prop.Normal(u_mid, v_mid, center, normal);
    if (normal.Magnitude() > 0.0) {
      normal.Normalize();
    } else {
      normal = gp_Vec(0.0, 0.0, 1.0);
    }
    // Honor the face's orientation flag so the emitted normal is the
    // outward-pointing one consumers expect.
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
    }

    info.center_x = center.X();
    info.center_y = center.Y();
    info.center_z = center.Z();
    info.normal_x = normal.X();
    info.normal_y = normal.Y();
    info.normal_z = normal.Z();

    if (type == GeomAbs_Plane) {
      info.is_planar = true;
      const gp_Pln plane = surface.Plane();
      const gp_Ax3 ax = plane.Position();
      const gp_Pnt origin = ax.Location();
      gp_Dir x_axis = ax.XDirection();
      gp_Dir y_axis = ax.YDirection();
      gp_Dir z_axis = ax.Direction();
      // Mirror the orientation flag onto the plane axes too — a reversed
      // face has its "outward" normal flipped, and the y-axis is flipped
      // to keep the frame right-handed.
      if (face.Orientation() == TopAbs_REVERSED) {
        z_axis.Reverse();
        y_axis.Reverse();
      }
      info.frame = ViewportSolidFace::PlaneFrame{
          .origin_x = origin.X(),
          .origin_y = origin.Y(),
          .origin_z = origin.Z(),
          .x_axis_x = x_axis.X(),
          .x_axis_y = x_axis.Y(),
          .x_axis_z = x_axis.Z(),
          .y_axis_x = y_axis.X(),
          .y_axis_y = y_axis.Y(),
          .y_axis_z = y_axis.Z(),
          .normal_x = z_axis.X(),
          .normal_y = z_axis.Y(),
          .normal_z = z_axis.Z(),
      };
      // Override center/normal with the plane axes for consistency.
      info.normal_x = z_axis.X();
      info.normal_y = z_axis.Y();
      info.normal_z = z_axis.Z();
    } else {
      // Non-planar face: synthesize a frame at the sampled center with
      // the sampled normal so the UI can still place a label / preview
      // marker. Sketch-on-face is rejected via `is_planar = false`.
      gp_Vec arbitrary = std::abs(normal.X()) < 0.9
                             ? gp_Vec(1.0, 0.0, 0.0)
                             : gp_Vec(0.0, 1.0, 0.0);
      gp_Vec x_axis = arbitrary.Crossed(normal);
      if (x_axis.Magnitude() > 0.0) {
        x_axis.Normalize();
      }
      gp_Vec y_axis = normal.Crossed(x_axis);
      if (y_axis.Magnitude() > 0.0) {
        y_axis.Normalize();
      }
      info.frame = ViewportSolidFace::PlaneFrame{
          .origin_x = center.X(),
          .origin_y = center.Y(),
          .origin_z = center.Z(),
          .x_axis_x = x_axis.X(),
          .x_axis_y = x_axis.Y(),
          .x_axis_z = x_axis.Z(),
          .y_axis_x = y_axis.X(),
          .y_axis_y = y_axis.Y(),
          .y_axis_z = y_axis.Z(),
          .normal_x = normal.X(),
          .normal_y = normal.Y(),
          .normal_z = normal.Z(),
      };
    }
  } catch (const std::exception&) {
    // Bad surface: leave defaults, mark non-planar.
    info.is_planar = false;
  }

  return info;
}

// Append every unique face of `body_shape` to `out`, owned by `body_id`.
// Each face is fully tessellated in world space so the UI can build a
// real BufferGeometry and the picker hits the actual face geometry.
// Face ids match the format expected by DocumentManager::select_face
// ("<owner_id>:face:<index>"), with index coming from
// TopExp::MapShapes(TopAbs_FACE).
void enumerate_body_faces(const TopoDS_Shape& body_shape,
                           const std::string& body_id,
                           const std::string& body_kind,
                           const DocumentState& document,
                           const std::optional<std::string>& selected_face_id,
                           std::vector<ViewportSolidFace>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  // BRepMesh_IncrementalMesh deflection mirrors body_compiler so the
  // face triangulation matches the body mesh that's already on screen.
  // Skipping this would leave faces without per-face triangulation when
  // body_compiler hasn't tessellated this body (legacy non-boolean path).
  try {
    BRepMesh_IncrementalMesh mesher(body_shape,
                                    /*deflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/true);
    if (!mesher.IsDone()) {
      return;
    }
  } catch (const std::exception&) {
    return;
  }

  TopTools_IndexedMapOfShape face_map;
  TopExp::MapShapes(body_shape, TopAbs_FACE, face_map);

  for (int i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(face_map(i));
    if (face.IsNull()) {
      continue;
    }

    const std::string face_id =
        body_id + ":face:" + std::to_string(i - 1);

    TopLoc_Location location;
    const Handle(Poly_Triangulation) triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      // No tessellation for this face — skip rather than emit a face
      // that can't be picked or rendered.
      continue;
    }

    const FaceFrameInfo frame_info = derive_face_frame(face);

    ViewportSolidFace primitive{};
    primitive.face_id = face_id;
    primitive.owner_id = body_id;
    primitive.owner_kind = body_kind;
    primitive.label = "Face " + std::to_string(i);
    primitive.sketchability = frame_info.is_planar ? "planar" : "non-planar";
    primitive.center_x = frame_info.center_x;
    primitive.center_y = frame_info.center_y;
    primitive.center_z = frame_info.center_z;
    primitive.normal_x = frame_info.normal_x;
    primitive.normal_y = frame_info.normal_y;
    primitive.normal_z = frame_info.normal_z;
    primitive.plane_frame = frame_info.frame;
    primitive.is_selected = selected_face_id.has_value() &&
                            selected_face_id.value() == face_id;
    primitive.appearance_color =
        face_appearance_color(document, face_id, appearance_face_signature(face));

    // Append per-face triangulation (positions in world space, indices
    // local to this face's positions array).
    const gp_Trsf transform = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int node_count = triangulation->NbNodes();
    primitive.triangle_positions.reserve(static_cast<size_t>(node_count) * 3);
    for (int node_index = 1; node_index <= node_count; ++node_index) {
      gp_Pnt node = triangulation->Node(node_index);
      node.Transform(transform);
      primitive.triangle_positions.push_back(node.X());
      primitive.triangle_positions.push_back(node.Y());
      primitive.triangle_positions.push_back(node.Z());
    }

    const int triangle_count = triangulation->NbTriangles();
    primitive.triangle_indices.reserve(static_cast<size_t>(triangle_count) * 3);
    for (int tri_index = 1; tri_index <= triangle_count; ++tri_index) {
      const Poly_Triangle& triangle = triangulation->Triangle(tri_index);
      int n1 = 0;
      int n2 = 0;
      int n3 = 0;
      triangle.Get(n1, n2, n3);
      if (reversed) {
        std::swap(n2, n3);
      }
      primitive.triangle_indices.push_back(n1 - 1);
      primitive.triangle_indices.push_back(n2 - 1);
      primitive.triangle_indices.push_back(n3 - 1);
    }

    if (primitive.triangle_positions.empty() ||
        primitive.triangle_indices.empty()) {
      continue;
    }

    out.push_back(std::move(primitive));
  }
}

// Tessellate `shape` into flat positions/normals/indices arrays. Used
// for the cut-preview emission below, which renders a translucent
// "this is what's about to be cut" volume so the user can see exactly
// which material the cut extrude will remove.
//
// The output is intentionally minimal compared to body_compiler's
// tessellate_shape: per-vertex normals are computed in a single pass
// (one normal per vertex, summed across triangles) without seam-
// splitting. The cut preview is a UI overlay so visual fidelity is
// less important than getting it on screen at all.
void tessellate_shape_to_arrays(const TopoDS_Shape& shape,
                                std::vector<double>& positions,
                                std::vector<double>& normals,
                                std::vector<int>& indices) {
  if (shape.IsNull()) {
    return;
  }
  try {
    BRepMesh_IncrementalMesh mesher(shape,
                                    /*deflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/true);
    if (!mesher.IsDone()) {
      return;
    }
  } catch (const std::exception&) {
    return;
  }

  for (TopExp_Explorer face_explorer(shape, TopAbs_FACE);
       face_explorer.More();
       face_explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(face_explorer.Current());
    TopLoc_Location location;
    const Handle(Poly_Triangulation) triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      continue;
    }
    const gp_Trsf transform = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int base_index = static_cast<int>(positions.size() / 3);
    const int node_count = triangulation->NbNodes();
    for (int i = 1; i <= node_count; ++i) {
      gp_Pnt node = triangulation->Node(i);
      node.Transform(transform);
      positions.push_back(node.X());
      positions.push_back(node.Y());
      positions.push_back(node.Z());
      normals.push_back(0.0);
      normals.push_back(0.0);
      normals.push_back(0.0);
    }
    const int triangle_count = triangulation->NbTriangles();
    for (int t = 1; t <= triangle_count; ++t) {
      const Poly_Triangle& triangle = triangulation->Triangle(t);
      int n1 = 0;
      int n2 = 0;
      int n3 = 0;
      triangle.Get(n1, n2, n3);
      if (reversed) {
        std::swap(n2, n3);
      }
      const int i1 = base_index + (n1 - 1);
      const int i2 = base_index + (n2 - 1);
      const int i3 = base_index + (n3 - 1);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i3);
      // Accumulate per-triangle normal onto each vertex so the simple
      // viewer flat-shading pass has something to work with.
      const double ax = positions[3 * i1 + 0];
      const double ay = positions[3 * i1 + 1];
      const double az = positions[3 * i1 + 2];
      const double bx = positions[3 * i2 + 0];
      const double by = positions[3 * i2 + 1];
      const double bz = positions[3 * i2 + 2];
      const double cx = positions[3 * i3 + 0];
      const double cy = positions[3 * i3 + 1];
      const double cz = positions[3 * i3 + 2];
      const double ux = bx - ax;
      const double uy = by - ay;
      const double uz = bz - az;
      const double vx = cx - ax;
      const double vy = cy - ay;
      const double vz = cz - az;
      const double nx = uy * vz - uz * vy;
      const double ny = uz * vx - ux * vz;
      const double nz = ux * vy - uy * vx;
      const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
      const double inv = length > 0.0 ? 1.0 / length : 0.0;
      const double nxn = nx * inv;
      const double nyn = ny * inv;
      const double nzn = nz * inv;
      for (int idx : {i1, i2, i3}) {
        normals[3 * idx + 0] += nxn;
        normals[3 * idx + 1] += nyn;
        normals[3 * idx + 2] += nzn;
      }
    }
  }
}

bool has_feature_id(const DocumentState& document, const std::string& id) {
  return std::any_of(document.feature_history.begin(),
                     document.feature_history.end(),
                     [&](const FeatureEntry& feature) {
                       return feature.id == id;
                     });
}

DocumentState document_at_timeline_cursor(const DocumentState& source) {
  if (!source.timeline_cursor.has_value()) {
    return source;
  }

  DocumentState snapshot = source;
  snapshot.feature_history.clear();

  int remaining_actions = std::max(0, source.timeline_cursor.value());
  for (const auto& feature : source.feature_history) {
    if (feature.kind == "root_part") {
      snapshot.feature_history.push_back(feature);
      continue;
    }
    if (remaining_actions <= 0) {
      continue;
    }
    snapshot.feature_history.push_back(feature);
    --remaining_actions;
  }

  if (snapshot.selected_feature_id.has_value() &&
      !has_feature_id(snapshot, snapshot.selected_feature_id.value())) {
    snapshot.selected_feature_id = std::nullopt;
  }
  if (snapshot.selected_reference_id.has_value() &&
      snapshot.selected_reference_id->rfind("ref-plane-", 0) != 0 &&
      !has_feature_id(snapshot, snapshot.selected_reference_id.value())) {
    snapshot.selected_reference_id = std::nullopt;
  }
  if (snapshot.active_sketch_feature_id.has_value() &&
      !has_feature_id(snapshot, snapshot.active_sketch_feature_id.value())) {
    snapshot.active_sketch_feature_id = std::nullopt;
    snapshot.active_sketch_plane_id = std::nullopt;
    snapshot.active_sketch_face_id = std::nullopt;
    snapshot.active_sketch_tool = std::nullopt;
    snapshot.selected_sketch_point_id = std::nullopt;
    snapshot.selected_sketch_entity_id = std::nullopt;
    snapshot.selected_sketch_dimension_id = std::nullopt;
    snapshot.selected_sketch_profile_id = std::nullopt;
    snapshot.selected_sketch_point_ids.clear();
    snapshot.selected_sketch_entity_ids.clear();
    snapshot.selected_sketch_profile_ids.clear();
  }
  snapshot.selected_face_id = std::nullopt;
  snapshot.selected_edge_ids.clear();
  snapshot.selected_vertex_ids.clear();

  return snapshot;
}
