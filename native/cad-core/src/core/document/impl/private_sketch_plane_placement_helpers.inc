// ── Re-parenting a sketch onto another plane ─────────────────────────────
//
// `redefine_sketch_plane` can mean two different things:
//
//   * The sketch must MOVE onto a different plane. It keeps its local
//     coordinates and adopts the target plane's frame — the long-standing
//     behaviour the face_projection_arc suite pins.
//
//   * The target plane is the plane the sketch already lies in (only its
//     identity changes — e.g. a body face re-parented to XY so the body can
//     be deleted). Adopting the target's frame verbatim would drag the
//     sketch, because the two frames are anchored differently: an imported
//     body's face plane is usually located at the part centre, while XY is
//     anchored at the model origin. That moved a board outline half the
//     board sideways (precedent 2026-11-17).
//
// The helpers below detect the second case and keep the sketch's world
// placement: its own origin and in-plane axes, slid along the target normal
// onto the target plane. Local coordinates stay valid, so nothing moves and
// every live projection keeps its alignment.

bool planes_are_parallel_same_facing(
    const SketchFeatureParameters::SketchPlaneFrame& current,
    const SketchFeatureParameters::SketchPlaneFrame& target) {
  const double dot = current.normal_x * target.normal_x +
                     current.normal_y * target.normal_y +
                     current.normal_z * target.normal_z;
  // Same direction only: an opposing normal mirrors the in-plane basis, and
  // silently mirroring a sketch is worse than moving it.
  return dot >= 1.0 - 1e-9;
}

SketchFeatureParameters::SketchPlaneFrame keep_sketch_placement_on_plane(
    const SketchFeatureParameters::SketchPlaneFrame& current,
    const SketchFeatureParameters::SketchPlaneFrame& target) {
  const double dx = current.origin_x - target.origin_x;
  const double dy = current.origin_y - target.origin_y;
  const double dz = current.origin_z - target.origin_z;
  const double along =
      dx * target.normal_x + dy * target.normal_y + dz * target.normal_z;
  return SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = current.origin_x - along * target.normal_x,
      .origin_y = current.origin_y - along * target.normal_y,
      .origin_z = current.origin_z - along * target.normal_z,
      .x_axis_x = current.x_axis_x,
      .x_axis_y = current.x_axis_y,
      .x_axis_z = current.x_axis_z,
      .y_axis_x = current.y_axis_x,
      .y_axis_y = current.y_axis_y,
      .y_axis_z = current.y_axis_z,
      .normal_x = target.normal_x,
      .normal_y = target.normal_y,
      .normal_z = target.normal_z,
  };
}
