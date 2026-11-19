// Per-face pick-proxy triangle budget for converted-mesh bodies (see
// enumerate_body_faces).
constexpr int kMaxMeshFacePickTriangles = 48;

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
  // Surface classification + cylinder witness (drilling hole-wall
  // candidates).  surface_kind mirrors ViewportSolidFace::surface_kind;
  // the cylinder fields are set only for GeomAbs_Cylinder surfaces.
  std::string surface_kind = "other";
  std::optional<std::array<double, 3>> cylinder_axis;
  std::optional<std::array<double, 3>> cylinder_location;
  std::optional<double> cylinder_radius;
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

    // Classify the underlying surface once; CAM consumers (drilling)
    // read this to find hole walls.  Cylinder faces additionally
    // carry the axis witness — the hole center lives on the cylinder
    // axis, not at the mid-UV `center` point.
    switch (type) {
      case GeomAbs_Plane:
        info.surface_kind = "planar";
        break;
      case GeomAbs_Cylinder: {
        info.surface_kind = "cylinder";
        const gp_Cylinder cylinder = surface.Cylinder();
        const gp_Pnt location = cylinder.Location();
        const gp_Dir axis = cylinder.Axis().Direction();
        info.cylinder_location =
            std::array<double, 3>{location.X(), location.Y(), location.Z()};
        info.cylinder_axis =
            std::array<double, 3>{axis.X(), axis.Y(), axis.Z()};
        info.cylinder_radius = cylinder.Radius();
        break;
      }
      case GeomAbs_Cone:
        info.surface_kind = "cone";
        break;
      case GeomAbs_Sphere:
        info.surface_kind = "sphere";
        break;
      case GeomAbs_Torus:
        info.surface_kind = "torus";
        break;
      case GeomAbs_BezierSurface:
      case GeomAbs_BSplineSurface:
        info.surface_kind = "spline";
        break;
      default:
        info.surface_kind = "other";
        break;
    }

    gp_Pnt center;
    gp_Vec normal;
    if (type == GeomAbs_Plane) {
      // Planar face: the plane branch below derives the frame from
      // the surface itself — only the representative center point is
      // needed. Use the first triangulation node (or wire vertex)
      // instead of BRepGProp_Face numerical integration, which is
      // the dominant per-face cost on converted-mesh bodies (measured
      // ~14 ms per face on an 1182-face panel, i.e. ~17 s total).
      TopLoc_Location loc;
      const occ::handle<Poly_Triangulation> tri =
          BRep_Tool::Triangulation(face, loc);
      if (!tri.IsNull() && tri->NbNodes() > 0) {
        center = tri->Node(1).Transformed(loc.Transformation());
      } else {
        for (TopExp_Explorer vexp(face, TopAbs_VERTEX); vexp.More();
             vexp.Next()) {
          center = BRep_Tool::Pnt(TopoDS::Vertex(vexp.Current()));
          break;
        }
      }
      normal = gp_Vec(0.0, 0.0, 1.0);  // overwritten by the plane branch
    } else {
      // Non-planar face: sample a representative (point, normal) via
      // numerical integration — rare outside converted meshes.
      const double u_mid =
          0.5 * (surface.FirstUParameter() + surface.LastUParameter());
      const double v_mid =
          0.5 * (surface.FirstVParameter() + surface.LastVParameter());
      BRepGProp_Face prop(face);
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
    }

    info.center_x = center.X();
    info.center_y = center.Y();
    info.center_z = center.Z();
    info.normal_x = normal.X();
    info.normal_y = normal.Y();
    info.normal_z = normal.Z();

    if (type == GeomAbs_Plane) {
      // Canonical axes (see plane_frame_derivation.h): the surface's own
      // plane placement is an import artifact — a STEP face lying flat in
      // XY can carry a plane rotated 180 degrees about Z — and reading it
      // verbatim rolled the sketch on imported bodies. The UI sends this
      // frame straight back with `start_sketch_on_face`, so it must match
      // what the dependency walker resolves for the same face.
      const std::optional<PlaneFrame> canonical =
          canonical_planar_face_frame(face);
      if (canonical.has_value()) {
        info.is_planar = true;
        info.frame = ViewportSolidFace::PlaneFrame{
            .origin_x = canonical->origin_x,
            .origin_y = canonical->origin_y,
            .origin_z = canonical->origin_z,
            .x_axis_x = canonical->x_axis_x,
            .x_axis_y = canonical->x_axis_y,
            .x_axis_z = canonical->x_axis_z,
            .y_axis_x = canonical->y_axis_x,
            .y_axis_y = canonical->y_axis_y,
            .y_axis_z = canonical->y_axis_z,
            .normal_x = canonical->normal_x,
            .normal_y = canonical->normal_y,
            .normal_z = canonical->normal_z,
        };
        // Override center/normal with the frame normal for consistency.
        info.normal_x = canonical->normal_x;
        info.normal_y = canonical->normal_y;
        info.normal_z = canonical->normal_z;
      }
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
    if (triangulation.IsNull() && body_kind != "mesh_to_body") {
      // No tessellation for this face — skip rather than emit a face
      // that can't be picked or rendered. (mesh_to_body faces are
      // emitted without pick geometry below.)
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
    primitive.surface_kind = frame_info.surface_kind;
    primitive.cylinder_axis = frame_info.cylinder_axis;
    primitive.cylinder_location = frame_info.cylinder_location;
    primitive.cylinder_radius = frame_info.cylinder_radius;
    primitive.is_selected = selected_face_id.has_value() &&
                            selected_face_id.value() == face_id;
    primitive.appearance_color =
        face_appearance_color(document, face_id, face_signature(face));

    // The face's pick geometry is NOT shipped: the body mesh carries
    // `face_ids` (one face ordinal per triangle), so the UI rebuilds the
    // face proxies from the mesh it already has. This deleted the second
    // copy of every triangle plus the whole pick-budget/decimation path
    // that existed only to keep that copy small.

    out.push_back(std::move(primitive));
  }
}
