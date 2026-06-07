ViewportSketchLinePrimitive make_sketch_line_primitive(const SketchLine& line,
                                                       const SketchFeatureParameters& parameters,
                                                       bool is_selected) {
  const WorldPoint start = to_world_point(parameters, line.start_x, line.start_y);
  const WorldPoint end = to_world_point(parameters, line.end_x, line.end_y);

  return ViewportSketchLinePrimitive{
      .line_id = line.id,
      .start_point_id = line.start_point_id,
      .end_point_id = line.end_point_id,
      .plane_id = parameters.plane_id,
      .start_x = start.x,
      .start_y = start.y,
      .start_z = start.z,
      .end_x = end.x,
      .end_y = end.y,
      .end_z = end.z,
      .is_selected = is_selected,
      .constraint = line.constraint,
      .is_construction = line.is_construction,
  };
}

ViewportSketchPolygonPrimitive make_sketch_polygon_primitive(
    const SketchPolygon& polygon,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, polygon.center_x, polygon.center_y);

  ViewportSketchPolygonPrimitive primitive{
      .polygon_id = polygon.id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .sides = polygon.sides,
      .mode = polygon.mode,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = polygon.radius,
      .is_selected = is_selected,
      .is_construction = polygon.is_construction,
  };

  // Compute world-space corners
  const int n = polygon.sides;
  // Align first vertex / edge midpoint with the click direction.
  double angle_offset = -M_PI / 2.0;
  if (polygon.mode == "inscribed") {
    angle_offset = std::atan2(polygon.end_y - polygon.center_y, polygon.end_x - polygon.center_x);
  } else if (polygon.mode == "circumscribed") {
    // Click is edge midpoint; rotate so edge 0 sits there.
    angle_offset = std::atan2(polygon.end_y - polygon.center_y, polygon.end_x - polygon.center_x) + M_PI / n;
  } else if (polygon.mode == "edge") {
    angle_offset = std::atan2(polygon.start_y - polygon.center_y, polygon.start_x - polygon.center_x);
  }
  for (int i = 0; i < n; ++i) {
    double angle = angle_offset + 2.0 * M_PI * i / n;
    double local_x = polygon.center_x + polygon.radius * std::cos(angle);
    double local_y = polygon.center_y + polygon.radius * std::sin(angle);
    if (polygon.mode == "circumscribed") {
      double r = polygon.radius / std::cos(M_PI / n);
      local_x = polygon.center_x + r * std::cos(angle);
      local_y = polygon.center_y + r * std::sin(angle);
    }
    const WorldPoint corner = to_world_point(parameters, local_x, local_y);
    primitive.corner_x.push_back(corner.x);
    primitive.corner_y.push_back(corner.y);
    primitive.corner_z.push_back(corner.z);
  }
  return primitive;
}

ViewportSketchCirclePrimitive make_sketch_circle_primitive(
    const SketchCircle& circle,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, circle.center_x, circle.center_y);

  return ViewportSketchCirclePrimitive{
      .circle_id = circle.id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = circle.radius,
      .is_selected = is_selected,
      .is_construction = circle.is_construction,
  };
}

ViewportSketchArcPrimitive make_sketch_arc_primitive(
    const SketchArc& arc,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, arc.center_x, arc.center_y);
  const WorldPoint start = to_world_point(parameters, arc.start_x, arc.start_y);
  const WorldPoint end = to_world_point(parameters, arc.end_x, arc.end_y);

  return ViewportSketchArcPrimitive{
      .arc_id = arc.id,
      .start_point_id = arc.start_point_id,
      .end_point_id = arc.end_point_id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = arc.radius,
      .start_x = start.x,
      .start_y = start.y,
      .start_z = start.z,
      .end_x = end.x,
      .end_y = end.y,
      .end_z = end.z,
      .ccw = arc.ccw,
      .is_selected = is_selected,
      .is_construction = arc.is_construction,
  };
}

ViewportSketchPointPrimitive make_sketch_point_primitive(
    const SketchPoint& point,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint world = to_world_point(parameters, point.x, point.y);

  return ViewportSketchPointPrimitive{
      .point_id = point.id,
      .plane_id = parameters.plane_id,
      .kind = point.kind,
      .position_x = world.x,
      .position_y = world.y,
      .position_z = world.z,
      .is_fixed = point.is_fixed,
      .is_selected = is_selected,
  };
}

