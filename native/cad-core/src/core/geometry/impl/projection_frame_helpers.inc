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

// Mirror of circle_axis_parallel_to_sketch in document.cpp's
// private_projection_command_helpers.inc (this TU can't reach across).
bool circle_axis_parallel_to_sketch(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    const FaceOutlinePoint& axis,
    double tolerance = 1e-6) {
  const double dot = axis.x * frame.normal_x + axis.y * frame.normal_y +
                     axis.z * frame.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

// Mirror of arc_ccw_from_local (same rationale) — sweep direction from
// the sketch-local geometry; orientation-proof against wire reversals.
bool arc_ccw_from_local(const std::pair<double, double>& start,
                        const std::pair<double, double>& end,
                        const std::pair<double, double>& mid) {
  const double ux = mid.first - start.first;
  const double uy = mid.second - start.second;
  const double vx = end.first - start.first;
  const double vy = end.second - start.second;
  return ux * vy - uy * vx > 0.0;
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
