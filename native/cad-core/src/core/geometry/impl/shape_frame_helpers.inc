constexpr double kPi = 3.14159265358979323846;

gp_Pnt to_world_point(const std::string& plane_id, double local_x, double local_y) {
  if (plane_id == "ref-plane-xy") {
    return gp_Pnt(local_x, 0.0, local_y);
  }

  if (plane_id == "ref-plane-yz") {
    return gp_Pnt(0.0, local_x, local_y);
  }

  if (plane_id == "ref-plane-xz") {
    return gp_Pnt(local_x, local_y, 0.0);
  }

  throw std::runtime_error("Unsupported sketch plane for shape: " + plane_id);
}

gp_Pnt to_world_point(const PlaneFrame& frame, double local_x, double local_y) {
  return gp_Pnt(frame.origin_x + frame.x_axis_x * local_x +
                    frame.y_axis_x * local_y,
                frame.origin_y + frame.x_axis_y * local_x +
                    frame.y_axis_y * local_y,
                frame.origin_z + frame.x_axis_z * local_x +
                    frame.y_axis_z * local_y);
}

gp_Pnt to_world_point(const ExtrudeFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    return to_world_point(parameters.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(parameters.plane_id, local_x, local_y);
}

gp_Pnt to_world_point(const LoftSectionParameters& section,
                      double local_x,
                      double local_y) {
  if (section.plane_frame.has_value()) {
    return to_world_point(section.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(section.plane_id, local_x, local_y);
}

gp_Pnt to_world_point(const RevolveFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    return to_world_point(parameters.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(parameters.plane_id, local_x, local_y);
}

gp_Pnt to_world_point(const SweepFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    return to_world_point(parameters.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(parameters.plane_id, local_x, local_y);
}

gp_Dir plane_normal(const ExtrudeFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    return gp_Dir(frame.normal_x, frame.normal_y, frame.normal_z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, 1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for shape: " +
                           parameters.plane_id);
}

gp_Dir profile_wire_normal(const LoftSectionParameters& section) {
  if (section.plane_frame.has_value()) {
    const auto& frame = section.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (section.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (section.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (section.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for loft: " +
                           section.plane_id);
}

gp_Dir profile_wire_normal(const ExtrudeFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    // Sketch-local coordinates on XY are mapped as (x, z) in world space.
    // The local wire normal is therefore X x Z = -Y, while extrusion still
    // uses +Y. Exact circle wires must follow the local wire orientation so
    // hole reversal matches polygon loops built from the same sketch points.
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for shape: " +
                           parameters.plane_id);
}

gp_Dir profile_wire_normal(const RevolveFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for revolve: " +
                           parameters.plane_id);
}

gp_Dir profile_wire_normal(const SweepFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for sweep: " +
                           parameters.plane_id);
}

gp_Vec extrusion_vector(const std::string& plane_id, double depth) {
  if (plane_id == "ref-plane-xy") {
    return gp_Vec(0.0, depth, 0.0);
  }

  if (plane_id == "ref-plane-yz") {
    return gp_Vec(depth, 0.0, 0.0);
  }

  if (plane_id == "ref-plane-xz") {
    return gp_Vec(0.0, 0.0, depth);
  }

  throw std::runtime_error("Unsupported sketch plane for shape: " + plane_id);
}

gp_Vec extrusion_vector(const PlaneFrame& frame, double depth) {
  return gp_Vec(frame.normal_x * depth,
                frame.normal_y * depth,
                frame.normal_z * depth);
}

gp_Vec extrusion_vector(const ExtrudeFeatureParameters& parameters,
                        double depth) {
  return parameters.plane_frame.has_value()
             ? extrusion_vector(parameters.plane_frame.value(), depth)
             : extrusion_vector(parameters.plane_id, depth);
}

gp_Vec extrusion_normal_vector(const ExtrudeFeatureParameters& parameters) {
  return extrusion_vector(parameters, 1.0);
}

TopoDS_Shape translate_shape(const TopoDS_Shape& shape, const gp_Vec& offset) {
  if (shape.IsNull() || offset.SquareMagnitude() <= 1.0e-18) {
    return shape;
  }
  gp_Trsf transform;
  transform.SetTranslation(offset);
  return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

TopoDS_Shape fuse_shapes(const TopoDS_Shape& first, const TopoDS_Shape& second) {
  if (first.IsNull()) {
    return second;
  }
  if (second.IsNull()) {
    return first;
  }
  const TopoDS_Shape fused = BRepAlgoAPI_Fuse(first, second).Shape();
  return fused.IsNull() ? first : fused;
}

TopoDS_Shape apply_draft_angle(const TopoDS_Shape& shape,
                               const ExtrudeFeatureParameters& parameters,
                               double side_sign,
                               double angle_degrees,
                               double start_offset) {
  if (shape.IsNull() || std::abs(angle_degrees) <= 1.0e-9) {
    return shape;
  }

  const gp_Vec normal_vec = extrusion_normal_vector(parameters);
  gp_Dir direction(normal_vec.X() * side_sign,
                   normal_vec.Y() * side_sign,
                   normal_vec.Z() * side_sign);
  gp_Pnt plane_origin =
      parameters.plane_frame.has_value()
          ? gp_Pnt(parameters.plane_frame->origin_x,
                   parameters.plane_frame->origin_y,
                   parameters.plane_frame->origin_z)
          : to_world_point(parameters.plane_id, 0.0, 0.0);
  plane_origin.Translate(gp_Vec(direction) * start_offset);
  const gp_Pln neutral_plane(plane_origin, direction);

  try {
    BRepOffsetAPI_DraftAngle draft(shape);
    bool any_face = false;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More();
         explorer.Next()) {
      const TopoDS_Face face = TopoDS::Face(explorer.Current());
      const Handle(Geom_Plane) surface =
          Handle(Geom_Plane)::DownCast(BRep_Tool::Surface(face));
      if (surface.IsNull()) {
        continue;
      }
      const gp_Dir face_normal = surface->Pln().Axis().Direction();
      if (!face_normal.IsNormal(direction, 1.0e-5)) {
        continue;
      }
      draft.Add(face, direction, angle_degrees * kPi / 180.0, neutral_plane);
      if (draft.AddDone()) {
        any_face = true;
      } else {
        draft.Remove(face);
      }
    }
    if (!any_face) {
      return shape;
    }
    const TopoDS_Shape drafted = draft.Shape();
    if (drafted.IsNull()) {
      throw std::runtime_error(
          "Extrude taper failed. Reduce the taper angle, increase the distance, "
          "or use a wider profile.");
    }
    return drafted;
  } catch (const Standard_Failure&) {
    throw std::runtime_error(
        "Extrude taper failed. Reduce the taper angle, increase the distance, "
        "or use a wider profile.");
  }
}