// Render a dimension value as a short, human-friendly string with no
// trailing zeros: 12 -> "12", 12.5 -> "12.5", 12.50 -> "12.5",
// 12.345678 -> "12.35". std::to_string on a double produces six
// trailing decimals which makes the canvas-rendered dimension labels
// noisy ("12.500000 mm"). Two decimals of precision is enough for the
// viewport readout; the underlying value the core stores is unchanged.
std::string format_dimension_value(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  std::string text = buffer;
  if (text.find('.') != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  if (text == "-0") {
    text = "0";
  }
  return text;
}

// Centroid (in sketch-local 2D coords) of every line midpoint and
// circle center in the sketch. Used as the "inside" reference when
// placing line dimensions so the dimension line lands on the side of
// the segment that points away from the rest of the sketch — i.e. on
// the outside of a closed profile such as a rectangle. Falls back to
// the line's own midpoint when the sketch has no other entities, in
// which case the offset direction does not matter.
struct SketchCentroid2D {
  double x;
  double y;
  bool has_value;
};

SketchCentroid2D compute_sketch_centroid_2d(
    const SketchFeatureParameters& parameters) {
  double sum_x = 0.0;
  double sum_y = 0.0;
  int count = 0;
  for (const auto& other_line : parameters.lines) {
    sum_x += (other_line.start_x + other_line.end_x) / 2.0;
    sum_y += (other_line.start_y + other_line.end_y) / 2.0;
    count += 1;
  }
  for (const auto& other_circle : parameters.circles) {
    sum_x += other_circle.center_x;
    sum_y += other_circle.center_y;
    count += 1;
  }
  if (count == 0) {
    return SketchCentroid2D{.x = 0.0, .y = 0.0, .has_value = false};
  }
  return SketchCentroid2D{
      .x = sum_x / static_cast<double>(count),
      .y = sum_y / static_cast<double>(count),
      .has_value = true,
  };
}

ViewportSketchDimensionPrimitive make_line_dimension_primitive(
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  double normal_x = length > 0.0 ? -dy / length : 0.0;
  double normal_y = length > 0.0 ? dx / length : 1.0;

  // Flip the normal so it points *away* from the centroid of the rest
  // of the sketch. For a rectangle (or any closed profile) this puts
  // every edge's dimension line on the outside of the shape.
  const double midpoint_x = (line.start_x + line.end_x) / 2.0;
  const double midpoint_y = (line.start_y + line.end_y) / 2.0;
  const SketchCentroid2D centroid = compute_sketch_centroid_2d(parameters);
  const double to_outside_x = centroid.has_value ? midpoint_x - centroid.x : 0.0;
  const double to_outside_y = centroid.has_value ? midpoint_y - centroid.y : 0.0;
  const double to_outside_mag = std::sqrt(
      to_outside_x * to_outside_x + to_outside_y * to_outside_y);
  if (centroid.has_value) {
    if (normal_x * to_outside_x + normal_y * to_outside_y < 0.0) {
      normal_x = -normal_x;
      normal_y = -normal_y;
    }
  }

  // After the centroid flip, ensure the length dimension sits on the
  // opposite side of the line from the angle arc — but only as a
  // tiebreaker when the centroid is ambiguous (the line's midpoint
  // is near the centroid, i.e. isolated line or single entity).
  // When the centroid is well-defined (multi-entity sketch), let
  // the centroid-aware flip handle collision avoidance.
  // The angle arc sweeps from the +X reference (horizontal right)
  // to the line direction. For dy > 0 (up-right line), the arc is at
  // sin(θ) ≤ sin(line_angle) → arc is BELOW the line → normal already
  // above → no flip needed.
  // For dy < 0 (down-right line), the arc is at sin(θ) ≥ sin(line_angle)
  // → arc is ABOVE the line → normal also points above → flip to below.
  if (dy < 0.0 && to_outside_mag < 0.5) {
    normal_x = -normal_x;
    normal_y = -normal_y;
  }

  const WorldPoint anchor_start = to_world_point(parameters, line.start_x, line.start_y);
  const WorldPoint anchor_end = to_world_point(parameters, line.end_x, line.end_y);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      line.start_x + normal_x * kLineDimensionOffset,
      line.start_y + normal_y * kLineDimensionOffset);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      line.end_x + normal_x * kLineDimensionOffset,
      line.end_y + normal_y * kLineDimensionOffset);
  const WorldPoint label = to_world_point(
      parameters,
      midpoint_x + normal_x * (kLineDimensionOffset + 1.5),
      midpoint_y + normal_y * (kLineDimensionOffset + 1.5));

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_length",
      .entity_id = line.id,
      .label = format_dimension_value(dimension.value) + " mm",
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
      .driven = dimension.driven,
  };
}

