// Build a single OCCT line edge from a sketch line in the given frame.
TopoDS_Edge make_line_edge(const SketchLine& line,
                           const PlaneFrame& frame) {
  const gp_Pnt p1 = to_world_point(frame, line.start_x, line.start_y);
  const gp_Pnt p2 = to_world_point(frame, line.end_x, line.end_y);
  GC_MakeSegment seg(p1, p2);
  if (!seg.IsDone()) {
    throw std::runtime_error("Failed to create line segment");
  }
  return BRepBuilderAPI_MakeEdge(seg.Value());
}

// Build a single OCCT arc edge from a sketch arc.
TopoDS_Edge make_arc_edge(const SketchArc& arc,
                          const PlaneFrame& frame) {
  const gp_Pnt center = to_world_point(frame, arc.center_x, arc.center_y);
  const gp_Pnt start  = to_world_point(frame, arc.start_x, arc.start_y);
  const gp_Pnt end    = to_world_point(frame, arc.end_x, arc.end_y);
  const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
  const gp_Circ circle(gp_Ax2(center, normal), arc.radius);
  // The OCCT circle built with gp_Ax2(center, normal) has its parameter
  // CCW opposite to the sketch CCW (Y_direction = (0,0,-1) vs sketch +Z
  // for ref-plane-xy).  The caller already oriented start/end to match the
  // wire traversal direction; negating ccw picks the correct minor arc.
  GC_MakeArcOfCircle arc_maker(circle, start, end, !arc.ccw);
  if (!arc_maker.IsDone()) {
    throw std::runtime_error("Failed to create arc of circle");
  }
  return BRepBuilderAPI_MakeEdge(arc_maker.Value());
}

// Build a circle-arc edge spanning two points on a sketch circle.
// `p1` and `p2` are the profile boundary points in sketch-local
// coordinates; the circle centre and radius come from the sketch data.
// `hint_point` (optional) is an interior sample point that lies on the
// correct arc; when provided it disambiguates the arc sense by checking
// which direction puts the arc through the hint point.
TopoDS_Edge make_circle_arc_edge(const SketchCircle& circle,
                                  const SketchProfilePoint& p1,
                                  const SketchProfilePoint& p2,
                                  const PlaneFrame& frame,
                                  const SketchProfilePoint* hint_point = nullptr) {
  const gp_Pnt center = to_world_point(frame, circle.center_x, circle.center_y);
  const gp_Pnt start  = to_world_point(frame, p1.x, p1.y);
  const gp_Pnt end    = to_world_point(frame, p2.x, p2.y);

  // Skip degenerate segments (start == end within tolerance).
  if (std::abs(p1.x - p2.x) < 1e-9 && std::abs(p1.y - p2.y) < 1e-9) {
    return TopoDS_Edge{};
  }

  const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
  const gp_Circ circ(gp_Ax2(center, normal), circle.radius);

  // When a hint point is available, determine the correct arc sense
  // by checking which arc's midpoint is closer to the hint point.
  if (hint_point != nullptr) {
    const gp_Pnt hint = to_world_point(frame, hint_point->x, hint_point->y);
    double best_dist = std::numeric_limits<double>::max();
    TopoDS_Edge best_edge;

    for (bool ccw : {false, true}) {
      GC_MakeArcOfCircle arc_maker(circ, start, end, ccw);
      if (!arc_maker.IsDone()) continue;
      const Handle(Geom_TrimmedCurve)& curve = arc_maker.Value();
      double mid_param = (curve->FirstParameter() + curve->LastParameter()) * 0.5;
      gp_Pnt mid;
      curve->D0(mid_param, mid);
      const double dist = mid.SquareDistance(hint);
      if (dist < best_dist) {
        best_dist = dist;
        best_edge = BRepBuilderAPI_MakeEdge(curve);
      }
    }
    if (!best_edge.IsNull()) return best_edge;
  }

  // Fallback: try both arc senses, preferring the shorter (CW) arc.
  for (bool ccw : {false, true}) {
    GC_MakeArcOfCircle arc_maker(circ, start, end, ccw);
    if (arc_maker.IsDone()) {
      return BRepBuilderAPI_MakeEdge(arc_maker.Value());
    }
  }
  return TopoDS_Edge{};
}

