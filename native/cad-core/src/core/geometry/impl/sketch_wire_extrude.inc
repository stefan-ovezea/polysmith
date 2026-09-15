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
  gp_Pnt start = to_world_point(frame, arc.start_x, arc.start_y);
  gp_Pnt end   = to_world_point(frame, arc.end_x, arc.end_y);
  const gp_Pnt center = to_world_point(frame, arc.center_x, arc.center_y);
  const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
  const gp_Ax2 ax2(center, normal);
  const gp_Circ circle(ax2, arc.radius);

  // OCCT's gp_Ax2 picks its own local X/Y axes (Y = N × X), so the local
  // parameter angle is NOT the sketch angle.  GC_MakeArcOfCircle walks
  // from the first point to the second in the LOCAL orientation and the
  // geometric span is fixed by the endpoint order — the sense flag only
  // shifts the parameter range, it does not pick the other arc.  Measure
  // the local parameter span and swap the endpoints when it is the major
  // arc so the edge always follows the minor (fillet) arc.
  const gp_Dir local_x = ax2.XDirection();
  const gp_Dir local_y = ax2.YDirection();
  constexpr double kPi = 3.14159265358979323846;
  auto local_angle = [&](const gp_Pnt& p) {
    const gp_Vec v(center, p);
    return std::atan2(v.Dot(local_y), v.Dot(local_x));
  };
  double span = local_angle(end) - local_angle(start);
  while (span <= 0.0) span += 2.0 * kPi;
  while (span > 2.0 * kPi) span -= 2.0 * kPi;
  if (span > kPi) {
    std::swap(start, end);
  }

  GC_MakeArcOfCircle arc_maker(circle, start, end, true);
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
  // GC_MakeArcOfCircle with Sense=false does NOT produce the complementary
  // arc — it merely shifts the CCW arc's parameter range by 2π.  To get
  // the OTHER arc (the short one when we have the long one, or vice versa),
  // swap start and end and build a CCW arc, which IS the complement.
  if (hint_point != nullptr) {
    const gp_Pnt hint = to_world_point(frame, hint_point->x, hint_point->y);
    double best_dist = std::numeric_limits<double>::max();
    TopoDS_Edge best_edge;
    int best_variant = -1;
    double best_mid_x = 0, best_mid_y = 0;

    // Variant 0: CCW arc from start to end (what we have now).
    // Variant 1: CCW arc from end to start (the complementary arc).
    for (int variant = 0; variant < 2; ++variant) {
      gp_Pnt arc_start = (variant == 0) ? start : end;
      gp_Pnt arc_end   = (variant == 0) ? end   : start;
      GC_MakeArcOfCircle arc_maker(circ, arc_start, arc_end, true);
      if (!arc_maker.IsDone()) continue;
      const Handle(Geom_TrimmedCurve)& curve = arc_maker.Value();
      double mid_param = (curve->FirstParameter() + curve->LastParameter()) * 0.5;
      gp_Pnt mid;
      curve->D0(mid_param, mid);
      const double dist = mid.SquareDistance(hint);
      if (dist < best_dist) {
        best_dist = dist;
        best_edge = BRepBuilderAPI_MakeEdge(curve);
        best_variant = variant;
        best_mid_x = mid.X(); best_mid_y = mid.Y();
      }
    }
    // Diagnostic: log arc parameters when hint is available.
    {
      constexpr double rad2deg = 180.0 / 3.14159265358979323846;
      double a1 = std::atan2(p1.y - circle.center_y, p1.x - circle.center_x) * rad2deg;
      double a2 = std::atan2(p2.y - circle.center_y, p2.x - circle.center_x) * rad2deg;
      polysmith::core::log_debug("circle_arc",
          "arc from " + std::to_string(a1) + " deg to " + std::to_string(a2) +
          " deg  hint=(" + std::to_string(hint_point->x) + "," + std::to_string(hint_point->y) +
          ")  picked=" + (best_variant == 0 ? "fwd" : "rev") +
          " mid=(" + std::to_string(best_mid_x) + "," + std::to_string(best_mid_y) +
          ")  circle r=" + std::to_string(circle.radius));
    }
    if (!best_edge.IsNull()) {
      // Variant 1 builds the complement arc end→start — orient it back
      // to the walk direction (same wire-chain requirement as
      // make_wire_from_boundary_edges).
      if (best_variant == 1) best_edge.Reverse();
      return best_edge;
    }
  }

  // Fallback: try both directions (start→end and end→start), preferring
  // the shorter arc.
  for (int variant = 0; variant < 2; ++variant) {
    gp_Pnt arc_start = (variant == 0) ? start : end;
    gp_Pnt arc_end   = (variant == 0) ? end   : start;
    GC_MakeArcOfCircle arc_maker(circ, arc_start, arc_end, true);
    if (arc_maker.IsDone()) {
      return BRepBuilderAPI_MakeEdge(arc_maker.Value());
    }
  }
  return TopoDS_Edge{};
}