ViewportSketchDimensionPrimitive make_line_angle_dimension_primitive(
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  // Render the line-angle dimension as an angle arc from the horizontal
  // reference (+X axis) to the line's direction, pivoted at the line's
  // start point. This makes it visually distinct from a line_length
  // dimension — the UI renders angle kinds as arcs with arrowheads.
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);

  // Reference ray: horizontal right (+X axis).
  const double ref_ux = 1.0;
  const double ref_uy = 0.0;

  // Line direction ray (from start to end).
  double line_ux = 1.0;
  double line_uy = 0.0;
  if (length > 0.0) {
    line_ux = dx / length;
    line_uy = dy / length;
  }

  const double pivot_x = line.start_x;
  const double pivot_y = line.start_y;

  // Bisector direction (sum of unit vectors).
  double bx = ref_ux + line_ux;
  double by = ref_uy + line_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) {
    bx = -ref_uy;
    by = ref_ux;
    blen = 1.0;
  }
  const double bisector_ux = bx / blen;
  const double bisector_uy = by / blen;

  const double kArcRadius = std::max(8.0, std::min(length, 500.0));
  constexpr double kAnchorRadius = 4.0;
  const double kLabelRadius = kArcRadius;

  const WorldPoint anchor_start = to_world_point(
      parameters,
      pivot_x + ref_ux * kArcRadius,
      pivot_y + ref_uy * kArcRadius);
  const WorldPoint anchor_end = to_world_point(
      parameters,
      pivot_x + line_ux * kAnchorRadius,
      pivot_y + line_uy * kAnchorRadius);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      pivot_x + ref_ux * kArcRadius,
      pivot_y + ref_uy * kArcRadius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      pivot_x + line_ux * kArcRadius,
      pivot_y + line_uy * kArcRadius);
  const WorldPoint label = to_world_point(
      parameters,
      pivot_x + bisector_ux * kLabelRadius,
      pivot_y + bisector_uy * kLabelRadius);

  // Compute arc sweep: from reference direction to line direction.
  // Use cross-product sign to determine CCW vs CW.
  const double sweep_cross = ref_ux * line_uy - ref_uy * line_ux;
  const bool arc_ccw = sweep_cross >= 0.0;
  const double arc_start_angle = std::atan2(ref_uy, ref_ux);
  const double arc_end_angle = std::atan2(line_uy, line_ux);

  // Compute reference line endpoints: from pivot along reference direction.
  const WorldPoint ref_line_start = to_world_point(parameters, pivot_x, pivot_y);
  const WorldPoint ref_line_end = to_world_point(
      parameters,
      pivot_x + ref_ux * kArcRadius,
      pivot_y + ref_uy * kArcRadius);

  // Arc center in world space (the pivot).
  const WorldPoint arc_center_world = to_world_point(parameters, pivot_x, pivot_y);

  const double degrees = dimension.value * 180.0 / 3.14159265358979323846;
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_angle",
      .entity_id = line.id,
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

      // Angle arc geometry
      .arc_center_x = arc_center_world.x,
      .arc_center_y = arc_center_world.y,
      .arc_center_z = arc_center_world.z,
      .arc_radius = kArcRadius,
      .arc_start_angle = arc_start_angle,
      .arc_end_angle = arc_end_angle,
      .arc_ccw = arc_ccw,

      // Reference line (along reference direction from pivot)
      .ref_line_start_x = ref_line_start.x,
      .ref_line_start_y = ref_line_start.y,
      .ref_line_start_z = ref_line_start.z,
      .ref_line_end_x = ref_line_end.x,
      .ref_line_end_y = ref_line_end.y,
      .ref_line_end_z = ref_line_end.z,
      .driven = dimension.driven,
  };
}

