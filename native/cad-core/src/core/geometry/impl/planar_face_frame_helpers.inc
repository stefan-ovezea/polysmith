// Fold a planar face's surface into a PlaneFrame. The canonical
// derivation lives in one place (plane_frame_derivation.cpp) so the
// viewport, the dependency walker and the profile projection all agree
// on the same axes for the same face.
std::optional<PlaneFrame> derive_planar_frame(const TopoDS_Face& face) {
  return canonical_planar_face_frame(face);
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
