// "<body_id>:face:<index>" — same shape used everywhere else in the
// core. Returns std::nullopt for non-face ids (legacy sketch on a
// reference plane, etc.). When the suffix isn't a non-negative
// integer, callers treat the id as a legacy named-face sketch and
// skip refreshing — those don't need it because their plane_frame is
// already invariant under upstream edits.
struct ParsedFaceTarget {
  std::string body_id;
  int face_index;
};

std::optional<ParsedFaceTarget> parse_face_target(const std::string &plane_id) {
  const std::string separator = ":face:";
  const auto pos = plane_id.find(separator);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  const std::string body_id = plane_id.substr(0, pos);
  const std::string suffix = plane_id.substr(pos + separator.size());
  if (suffix.empty()) {
    return std::nullopt;
  }
  try {
    size_t consumed = 0;
    const int index = std::stoi(suffix, &consumed);
    if (consumed != suffix.size() || index < 0) {
      return std::nullopt;
    }
    return ParsedFaceTarget{.body_id = body_id, .face_index = index};
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

// Build a PlaneFrame from a planar OCCT face. Mirrors the planar
// branch of `derive_face_frame` in viewport.cpp, kept inline here so
// the dependency walker doesn't reach across translation units. If
// the face isn't planar we return nullopt — sketches require a planar
// face by construction, so a non-planar resolution is treated the
// same as a missing face (broken dependency).
std::optional<PlaneFrame> frame_from_planar_face(const TopoDS_Face &face) {
  try {
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
      return std::nullopt;
    }
    const gp_Pln plane = surface.Plane();
    const gp_Ax3 ax = plane.Position();
    const gp_Pnt origin = ax.Location();
    gp_Dir x_axis = ax.XDirection();
    gp_Dir y_axis = ax.YDirection();
    gp_Dir z_axis = ax.Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
      // Same handed-frame convention as viewport.cpp: flip both the
      // normal and the y-axis on a reversed face so the resulting
      // basis stays right-handed and the normal points outward.
      z_axis.Reverse();
      y_axis.Reverse();
    }
    return PlaneFrame{
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
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

// Resolve the world-space frame of face `index` on body `body_id` in
// `document` by recompiling bodies and walking the body's face map.
// Returns nullopt when the body or face index can't be found, when
// the OCCT call throws, or when the face isn't planar.
std::optional<PlaneFrame> resolve_face_frame(const DocumentState &document,
                                             const std::string &body_id,
                                             int face_index) {
  if (face_index < 0) {
    return std::nullopt;
  }
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto &body : compiled.bodies) {
    if (body.id != body_id || body.shape.IsNull()) {
      continue;
    }
    TopTools_IndexedMapOfShape face_map;
    TopExp::MapShapes(body.shape, TopAbs_FACE, face_map);
    const int one_based = face_index + 1;
    if (one_based < 1 || one_based > face_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(face_map(one_based));
    if (face.IsNull()) {
      return std::nullopt;
    }
    return frame_from_planar_face(face);
  }
  return std::nullopt;
}

std::optional<PlaneFrame> frame_from_tangent_face(const TopoDS_Face &face) {
  if (face.IsNull()) {
    return std::nullopt;
  }
  try {
    BRepAdaptor_Surface surface(face);
    const double u_mid =
        0.5 * (surface.FirstUParameter() + surface.LastUParameter());
    const double v_mid =
        0.5 * (surface.FirstVParameter() + surface.LastVParameter());

    BRepGProp_Face prop(face);
    gp_Pnt center;
    gp_Vec normal;
    prop.Normal(u_mid, v_mid, center, normal);
    if (normal.Magnitude() <= 0.0) {
      return std::nullopt;
    }
    normal.Normalize();
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
    }

    gp_Vec arbitrary = std::abs(normal.X()) < 0.9 ? gp_Vec(1.0, 0.0, 0.0)
                                                  : gp_Vec(0.0, 1.0, 0.0);
    gp_Vec x_axis = arbitrary.Crossed(normal);
    if (x_axis.Magnitude() <= 0.0) {
      return std::nullopt;
    }
    x_axis.Normalize();
    gp_Vec y_axis = normal.Crossed(x_axis);
    if (y_axis.Magnitude() <= 0.0) {
      return std::nullopt;
    }
    y_axis.Normalize();

    return PlaneFrame{
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
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

std::optional<PlaneFrame> resolve_body_face_frame(const DocumentState &document,
                                                  const std::string &body_id,
                                                  int face_index,
                                                  bool require_planar) {
  if (face_index < 0) {
    return std::nullopt;
  }
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto &body : compiled.bodies) {
    if (body.id != body_id || body.shape.IsNull()) {
      continue;
    }
    TopTools_IndexedMapOfShape face_map;
    TopExp::MapShapes(body.shape, TopAbs_FACE, face_map);
    const int one_based = face_index + 1;
    if (one_based < 1 || one_based > face_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(face_map(one_based));
    if (face.IsNull()) {
      return std::nullopt;
    }
    return require_planar ? frame_from_planar_face(face)
                          : frame_from_tangent_face(face);
  }
  return std::nullopt;
}