ViewportSketchDimensionPrimitive make_circle_dimension_primitive(
    const SketchCircle& circle,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      circle.center_x - circle.radius,
      circle.center_y);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      circle.center_x + circle.radius,
      circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      circle.center_x + circle.radius + kCircleDimensionOffset,
      circle.center_y);

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .label = "D " + format_dimension_value(dimension.value * 2.0) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = dimension_start.x,
      .anchor_start_y = dimension_start.y,
      .anchor_start_z = dimension_start.z,
      .anchor_end_x = dimension_end.x,
      .anchor_end_y = dimension_end.y,
      .anchor_end_z = dimension_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
      .driven = dimension.driven,
  };
}

// Build the viewport primitive for an angle dimension between two
// lines that share an endpoint. We emit a chord-shaped "dimension
// line" along the imaginary arc between the two outgoing rays so the
// existing UI dimension renderer can draw something sensible without
// arc support: anchor_start / anchor_end land on each line slightly
// past the pivot, dimension_start / dimension_end form a chord at a
// larger radius, and the label sits beyond that on the bisector.
ViewportSketchDimensionPrimitive make_angle_dimension_primitive(
    const SketchLine& line_a,
    const SketchLine& line_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  // Locate the shared endpoint or intersection point.  First try a
  // direct endpoint match; if that fails, check whether one line's
  // endpoint lies on the other line's segment.
  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  auto point_on_seg = [](double px, double py,
                         double ax, double ay,
                         double bx, double by,
                         double tol) -> bool {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12)
      return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay)) <= tol;
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double proj_x = ax + t * dx, proj_y = ay + t * dy;
    return std::sqrt((px - proj_x) * (px - proj_x) +
                     (py - proj_y) * (py - proj_y)) <= tol;
  };

  auto dir_away = [](double px, double py,
                     double e0x, double e0y,
                     double e1x, double e1y) -> std::pair<double, double> {
    double d0 = std::sqrt((e0x - px) * (e0x - px) + (e0y - py) * (e0y - py));
    double d1 = std::sqrt((e1x - px) * (e1x - px) + (e1y - py) * (e1y - py));
    double tx = d0 > d1 ? e0x : e1x, ty = d0 > d1 ? e0y : e1y;
    double dx = tx - px, dy = ty - py;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return {1.0, 0.0};
    return {dx / len, dy / len};
  };

  constexpr double kPivotTol = 0.05;
  int a_pivot = -1, b_pivot = -1;

  // Pass 1: direct endpoint–endpoint match.
  for (int i = 0; i < 2 && a_pivot < 0; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (std::abs(a_ends[i].first - b_ends[j].first) <= kPivotTol &&
          std::abs(a_ends[i].second - b_ends[j].second) <= kPivotTol) {
        a_pivot = i; b_pivot = j; break;
      }
    }
  }
  // Pass 2: endpoint-on-segment.
  if (a_pivot < 0) {
    for (int i = 0; i < 2 && a_pivot < 0; ++i) {
      if (point_on_seg(a_ends[i].first, a_ends[i].second,
                       b_ends[0].first, b_ends[0].second,
                       b_ends[1].first, b_ends[1].second, kPivotTol)) {
        a_pivot = i; b_pivot = -1; break;
      }
    }
  }
  if (a_pivot < 0) {
    for (int j = 0; j < 2 && a_pivot < 0; ++j) {
      if (point_on_seg(b_ends[j].first, b_ends[j].second,
                       a_ends[0].first, a_ends[0].second,
                       a_ends[1].first, a_ends[1].second, kPivotTol)) {
        a_pivot = -1; b_pivot = j; break;
      }
    }
  }

  double pivot_x, pivot_y;
  double a_dx, a_dy, b_dx, b_dy;

  if (a_pivot >= 0 && b_pivot >= 0) {
    pivot_x = a_ends[a_pivot].first;
    pivot_y = a_ends[a_pivot].second;
    a_dx = a_ends[1 - a_pivot].first - pivot_x;
    a_dy = a_ends[1 - a_pivot].second - pivot_y;
    b_dx = b_ends[1 - b_pivot].first - pivot_x;
    b_dy = b_ends[1 - b_pivot].second - pivot_y;
  } else if (a_pivot >= 0) {
    pivot_x = a_ends[a_pivot].first;
    pivot_y = a_ends[a_pivot].second;
    auto ad = dir_away(pivot_x, pivot_y,
                       a_ends[1 - a_pivot].first, a_ends[1 - a_pivot].second,
                       a_ends[a_pivot].first, a_ends[a_pivot].second);
    a_dx = ad.first; a_dy = ad.second;
    auto bd = dir_away(pivot_x, pivot_y,
                       b_ends[0].first, b_ends[0].second,
                       b_ends[1].first, b_ends[1].second);
    b_dx = bd.first; b_dy = bd.second;
  } else if (b_pivot >= 0) {
    pivot_x = b_ends[b_pivot].first;
    pivot_y = b_ends[b_pivot].second;
    auto ad = dir_away(pivot_x, pivot_y,
                       a_ends[0].first, a_ends[0].second,
                       a_ends[1].first, a_ends[1].second);
    a_dx = ad.first; a_dy = ad.second;
    auto bd = dir_away(pivot_x, pivot_y,
                       b_ends[1 - b_pivot].first, b_ends[1 - b_pivot].second,
                       b_ends[b_pivot].first, b_ends[b_pivot].second);
    b_dx = bd.first; b_dy = bd.second;
  } else {
    // Should not happen — add_sketch_angle_dimension validates first.
    pivot_x = a_ends[0].first;  pivot_y = a_ends[0].second;
    a_dx = 1.0; a_dy = 0.0;  b_dx = 0.0; b_dy = 1.0;
  }
  const double a_len = std::sqrt(a_dx * a_dx + a_dy * a_dy);
  const double b_len = std::sqrt(b_dx * b_dx + b_dy * b_dy);
  const double a_ux = a_len > 0.0 ? a_dx / a_len : 1.0;
  const double a_uy = a_len > 0.0 ? a_dy / a_len : 0.0;
  const double b_ux = b_len > 0.0 ? b_dx / b_len : 0.0;
  const double b_uy = b_len > 0.0 ? b_dy / b_len : 1.0;

  // Bisector direction (sum of unit vectors). Normalize defensively
  // because antiparallel rays sum to zero — fall back to A's normal.
  double bx = a_ux + b_ux;
  double by = a_uy + b_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) {
    bx = -a_uy;
    by = a_ux;
    blen = 1.0;
  }
  const double bisector_ux = bx / blen;
  const double bisector_uy = by / blen;

  const double kArcRadius = std::max(8.0, std::min(std::min(a_len, b_len), 500.0));
  constexpr double kAnchorRadius = 4.0;
  const double kLabelRadius = kArcRadius;

  const WorldPoint anchor_start = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);
  const WorldPoint anchor_end = to_world_point(
      parameters,
      pivot_x + b_ux * kAnchorRadius,
      pivot_y + b_uy * kAnchorRadius);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      pivot_x + b_ux * kArcRadius,
      pivot_y + b_uy * kArcRadius);
  const WorldPoint label = to_world_point(
      parameters,
      pivot_x + bisector_ux * kLabelRadius,
      pivot_y + bisector_uy * kLabelRadius);

  // Compute arc sweep: from line A direction to line B direction.
  const double sweep_cross = a_ux * b_uy - a_uy * b_ux;
  const bool arc_ccw = sweep_cross >= 0.0;
  const double arc_start_angle = std::atan2(a_uy, a_ux);
  const double arc_end_angle = std::atan2(b_uy, b_ux);

  // Reference line along line A direction from pivot
  const WorldPoint ref_line_start = to_world_point(parameters, pivot_x, pivot_y);
  const WorldPoint ref_line_end = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);

  // Arc center in world space (the pivot)
  const WorldPoint arc_center_world = to_world_point(parameters, pivot_x, pivot_y);

  // Render the value in degrees (CAD convention) with the same
  // formatter as length / radius so trailing-zero handling stays
  // consistent.
  const double degrees = dimension.value * 180.0 / 3.14159265358979323846;
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "angle",
      .entity_id = line_a.id,
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

      // Angle arc geometry
      .arc_center_x = arc_center_world.x,
      .arc_center_y = arc_center_world.y,
      .arc_center_z = arc_center_world.z,
      .arc_radius = kArcRadius,
      .arc_start_angle = arc_start_angle,
      .arc_end_angle = arc_end_angle,
      .arc_ccw = arc_ccw,

      // Reference line (along line A direction from pivot)
      .ref_line_start_x = ref_line_start.x,
      .ref_line_start_y = ref_line_start.y,
      .ref_line_start_z = ref_line_start.z,
    .ref_line_end_x = ref_line_end.x,
    .ref_line_end_y = ref_line_end.y,
    .ref_line_end_z = ref_line_end.z,
    .driven = dimension.driven,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double raw_dx = *dimension.label_x - pivot_x;
    const double raw_dy = *dimension.label_y - pivot_y;
    const double dimension_radius =
        std::max(6.0, std::min(std::sqrt(raw_dx * raw_dx + raw_dy * raw_dy), 500.0));
    const double label_radius = dimension_radius;
    const WorldPoint saved_anchor_start = to_world_point(
        parameters,
        pivot_x + a_ux * kAnchorRadius,
        pivot_y + a_uy * kAnchorRadius);
    const WorldPoint saved_anchor_end = to_world_point(
        parameters,
        pivot_x + b_ux * kAnchorRadius,
        pivot_y + b_uy * kAnchorRadius);
    const WorldPoint saved_dimension_start = to_world_point(
        parameters,
        pivot_x + a_ux * dimension_radius,
        pivot_y + a_uy * dimension_radius);
    const WorldPoint saved_dimension_end = to_world_point(
        parameters,
        pivot_x + b_ux * dimension_radius,
        pivot_y + b_uy * dimension_radius);
    const WorldPoint saved_label = to_world_point(
        parameters,
        pivot_x + bisector_ux * label_radius,
        pivot_y + bisector_uy * label_radius);
    primitive.anchor_start_x = saved_anchor_start.x;
    primitive.anchor_start_y = saved_anchor_start.y;
    primitive.anchor_start_z = saved_anchor_start.z;
    primitive.anchor_end_x = saved_anchor_end.x;
    primitive.anchor_end_y = saved_anchor_end.y;
    primitive.anchor_end_z = saved_anchor_end.z;
    primitive.dimension_start_x = saved_dimension_start.x;
    primitive.dimension_start_y = saved_dimension_start.y;
    primitive.dimension_start_z = saved_dimension_start.z;
    primitive.dimension_end_x = saved_dimension_end.x;
    primitive.dimension_end_y = saved_dimension_end.y;
    primitive.dimension_end_z = saved_dimension_end.z;
    primitive.label_x = saved_label.x;
    primitive.label_y = saved_label.y;
    primitive.label_z = saved_label.z;
    primitive.arc_radius = dimension_radius;
    primitive.ref_line_end_x = saved_dimension_start.x;
    primitive.ref_line_end_y = saved_dimension_start.y;
    primitive.ref_line_end_z = saved_dimension_start.z;
  }
  return primitive;
}

