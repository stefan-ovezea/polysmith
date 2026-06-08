// Generic edge wrapper used by the loop detector so a single graph
// algorithm walks both lines and arcs uniformly. `kind == Arc` means
// the cached arc params (center / radius / ccw) are populated; lines
// leave them at zero.
struct ProfileEdge {
  enum class Kind { Line, Arc };

  std::string id;
  Kind kind;
  std::string start_point_id;
  std::string end_point_id;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  // Arc-only fields (zeroed for lines).
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  bool ccw = false;
};

ProfileEdge profile_edge_from_line(const SketchLine& line) {
  return ProfileEdge{
      .id = line.id,
      .kind = ProfileEdge::Kind::Line,
      .start_point_id = line.start_point_id,
      .end_point_id = line.end_point_id,
      .start_x = line.start_x,
      .start_y = line.start_y,
      .end_x = line.end_x,
      .end_y = line.end_y,
  };
}

ProfileEdge profile_edge_from_arc(const SketchArc& arc) {
  return ProfileEdge{
      .id = arc.id,
      .kind = ProfileEdge::Kind::Arc,
      .start_point_id = arc.start_point_id,
      .end_point_id = arc.end_point_id,
      .start_x = arc.start_x,
      .start_y = arc.start_y,
      .end_x = arc.end_x,
      .end_y = arc.end_y,
      .center_x = arc.center_x,
      .center_y = arc.center_y,
      .radius = arc.radius,
      .ccw = arc.ccw,
  };
}

std::string edge_node_id(const ProfileEdge& edge, bool is_start) {
  const std::string& point_id =
      is_start ? edge.start_point_id : edge.end_point_id;
  if (!point_id.empty()) {
    return point_id;
  }
  return make_node_key({
      .x = is_start ? edge.start_x : edge.end_x,
      .y = is_start ? edge.start_y : edge.end_y,
  });
}

// Sample interior points of an arc walked from `from` to `to`,
// excluding both endpoints. The result is the (kArcSampleSegments-1)
// intermediate samples in walk order, ready to slot between the
// endpoint vertices in the polygon point list. We parameterize on
// signed sweep angle: the arc's stored ccw direction tells us which
// way start→end goes; if the loop walk crosses the arc end-to-start
// instead, we just reverse `forward` so the sweep direction matches.
std::vector<SketchProfilePoint> sample_arc_interior(
    const ProfileEdge& edge, bool from_start) {
  std::vector<SketchProfilePoint> samples;
  if (edge.kind != ProfileEdge::Kind::Arc) {
    return samples;
  }

  const double start_angle =
      std::atan2(edge.start_y - edge.center_y, edge.start_x - edge.center_x);
  const double end_angle =
      std::atan2(edge.end_y - edge.center_y, edge.end_x - edge.center_x);

  // Compute the signed sweep from start_angle to end_angle along
  // the stored ccw direction. We normalize into (0, 2π) so a full-
  // circle sweep doesn't collapse to zero.
  double sweep = end_angle - start_angle;
  if (edge.ccw) {
    while (sweep <= 0.0) {
      sweep += 2.0 * kPi;
    }
  } else {
    while (sweep >= 0.0) {
      sweep -= 2.0 * kPi;
    }
  }

  // If we're walking the arc in reverse (loop entered at end_point
  // and exits at start_point) we still sample the same geometric
  // points but in reverse order. Compute samples forward, reverse
  // at the end if needed.
  for (int i = 1; i < kArcSampleSegments; ++i) {
    const double t = static_cast<double>(i) / kArcSampleSegments;
    const double angle = start_angle + sweep * t;
    samples.push_back(SketchProfilePoint{
        .x = edge.center_x + edge.radius * std::cos(angle),
        .y = edge.center_y + edge.radius * std::sin(angle),
    });
  }

  if (!from_start) {
    std::reverse(samples.begin(), samples.end());
  }
  return samples;
}
