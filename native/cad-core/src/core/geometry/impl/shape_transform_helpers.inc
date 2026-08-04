constexpr double kPi = 3.14159265358979323846;

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
  BRepAlgoAPI_Fuse fuse_op(first, second);
  fuse_op.SimplifyResult();
  const TopoDS_Shape fused = fuse_op.Shape();
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
      const occ::handle<Geom_Plane> surface =
          occ::down_cast<Geom_Plane>(BRep_Tool::Surface(face));
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
  } catch (const std::exception& e) {
    throw std::runtime_error(
        std::string("Extrude taper failed (OCCT): ") + e.what());
  }
}