// Project a point onto a circle's perimeter.  Returns the nearest point
// on the circle to `pt`, at distance `radius` from (`center_x`, `center_y`).
SketchProfilePoint project_onto_circle(const SketchProfilePoint& pt,
                                       double center_x, double center_y,
                                       double radius) {
  const double dx = pt.x - center_x;
  const double dy = pt.y - center_y;
  const double dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 1e-12) {
    // Point is at the centre — pick an arbitrary direction.
    return {center_x + radius, center_y};
  }
  const double scale = radius / dist;
  return {center_x + dx * scale, center_y + dy * scale};
}

// Remove consecutive duplicate points (and the closing duplicate if present).
// A closed loop should end with a point distinct from the first; the final
// closing edge is implicit via BRepBuilderAPI_MakeWire.
std::vector<SketchProfilePoint> deduplicate_adjacent_points(
    const std::vector<SketchProfilePoint>& pts) {
  constexpr double kTol = 1e-6;
  std::vector<SketchProfilePoint> clean;
  clean.reserve(pts.size());
  for (const auto& p : pts) {
    if (clean.empty()) {
      clean.push_back(p);
      continue;
    }
    const double dx = p.x - clean.back().x;
    const double dy = p.y - clean.back().y;
    if (dx * dx + dy * dy > kTol * kTol) {
      clean.push_back(p);
    }
  }
  // Remove duplicate closing point.
  if (clean.size() > 1) {
    const double dx = clean.front().x - clean.back().x;
    const double dy = clean.front().y - clean.back().y;
    if (dx * dx + dy * dy <= kTol * kTol) {
      clean.pop_back();
    }
  }
  return clean;
}

// Return the SketchCircle that matches `edge_id`, or nullptr.
const SketchCircle* find_circle_for_edge(
    const std::vector<SketchCircle>& circles, const std::string& edge_id) {
  auto it = std::find_if(circles.begin(), circles.end(),
      [&](const SketchCircle& c) { return c.id == edge_id; });
  return it != circles.end() ? &(*it) : nullptr;
}