ViewportSketchDimensionPrimitive make_circle_center_distance_dimension_primitive(
    const SketchCircle& driven_circle,
    const SketchCircle& reference_circle,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint start =
      to_world_point(parameters, reference_circle.center_x, reference_circle.center_y);
  const WorldPoint end =
      to_world_point(parameters, driven_circle.center_x, driven_circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      (reference_circle.center_x + driven_circle.center_x) / 2.0,
      (reference_circle.center_y + driven_circle.center_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_center_distance",
      .entity_id = driven_circle.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double dx = driven_circle.center_x - reference_circle.center_x;
    const double dy = driven_circle.center_y - reference_circle.center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length > 1e-6) {
      const double normal_x = -dy / length;
      const double normal_y = dx / length;
      const double label_x =
          (reference_circle.center_x + driven_circle.center_x) / 2.0;
      const double label_y =
          (reference_circle.center_y + driven_circle.center_y) / 2.0;
      const double raw_offset_x = *dimension.label_x - label_x;
      const double raw_offset_y = *dimension.label_y - label_y;
      const double along = raw_offset_x * normal_x + raw_offset_y * normal_y;
      const double offset_x = normal_x * along;
      const double offset_y = normal_y * along;
      const WorldPoint shifted_start = to_world_point(
          parameters,
          reference_circle.center_x + offset_x,
          reference_circle.center_y + offset_y);
      const WorldPoint shifted_end = to_world_point(
          parameters,
          driven_circle.center_x + offset_x,
          driven_circle.center_y + offset_y);
      const WorldPoint shifted_label =
          to_world_point(parameters, label_x + offset_x, label_y + offset_y);
      primitive.dimension_start_x = shifted_start.x;
      primitive.dimension_start_y = shifted_start.y;
      primitive.dimension_start_z = shifted_start.z;
      primitive.dimension_end_x = shifted_end.x;
      primitive.dimension_end_y = shifted_end.y;
      primitive.dimension_end_z = shifted_end.z;
      primitive.label_x = shifted_label.x;
      primitive.label_y = shifted_label.y;
      primitive.label_z = shifted_label.z;
    }
  }
  primitive.driven = dimension.driven;
  return primitive;
}

