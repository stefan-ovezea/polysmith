// Radial dimension leaders: arc radius, arc length, arc angle.
//
// Every radial kind stores its label as an absolute sketch-local point
// (SketchDimension::label_x / label_y). These helpers turn that stored
// point into the emitted leader geometry, so a label the user dragged
// stays where it was dropped instead of snapping back to a hardcoded
// default. Before this existed, the radius/arc emitters ignored the
// stored label entirely and the post-drag viewport refetch reverted it.
//
// The short horizontal landing under the value text is deliberately NOT
// computed here. It is a presentational detail the renderer derives from
// the contact point and the label, which keeps the live drag preview and
// the emitted geometry on one code path — two implementations of it is
// exactly how a label starts jumping on pointer release.
//
// Field convention for the leader kinds (arc_radius and circle_radius):
//
//   anchor_start    leader origin on the curve
//   anchor_end      rim point a quarter turn from the contact. The
//                   centre, contact and label are collinear, so the
//                   renderer cannot cross them to recover the sketch
//                   plane; this is its in-plane direction reference for
//                   orienting arrowheads. A direction only — it need not
//                   lie on the material arc.
//   dimension_start first arrowhead tip
//   dimension_end   second arrowhead tip, equal to the first when the
//                   kind draws a single arrow (radius, arc radius)
//   label_position  the label itself
//   arc_center      centre, and arc_radius the measured radius
//
// The span kinds (arc_length, arc_angle) instead use anchor_start/end as
// the witness-line feet and dimension_start/end as the extension-arc
// endpoints, with arc_radius carrying the extension radius.

// `SketchLabelPoint2D`, `resolve_dimension_label_local` and
// `radial_contact_direction` live in sketch_dimension_shared_helpers.inc
// because the circle emitter needs them too and is included earlier.

// Keep a radius arrowhead on the material arc. When the label points
// into the arc's missing part, snap the contact angle to whichever end
// of the opening is angularly nearer. `sweep` is the positive sweep
// magnitude measured from `start_angle` in the arc's own direction
// (ccw = increasing angle), matching how the arc emitters compute it.
inline double clamp_angle_into_arc_span(double angle, double start_angle,
                                        double sweep, bool ccw) {
  const double two_pi = 2.0 * kPi;
  // Offset of `angle` from the arc start, measured in the arc's own
  // sweep direction and normalized to [0, 2pi).
  double offset = ccw ? angle - start_angle : start_angle - angle;
  offset = std::fmod(std::fmod(offset, two_pi) + two_pi, two_pi);
  if (offset <= sweep) {
    return angle;
  }
  const double past_end = offset - sweep;
  const double before_start = two_pi - offset;
  const double snapped = past_end <= before_start ? sweep : 0.0;
  return ccw ? start_angle + snapped : start_angle - snapped;
}

// Sweep geometry shared by all three arc dimension emitters.
struct ArcSweep2D {
  double start_angle;
  double end_angle;
  double sweep;      // positive magnitude in the arc's own direction
  double mid_angle;  // midpoint of the sweep
};

inline ArcSweep2D compute_arc_sweep_2d(const SketchArc& arc) {
  const double two_pi = 2.0 * kPi;
  const double start_angle =
      std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
  const double end_angle =
      std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
  const double sweep = arc.ccw
                           ? std::fmod(end_angle - start_angle + two_pi, two_pi)
                           : std::fmod(start_angle - end_angle + two_pi, two_pi);
  const double mid_angle =
      arc.ccw ? start_angle + sweep * 0.5 : start_angle - sweep * 0.5;
  return ArcSweep2D{start_angle, end_angle, sweep, mid_angle};
}

// Default label placement for an arc dimension: just outside the arc's
// midpoint, so an undragged label never sits on top of its own leader.
inline SketchLabelPoint2D arc_default_label_local(const SketchArc& arc,
                                                  const ArcSweep2D& sweep) {
  const double distance = arc.radius + kCircleDimensionOffset;
  return SketchLabelPoint2D{
      arc.center_x + std::cos(sweep.mid_angle) * distance,
      arc.center_y + std::sin(sweep.mid_angle) * distance,
  };
}

