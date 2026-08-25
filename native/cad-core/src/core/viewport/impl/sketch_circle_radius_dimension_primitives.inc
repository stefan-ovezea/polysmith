// Circle radius / diameter dimension.
//
// The label is placed from the dimension's stored sketch-local override
// when the user has dragged it, otherwise at the historical default
// (+X of the circle, one kCircleDimensionOffset clear of the rim) so
// existing sketches do not shift.
//
// Radius mode emits a single contact point on the rim in the label's
// direction; the renderer draws the arrowhead there, a leader out to the
// label, and a short landing under the text. Diameter mode emits both
// rim contacts so the renderer can draw the conventional through-centre
// double-headed dimension along the label direction.
ViewportSketchDimensionPrimitive make_circle_dimension_primitive(
    const SketchCircle &circle, const SketchDimension &dimension,
    const SketchFeatureParameters &parameters, bool is_selected) {
  const bool show_radius = dimension.display_as == "radius";
  const std::string prefix = show_radius ? "R " : "\xe2\x8c\x80 ";
  const double display_value = show_radius ? dimension.value
                                           : dimension.value * 2.0;

  const SketchLabelPoint2D label = resolve_dimension_label_local(
      dimension,
      circle.center_x + circle.radius + kCircleDimensionOffset,
      circle.center_y);
  const SketchLabelPoint2D unit = radial_contact_direction(
      circle.center_x, circle.center_y, label.x, label.y);

  const WorldPoint center =
      to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint label_world =
      to_world_point(parameters, label.x, label.y);
  // Contact on the rim toward the label, and its antipode for diameter.
  const WorldPoint near_contact = to_world_point(
      parameters, circle.center_x + unit.x * circle.radius,
      circle.center_y + unit.y * circle.radius);
  const WorldPoint far_contact = to_world_point(
      parameters, circle.center_x - unit.x * circle.radius,
      circle.center_y - unit.y * circle.radius);
  // The rim point a quarter turn from the contact. The centre, contact
  // and label are collinear, so the renderer has no cross product to
  // derive the sketch plane from; this gives it an in-plane reference
  // direction for orienting the arrowheads.
  const WorldPoint quadrant = to_world_point(
      parameters, circle.center_x - unit.y * circle.radius,
      circle.center_y + unit.x * circle.radius);

  // anchor_start is where the leader leaves the rim; dimension_start/end
  // are the arrowhead tips and coincide in radius mode (one arrow).
  const WorldPoint anchor_start = near_contact;
  const WorldPoint anchor_end = quadrant;
  const WorldPoint dimension_start = show_radius ? near_contact : far_contact;
  const WorldPoint dimension_end = near_contact;

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .label = prefix + format_dimension_value(display_value) + " mm",
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
      // The renderer derives the leader and arrowheads from the centre,
      // so it cannot rely on the old midpoint-of-anchors heuristic once
      // dimension_end is the label rather than the far rim contact.
      .arc_center_x = center.x,
      .arc_center_y = center.y,
      .arc_center_z = center.z,
      .arc_radius = circle.radius,
      .driven = dimension.driven,
      .display_as = dimension.display_as,
  };
}
