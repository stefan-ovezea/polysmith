// ================================================================
// Arc helpers
// ================================================================

namespace {

std::pair<double, double> arc_angles(const SketchArc &arc) {
  double a_start = wrap_angle(
      std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x));
  double a_end = wrap_angle(
      std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x));
  return {a_start, a_end};
}

bool angle_in_arc_sweep(double a, double a_start, double a_end, bool ccw) {
  a = wrap_angle(a);
  a_start = wrap_angle(a_start);
  a_end = wrap_angle(a_end);
  if (ccw) {
    if (a_end <= a_start)
      a_end += 2.0 * M_PI;
    if (a < a_start)
      a += 2.0 * M_PI;
    return a >= a_start - 1e-12 && a <= a_end + 1e-12;
  } else {
    // CW sweep: from a_start down to a_end.  When the sweep wraps
    // (a_start <= a_end after wrapping) a_start moves up by 2π; angles
    // below a_end — the part of the sweep crossing the +x axis — must
    // shift up by 2π too, or they never land in the [a_end, a_start]
    // window.
    if (a_start <= a_end)
      a_start += 2.0 * M_PI;
    if (a < a_end)
      a += 2.0 * M_PI;
    return a <= a_start + 1e-12 && a >= a_end - 1e-12;
  }
}

} // namespace