// Exact wire from a boundary-edge list: one OCCT edge per boundary
// edge, anchored to the entity's exact parametric geometry.  Used for
// both the outer boundary and the per-hole inner loops.  Circular
// pieces are built via GC_MakeArcOfCircle with the sense disambiguated
// by comparing the arc midpoint against the expected midpoint in the
// walk's parametric direction (the local parameter direction of gp_Ax2
// may be mirrored relative to the sketch frame).  Full circles
// (span ≈ 2π) become a single gp_Circ edge.
TopoDS_Wire make_wire_from_boundary_edges(
    const std::vector<ProfileBoundaryEdge>& edges, const PlaneFrame& frame) {
  constexpr double kExactPi = 3.14159265358979323846;
  std::vector<TopoDS_Edge> edge_list;
  for (const auto& be : edges) {
    if (be.entity_kind == "line") {
      const gp_Pnt p1 = to_world_point(frame, be.start_x, be.start_y);
      const gp_Pnt p2 = to_world_point(frame, be.end_x, be.end_y);
      GC_MakeSegment seg(p1, p2);
      if (!seg.IsDone()) {
        throw std::runtime_error("Failed to create exact line edge");
      }
      edge_list.push_back(BRepBuilderAPI_MakeEdge(seg.Value()));
      continue;
    }

    if (be.entity_kind == "ellipse") {
      const gp_Pnt center = to_world_point(frame, be.center_x, be.center_y);
      const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
      // Major axis direction in the sketch plane, rotated into world
      // space by the sketch frame.
      const gp_Pnt a_point = to_world_point(
          frame, be.center_x + be.radius * std::cos(be.rotation),
          be.center_y + be.radius * std::sin(be.rotation));
      // Orthogonalize the major-axis direction against the plane
      // normal — gp_Ax2 requires an exactly perpendicular XDir and
      // throws ConstructionError on any numerical residual.
      gp_Vec vx(center, a_point);
      const double d = vx.Dot(normal);
      vx.Subtract(normal.XYZ().Multiplied(d));
      const gp_Dir x_dir(vx);
      // Ellipse edge — trimmed to the walked sub-span when it is a
      // partial ellipse (a trim result); a full ellipse stays one
      // analytic edge. Geom_Ellipse's parameter IS the sketch's
      // ellipse-frame angle, so the trim is exact.
      GC_MakeEllipse ell(gp_Ax2(center, normal, x_dir), be.radius,
                         be.minor_radius);
      if (!ell.IsDone()) {
        throw std::runtime_error("Failed to create exact ellipse edge");
      }
      const double span = std::abs(be.param_end - be.param_start);
      if (span < 2.0 * M_PI - 1e-9) {
        edge_list.push_back(BRepBuilderAPI_MakeEdge(
            new Geom_TrimmedCurve(ell.Value(), be.param_start, be.param_end)));
      } else {
        edge_list.push_back(BRepBuilderAPI_MakeEdge(ell.Value()));
      }
      continue;
    }

    if (be.entity_kind == "spline") {
      // Exact clamped B-spline edge from the boundary's cached poles —
      // the same open-uniform convention the profile walk evaluates
      // (spline_math.h), so the extruded edge IS the drawn curve.
      const int n = static_cast<int>(be.spline_pole_xs.size());
      if (n < 2) {
        throw std::runtime_error("Spline boundary edge has too few poles");
      }
      int degree = std::min(be.spline_degree, n - 1);
      degree = std::max(1, degree);
      NCollection_Array1<gp_Pnt> poles(1, n);
      for (int i = 0; i < n; ++i) {
        poles.SetValue(i + 1, to_world_point(frame, be.spline_pole_xs[i],
                                             be.spline_pole_ys[i]));
      }
      // OCCT takes DISTINCT knot values with per-value multiplicities
      // (the expanded sequence the de Boor evaluator uses would be
      // misread as a near-zero interval).
      const int interior = n - degree - 1;
      const int knot_count = interior + 2;
      NCollection_Array1<double> knots(1, knot_count);
      NCollection_Array1<int> mults(1, knot_count);
      knots.SetValue(1, 0.0);
      mults.SetValue(1, degree + 1);
      for (int i = 1; i <= interior; ++i) {
        knots.SetValue(i + 1, static_cast<double>(i) /
                                  static_cast<double>(interior + 1));
        mults.SetValue(i + 1, 1);
      }
      knots.SetValue(knot_count, 1.0);
      mults.SetValue(knot_count, degree + 1);
      Handle(Geom_Curve) piece =
          new Geom_BSplineCurve(poles, knots, mults, degree);
      // A split spline contributes only its walked sub-span.
      if (std::abs(be.param_start) > 1e-12 ||
          std::abs(be.param_end - 1.0) > 1e-12) {
        piece = new Geom_TrimmedCurve(piece, be.param_start, be.param_end);
      }
      edge_list.push_back(BRepBuilderAPI_MakeEdge(piece));
      continue;
    }

    const gp_Pnt center = to_world_point(frame, be.center_x, be.center_y);
    const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
    const gp_Circ circ(gp_Ax2(center, normal), be.radius);

    // Full circle: one analytic edge.
    if (std::abs(std::abs(be.param_end - be.param_start) - 2.0 * kExactPi) <
        1e-9) {
      edge_list.push_back(BRepBuilderAPI_MakeEdge(circ));
      continue;
    }

    const gp_Pnt p1 = to_world_point(frame, be.start_x, be.start_y);
    const gp_Pnt p2 = to_world_point(frame, be.end_x, be.end_y);

    // Expected midpoint in the walk's parametric direction (sketch
    // space → world).  Disambiguates the arc sense without relying on
    // the local parameter convention of gp_Ax2.
    const double mid_angle = 0.5 * (be.param_start + be.param_end);
    const gp_Pnt mid_expected = to_world_point(
        frame, be.center_x + be.radius * std::cos(mid_angle),
        be.center_y + be.radius * std::sin(mid_angle));

    TopoDS_Edge edge;
    bool made = false;
    for (const bool swap : {false, true}) {
      GC_MakeArcOfCircle arc_maker(circ, swap ? p2 : p1, swap ? p1 : p2,
                                   true);
      if (!arc_maker.IsDone()) continue;
      const Handle(Geom_TrimmedCurve)& curve = arc_maker.Value();
      const double mid_param =
          (curve->FirstParameter() + curve->LastParameter()) * 0.5;
      gp_Pnt mid_actual;
      curve->D0(mid_param, mid_actual);
      // The walk's stored angles and the arc-maker's projected
      // parameters differ by ~1e-4 rad (trim-engine error on the
      // nodes), so the two midpoints can disagree by ~1e-3 mm on
      // typical radii.  The wrong (major/complementary) arc is tens of
      // mm off, so 0.5 mm separates them by two orders of magnitude.
      constexpr double kMidTol = 0.5;  // mm (endpoints are exact)
      if (mid_actual.SquareDistance(mid_expected) <= kMidTol * kMidTol) {
        edge = BRepBuilderAPI_MakeEdge(curve);
        // The swapped variant built the arc end→start (GC_MakeArcOfCircle
        // only sweeps the circle's positive sense, so a CW walk needs
        // swapped endpoints).  Orient the edge back to the walk
        // direction — otherwise its end vertex is the walk's start and
        // the wire chain breaks ("Failed to build exact sketch wire").
        if (swap) edge.Reverse();
        made = true;
        break;
      }
    }
    if (!made) {
      // Degenerate span fallback — build whatever arc we can so the
      // wire still closes.
      GC_MakeArcOfCircle arc_maker(circ, p1, p2, true);
      if (!arc_maker.IsDone()) {
        throw std::runtime_error("Failed to create exact arc edge");
      }
      edge = BRepBuilderAPI_MakeEdge(arc_maker.Value());
    }
    edge_list.push_back(edge);
  }

  // Watertight joints: the walk's shared nodes carry the trim-engine
  // tolerance (~1e-2 mm) off the analytic curves, and building each
  // arc edge through its own circle projects a shared joint onto two
  // slightly different points — enough to make BRepBuilderAPI_MakeWire
  // refuse the wire ("Failed to build exact sketch wire").  Snap every
  // vertex back to the walk's shared node and widen its tolerance to
  // cover the projection split — the same healing the legacy polygon
  // path does by projecting endpoints onto adjacent circles.
  if (edge_list.size() > 1) {
    BRep_Builder snap_builder;
    for (size_t i = 0; i < edge_list.size(); ++i) {
      const auto& be = edges[i];
      TopoDS_Edge& cur = edge_list[i];
      // Snap by POSITION: an edge's walk-start vertex is the one lying
      // at the walk's start node, regardless of the edge's orientation
      // (reversed arcs keep their built vertex positions, and
      // TopExp::FirstVertex does not account for orientation in this
      // OCCT build — snapping "first/last" would drag a reversed
      // edge's vertices across the loop).
      for (const bool first : {true, false}) {
        const gp_Pnt target = to_world_point(
            frame, first ? be.start_x : be.end_x,
            first ? be.start_y : be.end_y);
        TopoDS_Vertex v;
        double best = 1e18;
        for (TopExp_Explorer vex(cur, TopAbs_VERTEX); vex.More();
             vex.Next()) {
          const TopoDS_Vertex& cand = TopoDS::Vertex(vex.Current());
          const double d = BRep_Tool::Pnt(cand).Distance(target);
          if (d < best) {
            best = d;
            v = cand;
          }
        }
        if (best > 1e-7) {
          snap_builder.UpdateVertex(v, target, best + 1e-4);
          // The face-level curve-on-surface check measures the
          // snapped vertex against the edge's curve with the EDGE's
          // tolerance, which MakeEdge left at ~1e-7 — far below the
          // snap distance.  Widen it to match the vertex, or
          // BRepCheck marks every face with a snapped vertex invalid
          // (free edges / missing surfaces after extrusion).
          const double edge_tol = BRep_Tool::Tolerance(cur);
          snap_builder.UpdateEdge(cur, std::max(edge_tol, best + 1e-4));
        }
      }
    }
  }

  BRepBuilderAPI_MakeWire wire_builder;
  for (const auto& edge : edge_list) {
    wire_builder.Add(edge);
  }
  if (!wire_builder.IsDone()) {
    throw std::runtime_error("Failed to build exact sketch wire");
  }
  return wire_builder.Wire();
}