ViewportSketchDimensionPrimitive make_circle_line_distance_dimension_primitive(
    const SketchCircle& circle,
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double ux = length > 0.0 ? dx / length : 1.0;
  const double uy = length > 0.0 ? dy / length : 0.0;
  const double t =
      ((circle.center_x - line.start_x) * ux +
       (circle.center_y - line.start_y) * uy);
  const double foot_x = line.start_x + ux * t;
  const double foot_y = line.start_y + uy * t;
  const WorldPoint start = to_world_point(parameters, foot_x, foot_y);
  const WorldPoint end = to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      (foot_x + circle.center_x) / 2.0,
      (foot_y + circle.center_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_line_distance",
      .entity_id = circle.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double label_x = (foot_x + circle.center_x) / 2.0;
    const double label_y = (foot_y + circle.center_y) / 2.0;
    const double raw_offset_x = *dimension.label_x - label_x;
    const double raw_offset_y = *dimension.label_y - label_y;
    const double along = raw_offset_x * ux + raw_offset_y * uy;
    const double offset_x = ux * along;
    const double offset_y = uy * along;
    const WorldPoint shifted_start =
        to_world_point(parameters, foot_x + offset_x, foot_y + offset_y);
    const WorldPoint shifted_end = to_world_point(
        parameters,
        circle.center_x + offset_x,
        circle.center_y + offset_y);
    const WorldPoint shifted_label =
        to_world_point(parameters, label_x + offset_x, label_y + offset_y);
    primitive.dimension_start_x = shifted_start.x;
    primitive.dimension_start_y = shifted_start.y;
    primitive.dimension_start_z = shifted_start.z;
    primitive.dimension_end_x = shifted_end.x;
    primitive.dimension_end_y = shifted_end.y;
    primitive.dimension_end_z = shifted_end.z;
    primitive.label_x = shifted_label.x;
    primitive.label_y = shifted_label.y;
    primitive.label_z = shifted_label.z;
  }
  primitive.driven = dimension.driven;
  return primitive;
}

