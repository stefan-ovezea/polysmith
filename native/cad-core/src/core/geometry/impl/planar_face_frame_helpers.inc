std::optional<PlaneFrame> derive_planar_frame(const TopoDS_Face& face) {
  if (face.IsNull()) {
    return std::nullopt;
  }
  try {
    BRepAdaptor_Surface surface(face, false);
    if (surface.GetType() != GeomAbs_Plane) {
      return std::nullopt;
    }
    gp_Pln plane = surface.Plane();
    gp_Ax3 axes = plane.Position();
    gp_Dir x_axis = axes.XDirection();
    gp_Dir y_axis = axes.YDirection();
    gp_Dir normal = axes.Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
      y_axis.Reverse();
    }
    const gp_Pnt origin = axes.Location();
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
        .normal_x = normal.X(),
        .normal_y = normal.Y(),
        .normal_z = normal.Z(),
    };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

SketchProfilePoint to_local_profile_point(const PlaneFrame& frame,
                                          const FaceOutlinePoint& world) {
  const double dx = world.x - frame.origin_x;
  const double dy = world.y - frame.origin_y;
  const double dz = world.z - frame.origin_z;
  return SketchProfilePoint{
      .x = dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z,
      .y = dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z,
  };
}