// Sketch plane frame for a set of extrude parameters — the captured
// plane_frame wins, then the named construction planes, then XY.
// Shared by the outer wire and the exact hole wires.
PlaneFrame resolve_sketch_plane_frame(
    const ExtrudeFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    return parameters.plane_frame.value();
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return PlaneFrame{.origin_x=0,.origin_y=0,.origin_z=0,
                      .x_axis_x=1,.x_axis_y=0,.x_axis_z=0,
                      .y_axis_x=0,.y_axis_y=1,.y_axis_z=0,
                      .normal_x=0,.normal_y=0,.normal_z=1};
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return PlaneFrame{.origin_x=0,.origin_y=0,.origin_z=0,
                      .x_axis_x=0,.x_axis_y=1,.x_axis_z=0,
                      .y_axis_x=0,.y_axis_y=0,.y_axis_z=1,
                      .normal_x=1,.normal_y=0,.normal_z=0};
  }
  return PlaneFrame{.origin_x=0,.origin_y=0,.origin_z=0,
                    .x_axis_x=1,.x_axis_y=0,.x_axis_z=0,
                    .y_axis_x=0,.y_axis_y=0,.y_axis_z=1,
                    .normal_x=0,.normal_y=1,.normal_z=0};
}

// Build a closed OCCT wire from the profile boundary points and ordered
// edge IDs. Consecutive segments sharing the same edge ID are merged into
// a single curve span so that a tessellated arc or circle produces one
// exact OCCT edge rather than dozens of tiny arc segments.
//
// Line-arc joints are made watertight by projecting the shared endpoint
// onto the adjacent circle — this eliminates the floating-point gaps that
// arise when line endpoints come from polygon samples that are slightly
// off the analytic circle.
//
// Legacy path: used when the profile carries no boundary_edges (old
// saved documents).  New profiles always carry boundary_edges and use
// make_wire_from_boundary_edges instead.
TopoDS_Wire make_sketch_wire(
    const ExtrudeFeatureParameters& parameters) {
  const PlaneFrame frame = resolve_sketch_plane_frame(parameters);

  // Exact-curve path: profiles built by the exact arrangement detector
  // carry boundary_edges — one exact OCCT edge per boundary edge.
  if (!parameters.boundary_edges.empty()) {
    return make_wire_from_boundary_edges(parameters.boundary_edges, frame);
  }

  const auto raw_pts = deduplicate_adjacent_points(parameters.profile_points);
  const auto& ids = parameters.sketch_edge_ids;

  // Full circle: when the profile is a single circle edge but the point
  // list was sampled from it (N points, 1 edge ID), build the analytic
  // circle directly rather than requiring pts.size() == ids.size().
  if (ids.size() == 1) {
    const SketchCircle* circle = find_circle_for_edge(
        parameters.sketch_circles, ids[0]);
    if (circle != nullptr && raw_pts.size() >= 3) {
      const gp_Pnt center = to_world_point(frame,
          circle->center_x, circle->center_y);
      const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
      const gp_Circ circ(gp_Ax2(center, normal), circle->radius);
      BRepBuilderAPI_MakeWire wb;
      wb.Add(BRepBuilderAPI_MakeEdge(circ));
      if (wb.IsDone()) return wb.Wire();
    }
  }

  if (raw_pts.size() < 2 || ids.size() != raw_pts.size()) {
    throw std::runtime_error("Sketch wire requires matching points and edge IDs");
  }

  const auto& pts = raw_pts;
  const size_t N = pts.size();

  // Pre-compute which groups are circle arcs so adjacent line edges can
  // project their endpoints onto the circle for watertight joints.
  auto group_is_circle = [&](size_t start, size_t end) -> const SketchCircle* {
    const auto& eid = ids[start];
    // All entries in the group have the same ID (verified by the grouping
    // loop), so checking the first is enough.
    return find_circle_for_edge(parameters.sketch_circles, eid);
  };

  BRepBuilderAPI_MakeWire wire_builder;
  std::set<std::string> emitted_edge_ids;

  // Group consecutive segments by edge ID and create one edge per group.
  // A single entity can appear in multiple groups when its chain was
  // split by a touching line endpoint (the walk lists each chain piece);
  // emitting it once per piece would duplicate the edge in the wire.
  size_t group_start = 0;
  while (group_start < N) {
    size_t group_end = group_start;
    while (group_end + 1 < N && ids[group_end + 1] == ids[group_start]) {
      ++group_end;
    }

    const size_t last_pt = (group_end + 1) % N;
    const auto& edge_id = ids[group_start];

    if (emitted_edge_ids.contains(edge_id)) {
      group_start = group_end + 1;
      continue;
    }
    emitted_edge_ids.insert(edge_id);

    // Resolve the start and end points, projecting onto adjacent circles
    // so that line-arc joints are watertight.
    SketchProfilePoint seg_start = pts[group_start];
    SketchProfilePoint seg_end   = pts[last_pt];

    // If the previous group is a circle arc, project the start point
    // onto that circle so the line starts exactly where the arc ends.
    if (group_start > 0) {
      size_t prev_end = group_start - 1;
      const SketchCircle* prev_circle = group_is_circle(
          prev_end, prev_end);
      // Walk back to find the start of the previous group.
      size_t prev_start = prev_end;
      while (prev_start > 0 && ids[prev_start - 1] == ids[prev_end]) {
        --prev_start;
      }
      if (prev_circle != nullptr && prev_end > prev_start) {
        // Previous group is a multi-segment arc — project onto its circle.
        seg_start = project_onto_circle(seg_start,
            prev_circle->center_x, prev_circle->center_y, prev_circle->radius);
      }
    }

    // If the next group is a circle arc, project the end point onto
    // that circle.
    {
      size_t next_start = (group_end + 1) % N;
      if (next_start != 0 || group_end + 1 < N) {
        const SketchCircle* next_circle = group_is_circle(next_start, next_start);
        if (next_circle != nullptr) {
          // Check it's a multi-segment arc group (not just a single point).
          size_t next_end = next_start;
          while (next_end + 1 < N && ids[next_end + 1] == ids[next_start]) {
            ++next_end;
          }
          if (next_end > next_start) {
            seg_end = project_onto_circle(seg_end,
                next_circle->center_x, next_circle->center_y,
                next_circle->radius);
          }
        }
      }
    }

    // --- Try circle lookup ---
    auto circle_it = std::find_if(
        parameters.sketch_circles.begin(), parameters.sketch_circles.end(),
        [&](const SketchCircle& c) { return c.id == edge_id; });
    if (circle_it != parameters.sketch_circles.end()) {
      if (group_start == 0 && group_end + 1 == N) {
        // Single group spanning the entire boundary → full circle.
        const gp_Pnt center = to_world_point(frame,
            circle_it->center_x, circle_it->center_y);
        const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
        const gp_Circ circ(gp_Ax2(center, normal), circle_it->radius);
        wire_builder.Add(BRepBuilderAPI_MakeEdge(circ));
      } else if (group_end > group_start) {
        // Try exact arc descriptor first (emitted by the profile
        // builder).  When available it tells us the exact angle range
        // and direction — no guessing, no hint-based disambiguation.
        const SketchArcDescriptor* desc = nullptr;
        for (const auto& d : parameters.arc_descriptors) {
          if (d.circle_id == edge_id &&
              std::abs(d.start_angle_rad -
                  std::atan2(seg_start.y - d.center_y,
                             seg_start.x - d.center_x)) < 1e-6 &&
              std::abs(d.end_angle_rad -
                  std::atan2(seg_end.y - d.center_y,
                             seg_end.x - d.center_x)) < 1e-6) {
            desc = &d;
            break;
          }
        }
        if (desc != nullptr) {
          // Build the exact arc from the descriptor — no hint needed.
          const gp_Pnt center = to_world_point(frame,
              desc->center_x, desc->center_y);
          const gp_Dir normal(frame.normal_x, frame.normal_y, frame.normal_z);
          const gp_Circ circ(gp_Ax2(center, normal), desc->radius);
          GC_MakeArcOfCircle arc_maker(
              circ,
              to_world_point(frame, seg_start.x, seg_start.y),
              to_world_point(frame, seg_end.x, seg_end.y),
              true);
          if (arc_maker.IsDone()) {
            wire_builder.Add(BRepBuilderAPI_MakeEdge(arc_maker.Value()));
          }
        } else {
          // Legacy path: no descriptor — fall back to hint-based guessing.
          const SketchProfilePoint* hint = nullptr;
          if (group_end > group_start + 1) {
            hint = &pts[(group_start + group_end) / 2];
          }
          const TopoDS_Edge edge = make_circle_arc_edge(
              *circle_it, seg_start, seg_end, frame, hint);
          if (!edge.IsNull()) {
            wire_builder.Add(edge);
          }
        }
      }
      group_start = group_end + 1;
      continue;
    }

    // --- Try arc lookup ---
    //
    // Anchor the edge to the STORED entity geometry rather than the walk
    // sample points: the walk only supplies the edge order.  A reversed
    // walk (loop normalized to CCW by detect_edge_loop) would otherwise
    // anchor the arc to an interior sample and produce a sliver edge,
    // breaking wire closure.
    auto arc_it = std::find_if(
        parameters.sketch_arcs.begin(), parameters.sketch_arcs.end(),
        [&](const SketchArc& a) { return a.id == edge_id; });
    if (arc_it != parameters.sketch_arcs.end()) {
      wire_builder.Add(make_arc_edge(*arc_it, frame));
      group_start = group_end + 1;
      continue;
    }

    // --- Try line lookup ---
    auto line_it = std::find_if(
        parameters.sketch_lines.begin(), parameters.sketch_lines.end(),
        [&](const SketchLine& l) { return l.id == edge_id; });
    if (line_it != parameters.sketch_lines.end()) {
      wire_builder.Add(make_line_edge(*line_it, frame));
      group_start = group_end + 1;
      continue;
    }

    // Unknown edge — skip to next group rather than failing.
    group_start = group_end + 1;
  }

  if (!wire_builder.IsDone()) {
    throw std::runtime_error("Failed to build sketch wire");
  }
  return wire_builder.Wire();
}