ViewportSketchDimensionPrimitive make_line_line_distance_dimension_primitive(
    const SketchLine& driven_line,
    const SketchLine& reference_line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double ref_dx = reference_line.end_x - reference_line.start_x;
  const double ref_dy = reference_line.end_y - reference_line.start_y;
  const double ref_length = std::sqrt(ref_dx * ref_dx + ref_dy * ref_dy);
  if (ref_length <= 1e-6) {
    return ViewportSketchDimensionPrimitive{};
  }
  const double ux = ref_dx / ref_length;
  const double uy = ref_dy / ref_length;
  const double midpoint_x = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double midpoint_y = (driven_line.start_y + driven_line.end_y) / 2.0;
  const double t =
      (midpoint_x - reference_line.start_x) * ux +
      (midpoint_y - reference_line.start_y) * uy;
  const double foot_x = reference_line.start_x + ux * t;
  const double foot_y = reference_line.start_y + uy * t;
  const WorldPoint start = to_world_point(parameters, foot_x, foot_y);
  const WorldPoint end = to_world_point(parameters, midpoint_x, midpoint_y);
  const WorldPoint label = to_world_point(
      parameters,
      (foot_x + midpoint_x) / 2.0,
      (foot_y + midpoint_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_line_distance",
      .entity_id = driven_line.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double label_x = (foot_x + midpoint_x) / 2.0;
    const double label_y = (foot_y + midpoint_y) / 2.0;
    const double raw_offset_x = *dimension.label_x - label_x;
    const double raw_offset_y = *dimension.label_y - label_y;
    const double along = raw_offset_x * ux + raw_offset_y * uy;
    const double offset_x = ux * along;
    const double offset_y = uy * along;
    const WorldPoint shifted_start =
        to_world_point(parameters, foot_x + offset_x, foot_y + offset_y);
    const WorldPoint shifted_end =
        to_world_point(parameters, midpoint_x + offset_x, midpoint_y + offset_y);
    const WorldPoint shifted_label =
        to_world_point(parameters, label_x + offset_x, label_y + offset_y);
    primitive.dimension_start_x = shifted_start.x;
    primitive.dimension_start_y = shifted_start.y;
    primitive.dimension_start_z = shifted_start.z;
    primitive.dimension_end_x = shifted_end.x;
    primitive.dimension_end_y = shifted_end.y;
    primitive.dimension_end_z = shifted_end.z;
    primitive.label_x = shifted_label.x;
    primitive.label_y = shifted_label.y;
    primitive.label_z = shifted_label.z;
  }
  primitive.driven = dimension.driven;
  return primitive;
}

ViewportSketchDimensionPrimitive make_point_distance_dimension_primitive(
    const SketchPoint& point_a,
    const SketchPoint& point_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint start = to_world_point(parameters, point_a.x, point_a.y);
  const WorldPoint end = to_world_point(parameters, point_b.x, point_b.y);
  const WorldPoint label = to_world_point(
      parameters,
      (point_a.x + point_b.x) / 2.0,
      (point_a.y + point_b.y) / 2.0);
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "point_distance",
      .entity_id = point_a.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
      .driven = dimension.driven,
  };
}