// Arc radius: arrowhead on the curve, leader out to the label. The
// contact angle is clamped into the arc's sweep so the arrowhead lands
// on material even when the label is dragged past an endpoint.
ViewportSketchDimensionPrimitive make_arc_radius_dimension_primitive(
    const SketchArc& arc, const SketchDimension& dimension,
    const SketchFeatureParameters& parameters, bool is_selected) {
  const ArcSweep2D sweep = compute_arc_sweep_2d(arc);
  const SketchLabelPoint2D fallback = arc_default_label_local(arc, sweep);
  const SketchLabelPoint2D label =
      resolve_dimension_label_local(dimension, fallback.x, fallback.y);
  const SketchLabelPoint2D unit = radial_contact_direction(
      arc.center_x, arc.center_y, label.x, label.y);
  const double contact_angle = clamp_angle_into_arc_span(
      std::atan2(unit.y, unit.x), sweep.start_angle, sweep.sweep, arc.ccw);

  const WorldPoint contact = to_world_point(
      parameters,
      arc.center_x + arc.radius * std::cos(contact_angle),
      arc.center_y + arc.radius * std::sin(contact_angle));
  // A quarter turn from the contact. The centre, contact and label are
  // collinear, so the renderer has no cross product to derive the sketch
  // plane from; this is its in-plane reference for arrowhead orientation
  // (a direction only — it need not lie on the material arc).
  const WorldPoint quadrant = to_world_point(
      parameters,
      arc.center_x - arc.radius * std::sin(contact_angle),
      arc.center_y + arc.radius * std::cos(contact_angle));
  const WorldPoint label_world =
      to_world_point(parameters, label.x, label.y);
  const WorldPoint center =
      to_world_point(parameters, arc.center_x, arc.center_y);

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "arc_radius",
      .entity_id = arc.id,
      .label = "R " + format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = contact.x,
      .anchor_start_y = contact.y,
      .anchor_start_z = contact.z,
      .anchor_end_x = quadrant.x,
      .anchor_end_y = quadrant.y,
      .anchor_end_z = quadrant.z,
      // One arrowhead: both tips coincide on the contact. The label is
      // carried by label_position, so nothing is lost by not repeating it.
      .dimension_start_x = contact.x,
      .dimension_start_y = contact.y,
      .dimension_start_z = contact.z,
      .dimension_end_x = contact.x,
      .dimension_end_y = contact.y,
      .dimension_end_z = contact.z,
      .label_x = label_world.x,
      .label_y = label_world.y,
      .label_z = label_world.z,
      .arc_center_x = center.x,
      .arc_center_y = center.y,
      .arc_center_z = center.z,
      .arc_radius = arc.radius,
      .arc_start_angle = sweep.start_angle,
      .arc_end_angle = sweep.end_angle,
      .arc_ccw = arc.ccw,
      .driven = dimension.driven,
      .display_as = "",
  };
}

// Arc length: a concentric extension arc at the label's radius, with the
// arc's own endpoints as the witness-line feet. The renderer draws the
// extension arc between dimension_start/end about arc_center and the two
// witness lines from the anchors out to them.
ViewportSketchDimensionPrimitive make_arc_length_dimension_primitive(
    const SketchArc& arc, const SketchDimension& dimension,
    const SketchFeatureParameters& parameters, bool is_selected) {
  const ArcSweep2D sweep = compute_arc_sweep_2d(arc);
  const SketchLabelPoint2D fallback = arc_default_label_local(arc, sweep);
  const SketchLabelPoint2D label =
      resolve_dimension_label_local(dimension, fallback.x, fallback.y);

  const double raw_dx = label.x - arc.center_x;
  const double raw_dy = label.y - arc.center_y;
  const double extension_radius = std::max(
      arc.radius + kArcLengthMinGap,
      std::min(std::sqrt(raw_dx * raw_dx + raw_dy * raw_dy), 500.0));

  const WorldPoint anchor_start =
      to_world_point(parameters, arc.start_x, arc.start_y);
  const WorldPoint anchor_end =
      to_world_point(parameters, arc.end_x, arc.end_y);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      arc.center_x + std::cos(sweep.start_angle) * extension_radius,
      arc.center_y + std::sin(sweep.start_angle) * extension_radius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      arc.center_x + std::cos(sweep.end_angle) * extension_radius,
      arc.center_y + std::sin(sweep.end_angle) * extension_radius);
  const WorldPoint label_world =
      to_world_point(parameters, label.x, label.y);
  const WorldPoint center =
      to_world_point(parameters, arc.center_x, arc.center_y);

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "arc_length",
      .entity_id = arc.id,
      .label = "L " + format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label_world.x,
      .label_y = label_world.y,
      .label_z = label_world.z,
      .arc_center_x = center.x,
      .arc_center_y = center.y,
      .arc_center_z = center.z,
      .arc_radius = extension_radius,
      .arc_start_angle = sweep.start_angle,
      .arc_end_angle = sweep.end_angle,
      .arc_ccw = arc.ccw,
      .driven = dimension.driven,
      .display_as = "",
  };
}

