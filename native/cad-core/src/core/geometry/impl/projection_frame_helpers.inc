// Flatten a world-space point onto a sketch plane's local (u, v).
// Mirror of `world_to_sketch_local` in document.cpp — pulled in here
// so the live-projection refresher doesn't have to reach across
// translation units. Identical maths.
std::pair<double, double> world_to_sketch_local(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    double wx, double wy, double wz) {
  const double dx = wx - frame.origin_x;
  const double dy = wy - frame.origin_y;
  const double dz = wz - frame.origin_z;
  const double sx =
      dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z;
  const double sy =
      dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z;
  return {sx, sy};
}

std::array<double, 3> sketch_local_to_world(const PlaneFrame& frame,
                                            double local_x,
                                            double local_y) {
  return {
      frame.origin_x + local_x * frame.x_axis_x + local_y * frame.y_axis_x,
      frame.origin_y + local_x * frame.x_axis_y + local_y * frame.y_axis_y,
      frame.origin_z + local_x * frame.x_axis_z + local_y * frame.y_axis_z,
  };
}