ViewportSketchConstraintPrimitive make_line_constraint_primitive(
    const SketchLine& line,
    const std::string& plane_id,
    const std::string& kind,
    const std::string& label,
    bool is_selected,
    const std::optional<std::string>& related_entity_id = std::nullopt,
    double badge_offset_multiplier = 1.0) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double normal_x = length > 0.0 ? -dy / length : 0.0;
  const double normal_y = length > 0.0 ? dx / length : 1.0;
  const double offset = kConstraintBadgeOffset * badge_offset_multiplier;
  const WorldPoint position = to_world_point(
      plane_id,
      (line.start_x + line.end_x) / 2.0 + normal_x * offset,
      (line.start_y + line.end_y) / 2.0 + normal_y * offset);

  return ViewportSketchConstraintPrimitive{
      .constraint_id =
          (kind == "equal_length" || kind == "perpendicular") &&
                  related_entity_id.has_value()
              ? "constraint-" + kind + "-" + line.id + "-" + related_entity_id.value()
              : "constraint-" + kind + "-" + line.id,
      .plane_id = plane_id,
      .kind = kind,
      .entity_id = line.id,
      .related_entity_id = related_entity_id,
      .label = label,
      .is_selected = is_selected,
      .position_x = position.x,
      .position_y = position.y,
      .position_z = position.z,
  };
}

ViewportSketchConstraintPrimitive make_point_constraint_primitive(
    const SketchPoint& point,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint position = to_world_point(
      parameters, point.x, point.y, kSketchPlaneOffset + kConstraintBadgeOffset);

  return ViewportSketchConstraintPrimitive{
      .constraint_id = "constraint-fixed-" + point.id,
      .plane_id = parameters.plane_id,
      .kind = "fixed",
      .entity_id = point.id,
      .related_entity_id = std::nullopt,
      .label = "FIX",
      .is_selected = is_selected,
      .position_x = position.x,
      .position_y = position.y,
      .position_z = position.z,
  };
}

ViewportSketchProfilePrimitive make_rectangle_profile_primitive(
    const PolygonSketchProfile& profile, bool is_selected) {
  return ViewportSketchProfilePrimitive{
      .profile_id = profile.id,
      .plane_id = profile.plane_id,
      .plane_frame = profile.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(profile.plane_frame.value()))
                         : std::nullopt,
      .profile_kind = "polygon",
      .profile_points = profile.points,
      .inner_loops = profile.inner_loops,
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .is_selected = is_selected,
  };
}

ViewportSketchProfilePrimitive make_circle_profile_primitive(
    const CircleSketchProfile& profile, bool is_selected) {
  return ViewportSketchProfilePrimitive{
      .profile_id = profile.id,
      .plane_id = profile.plane_id,
      .plane_frame = profile.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(profile.plane_frame.value()))
                         : std::nullopt,
      .profile_kind = "circle",
      .profile_points = {},
      .inner_loops = {},
      .start_x = profile.center_x,
      .start_y = profile.center_y,
      .width = 0.0,
      .height = 0.0,
      .radius = profile.radius,
      .is_selected = is_selected,
  };
}