// Arc angle: the arc's own sweep, drawn like a line-angle dimension.
// Unlike the other radial kinds this keeps the 1-DOF bisector placement
// that angle dimensions already use — the drag distance sets the radius
// and the label rides the bisector, which is the convention the existing
// angle dimensions follow.
ViewportSketchDimensionPrimitive make_arc_angle_dimension_primitive(
    const SketchArc& arc, const SketchDimension& dimension,
    const SketchFeatureParameters& parameters, bool is_selected) {
  const ArcSweep2D sweep = compute_arc_sweep_2d(arc);
  const double start_ux = std::cos(sweep.start_angle);
  const double start_uy = std::sin(sweep.start_angle);
  const double end_ux = std::cos(sweep.end_angle);
  const double end_uy = std::sin(sweep.end_angle);

  // Bisector of the two rays; for a half-turn arc the rays cancel, so
  // fall back to the normal of the start ray (same guard the line-angle
  // emitter uses).
  double bisector_x = start_ux + end_ux;
  double bisector_y = start_uy + end_uy;
  double bisector_length =
      std::sqrt(bisector_x * bisector_x + bisector_y * bisector_y);
  if (bisector_length <= 1e-9) {
    bisector_x = -start_uy;
    bisector_y = start_ux;
    bisector_length = 1.0;
  }
  bisector_x /= bisector_length;
  bisector_y /= bisector_length;
  // The bisector of the reflex side points away from the swept region;
  // flip it so the label sits inside the measured angle.
  const double mid_ux = std::cos(sweep.mid_angle);
  const double mid_uy = std::sin(sweep.mid_angle);
  if (bisector_x * mid_ux + bisector_y * mid_uy < 0.0) {
    bisector_x = -bisector_x;
    bisector_y = -bisector_y;
  }

  double extension_radius = std::max(8.0, std::min(arc.radius, 500.0));
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double raw_dx = *dimension.label_x - arc.center_x;
    const double raw_dy = *dimension.label_y - arc.center_y;
    extension_radius = std::max(
        6.0, std::min(std::sqrt(raw_dx * raw_dx + raw_dy * raw_dy), 500.0));
  }

  const WorldPoint anchor_start =
      to_world_point(parameters, arc.start_x, arc.start_y);
  const WorldPoint anchor_end =
      to_world_point(parameters, arc.end_x, arc.end_y);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      arc.center_x + start_ux * extension_radius,
      arc.center_y + start_uy * extension_radius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      arc.center_x + end_ux * extension_radius,
      arc.center_y + end_uy * extension_radius);
  const WorldPoint label = to_world_point(
      parameters,
      arc.center_x + bisector_x * extension_radius,
      arc.center_y + bisector_y * extension_radius);
  const WorldPoint center =
      to_world_point(parameters, arc.center_x, arc.center_y);

  const double degrees = std::abs(dimension.value) * 180.0 / kPi;
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "arc_angle",
      .entity_id = arc.id,
      .label = format_dimension_value(degrees) + "\xc2\xb0",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
      .arc_center_x = center.x,
      .arc_center_y = center.y,
      .arc_center_z = center.z,
      .arc_radius = extension_radius,
      .arc_start_angle = sweep.start_angle,
      .arc_end_angle = sweep.end_angle,
      .arc_ccw = arc.ccw,
      .ref_line_start_x = center.x,
      .ref_line_start_y = center.y,
      .ref_line_start_z = center.z,
      .ref_line_end_x = dimension_start.x,
      .ref_line_end_y = dimension_start.y,
      .ref_line_end_z = dimension_start.z,
      .driven = dimension.driven,
      .display_as = "",
  };
}