// Approximate the signed area of a planar wire by summing the chord
// polygons of its edges, walked in the wire's edge order.  Positive =
// CCW in the world XY sense.  Used to orient hole wires opposite to the
// outer boundary — BRepBuilderAPI_MakeFace treats a wire with the SAME
// orientation as the outer boundary as an island, not a hole.
double signed_wire_area(const TopoDS_Wire& wire) {
  constexpr int kSamplesPerEdge = 24;
  double area = 0.0;
  gp_Pnt previous;
  gp_Pnt first_point;
  bool has_previous = false;
  bool has_first = false;
  for (TopExp_Explorer ex(wire, TopAbs_EDGE); ex.More(); ex.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(ex.Current());
    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    for (int i = 0; i <= kSamplesPerEdge; ++i) {
      const double t = first + (last - first) *
                                  static_cast<double>(i) / kSamplesPerEdge;
      gp_Pnt p;
      curve.D0(t, p);
      if (!has_first) {
        first_point = p;
        has_first = true;
      }
      if (has_previous) {
        area += previous.X() * p.Y() - p.X() * previous.Y();
      }
      previous = p;
      has_previous = true;
    }
  }
  // Close the loop: the wire is a closed boundary, so the last sampled
  // point connects back to the first.
  if (has_previous && has_first) {
    area += previous.X() * first_point.Y() - first_point.X() * previous.Y();
  }
  return 0.5 * area;
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
  // faceted look of a polygon-approximated hole.  A hole wire must be
  // oriented OPPOSITE to the FACE's actual normal (a same-orientation
  // wire becomes an island).  The face's normal depends on how the
  // outer wire was built (a single gp_Circ edge produces +Z while the
  // multi-edge line/arc wire produces -Z), so measure the face normal
  // and compare the hole's world signed area against it.  Note that
  // TopoDS_Shape::Reverse() mutates the shape in place in this OCCT
  // build.
  double face_normal_z = 0.0;
  {
    const TopoDS_Face diag_face = face_maker.Face();
    if (!diag_face.IsNull()) {
      const Handle(Geom_Surface) surf = BRep_Tool::Surface(diag_face);
      if (!surf.IsNull() && surf->IsKind(STANDARD_TYPE(Geom_Plane))) {
        const Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surf);
        gp_Dir n = plane->Axis().Direction();
        if (diag_face.Orientation() == TopAbs_REVERSED) n.Reverse();
        face_normal_z = n.Z();
      }
    }
  }
  const PlaneFrame frame = resolve_sketch_plane_frame(parameters);
  for (size_t hole_index = 0; hole_index < parameters.inner_loops.size();
       ++hole_index) {
    const auto& loop = parameters.inner_loops[hole_index];
    if (loop.size() < 3) continue;
    try {
      TopoDS_Wire hole_wire;
      // Exact per-hole edges when the detector emitted them (new
      // profiles) — holes become true analytic wires instead of
      // polygon-approximated cutouts.  Fall back to the sampled-loop
      // path (auto circle detection / polygon) for legacy profiles.
      if (hole_index < parameters.inner_loop_edges.size() &&
          !parameters.inner_loop_edges[hole_index].empty()) {
        hole_wire = make_wire_from_boundary_edges(
            parameters.inner_loop_edges[hole_index], frame);
      } else {
        hole_wire = make_profile_wire(parameters, loop);
      }
      // Orientation decision from the WALK DATA's chord shoelace —
      // signed_wire_area samples through the OCCT adaptor, whose
      // parameterization flips on reversed arc edges and misreports
      // the sign (a CW hole reads CCW, the wire gets reversed into an
      // island and the face turns invalid).  A full-circle hole has
      // start==end and zero chord area — fall back to the sampler
      // there (one analytic edge, no reversals).
      double hole_orientation_area = 0.0;
      if (hole_index < parameters.inner_loop_edges.size() &&
          !parameters.inner_loop_edges[hole_index].empty()) {
        for (const auto& e : parameters.inner_loop_edges[hole_index]) {
          hole_orientation_area += e.start_x * e.end_y - e.end_x * e.start_y;
        }
        hole_orientation_area *= 0.5;
      }
      if (std::abs(hole_orientation_area) < 1e-12) {
        hole_orientation_area = signed_wire_area(hole_wire);
      }
      if (hole_orientation_area * face_normal_z > 0.0) {
        hole_wire.Reverse();
      }
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