// Build a closed OCCT wire from the profile boundary points and ordered
// edge IDs. Consecutive segments sharing the same edge ID are merged into
// a single curve span so that a tessellated arc or circle produces one
// exact OCCT edge rather than dozens of tiny arc segments.
TopoDS_Wire make_sketch_wire(
    const ExtrudeFeatureParameters& parameters) {
  PlaneFrame frame;
  if (parameters.plane_frame.has_value()) {
    frame = parameters.plane_frame.value();
  } else if (parameters.plane_id == "ref-plane-xy") {
    frame = PlaneFrame{.origin_x=0,.origin_y=0,.origin_z=0,
                       .x_axis_x=1,.x_axis_y=0,.x_axis_z=0,
                       .y_axis_x=0,.y_axis_y=1,.y_axis_z=0,
                       .normal_x=0,.normal_y=0,.normal_z=1};
  } else if (parameters.plane_id == "ref-plane-yz") {
    frame = PlaneFrame{.origin_x=0,.origin_y=0,.origin_z=0,
                       .x_axis_x=0,.x_axis_y=1,.x_axis_z=0,
                       .y_axis_x=0,.y_axis_y=0,.y_axis_z=1,
                       .normal_x=1,.normal_y=0,.normal_z=0};
  } else {
    frame = PlaneFrame{.origin_x=0,.origin_y=0,.origin_z=0,
                       .x_axis_x=1,.x_axis_y=0,.x_axis_z=0,
                       .y_axis_x=0,.y_axis_y=0,.y_axis_z=1,
                       .normal_x=0,.normal_y=1,.normal_z=0};
  }

  const auto& pts = parameters.profile_points;
  const auto& ids = parameters.sketch_edge_ids;

  if (pts.size() < 2 || ids.size() != pts.size()) {
    throw std::runtime_error("Sketch wire requires matching points and edge IDs");
  }

  BRepBuilderAPI_MakeWire wire_builder;

  // Group consecutive segments by edge ID and create one edge per group.
  size_t group_start = 0;
  while (group_start < pts.size()) {
    // Find the span of consecutive segments with the same edge ID.
    size_t group_end = group_start;
    while (group_end + 1 < pts.size() &&
           ids[group_end + 1] == ids[group_start]) {
      ++group_end;
    }

    // The group spans from pts[group_start] to pts[(group_end+1) % N],
    // connected by edge ids[group_start].
    const size_t last_pt = (group_end + 1) % pts.size();
    const auto& edge_id = ids[group_start];

    // --- Try circle lookup ---
    auto circle_it = std::find_if(
        parameters.sketch_circles.begin(), parameters.sketch_circles.end(),
        [&](const SketchCircle& c) { return c.id == edge_id; });
    if (circle_it != parameters.sketch_circles.end()) {
      // When the entire circle is tessellated (all edge IDs are the same
      // circle ID), build a full-circle edge so the face is made from a
      // single circular edge rather than a polygon of tiny arcs.
      if (group_start == 0 && group_end + 1 == pts.size()) {
        const gp_Pnt center = to_world_point(frame,
            circle_it->center_x, circle_it->center_y);
        const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
        const gp_Circ circ(gp_Ax2(center, normal), circle_it->radius);
        wire_builder.Add(BRepBuilderAPI_MakeEdge(circ));
      } else {
        const SketchProfilePoint* hint = nullptr;
        if (group_end > group_start + 1) {
          hint = &pts[(group_start + group_end) / 2];
        }
        const TopoDS_Edge edge = make_circle_arc_edge(
            *circle_it, pts[group_start], pts[last_pt], frame, hint);
        if (!edge.IsNull()) {
          wire_builder.Add(edge);
        }
      }
      group_start = group_end + 1;
      continue;
    }

    // --- Try arc lookup ---
    auto arc_it = std::find_if(
        parameters.sketch_arcs.begin(), parameters.sketch_arcs.end(),
        [&](const SketchArc& a) { return a.id == edge_id; });
    if (arc_it != parameters.sketch_arcs.end()) {
      // The stored arc direction (start→end) may not match the wire
      // traversal direction (group_start→last_pt).  When it doesn't,
      // swap endpoints and invert ccw so the edge goes the right way
      // and OCCT never has to reverse it internally — reversed edges
      // can confuse BRepBuilderAPI_MakeFace's interior determination.
      const auto& gp_start = pts[group_start];
      const auto& gp_end   = pts[last_pt];
      constexpr double kTol = 0.01;
      const bool stored_matches =
          std::abs(gp_start.x - arc_it->start_x) <= kTol &&
          std::abs(gp_start.y - arc_it->start_y) <= kTol;
      if (stored_matches) {
        wire_builder.Add(make_arc_edge(*arc_it, frame));
      } else {
        SketchArc oriented = *arc_it;
        oriented.start_x = gp_start.x;
        oriented.start_y = gp_start.y;
        oriented.end_x   = gp_end.x;
        oriented.end_y   = gp_end.y;
        oriented.ccw     = !oriented.ccw;
        wire_builder.Add(make_arc_edge(oriented, frame));
      }
      group_start = group_end + 1;
      continue;
    }

    // --- Try line lookup ---
    auto line_it = std::find_if(
        parameters.sketch_lines.begin(), parameters.sketch_lines.end(),
        [&](const SketchLine& l) { return l.id == edge_id; });
    if (line_it != parameters.sketch_lines.end()) {
      const auto& gp_start = pts[group_start];
      const auto& gp_end   = pts[last_pt];
      constexpr double kTol = 0.01;
      const bool stored_matches =
          std::abs(gp_start.x - line_it->start_x) <= kTol &&
          std::abs(gp_start.y - line_it->start_y) <= kTol;
      if (stored_matches) {
        wire_builder.Add(make_line_edge(*line_it, frame));
      } else {
        SketchLine oriented = *line_it;
        oriented.start_x = gp_start.x;
        oriented.start_y = gp_start.y;
        oriented.end_x   = gp_end.x;
        oriented.end_y   = gp_end.y;
        wire_builder.Add(make_line_edge(oriented, frame));
      }
      group_start = group_end + 1;
      continue;
    }

    // Unknown edge — skip to next group rather than failing, so that
    // sketch profiles with multi-entity boundaries degrade gracefully
    // for edges whose type we can't look up.
    group_start = group_end + 1;
  }

  if (!wire_builder.IsDone()) {
    throw std::runtime_error("Failed to build sketch wire");
  }
  return wire_builder.Wire();
}

TopoDS_Shape make_wire_extrude_shape(
    const ExtrudeFeatureParameters& parameters) {
  const TopoDS_Wire wire = make_sketch_wire(parameters);

  BRepBuilderAPI_MakeFace face_maker(wire);
  if (!face_maker.IsDone()) {
    throw std::runtime_error("Failed to build face from sketch wire");
  }

  // Inner loops (holes).  Use make_profile_wire which auto-detects
  // circular loops and builds a proper gp_Circ wire — avoiding the
  // faceted look of a polygon-approximated hole.
  for (const auto& loop : parameters.inner_loops) {
    if (loop.size() < 3) continue;
    try {
      TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
      hole_wire.Reverse();
      face_maker.Add(hole_wire);
    } catch (const std::exception&) {
      // Skip this hole — a bad inner loop shouldn't kill the whole face.
    }
  }

  const gp_Vec direction =
      extrusion_vector(parameters, parameters.depth);
  const TopoDS_Shape prism = BRepPrimAPI_MakePrism(
      face_maker.Face(), direction).Shape();
  if (prism.IsNull()) {
    throw std::runtime_error("Failed to extrude sketch wire face");
  }
  return prism;
}
