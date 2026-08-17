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
                                    /*isInParallel=*/false);
    if (!mesher.IsDone()) {
      return;
    }
  } catch (const std::exception&) {
    return;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(body_shape, TopAbs_FACE, face_map);

  for (int i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(face_map(i));
    if (face.IsNull()) {
      continue;
    }

    const std::string face_id =
        body_id + ":face:" + std::to_string(i - 1);

    TopLoc_Location location;
    const occ::handle<Poly_Triangulation> triangulation =
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
