// cam2d test — the shared 2D CAM math module (cam2d + cam_planning).
//
// Offset joins (round + miter), collinear miter propagation, loop
// connectivity, sampling sagitta, segment clipping, loop containment,
// area centroid, and chord-tolerance wire sampling over OCCT wires
// (reversed edges, circles, splines).

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_planning.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Curve.hxx>
#include <NCollection_Array1.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace {

using polysmith::core::cam2d::BaseSegment;
using polysmith::core::cam2d::OffsetSegment;
using polysmith::core::cam2d::XY;
using polysmith::core::cam2d::base_segments_signed_area;
using polysmith::core::cam2d::clip_segment_to_polygon;
using polysmith::core::cam2d::loop_contains;
using polysmith::core::cam2d::offset_closed_loop;
using polysmith::core::cam2d::offset_loop_self_intersects;
using polysmith::core::cam2d::sample_offset_loop;
using polysmith::core::cam2d::xy_area_centroid;
using polysmith::core::cam2d::xy_centroid;
using polysmith::core::cam2d::xy_length;
using polysmith::core::cam2d::xy_signed_area;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) < tolerance;
}

// CCW square [-1,1]².
std::vector<BaseSegment> make_square() {
  const XY pts[] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < 4; ++i) {
    BaseSegment segment;
    segment.start = pts[i];
    segment.end = pts[(i + 1) % 4];
    base.push_back(segment);
  }
  return base;
}

// Every segment must end exactly where the next begins.
bool loop_is_connected(const std::vector<OffsetSegment>& segments) {
  for (size_t i = 0; i < segments.size(); ++i) {
    const auto& a = segments[i];
    const auto& b = segments[(i + 1) % segments.size()];
    if (xy_length(a.end.x - b.start.x, a.end.y - b.start.y) > 1e-9) {
      return false;
    }
  }
  return true;
}

// ── Test 1: round joins on a square ──────────────────────────────

bool test_round_join_square() {
  const auto base = make_square();
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(base, 1.0, out, /*round_joins=*/true),
              "round: offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "round: loop connected")) {
    return false;
  }
  int joins = 0;
  int lines = 0;
  for (const auto& segment : out) {
    if (segment.is_arc) {
      if (!expect(segment.is_join && near(segment.radius, 1.0),
                  "round: join arcs have radius d")) {
        return false;
      }
      ++joins;
    } else {
      ++lines;
    }
  }
  return expect(joins == 4 && lines == 4, "round: 4 lines + 4 join arcs");
}

// ── Test 2: miter mode on a square ───────────────────────────────

bool test_miter_square() {
  const auto base = make_square();
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(base, 1.0, out, /*round_joins=*/false),
              "miter: offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "miter: loop connected")) {
    return false;
  }
  for (const auto& segment : out) {
    if (segment.is_arc) {
      return expect(false, "miter: no arcs");
    }
  }
  // The miter points sit at ±2: the offset boundary spans [-2, 2]².
  double minX = 1e9;
  double maxX = -1e9;
  double minY = 1e9;
  double maxY = -1e9;
  for (const auto& segment : out) {
    minX = std::min({minX, segment.start.x, segment.end.x});
    maxX = std::max({maxX, segment.start.x, segment.end.x});
    minY = std::min({minY, segment.start.y, segment.end.y});
    maxY = std::max({maxY, segment.start.y, segment.end.y});
  }
  return expect(minX <= -2.0 + 1e-9 && maxX >= 2.0 - 1e-9 &&
                    minY <= -2.0 + 1e-9 && maxY >= 2.0 - 1e-9,
                "miter: corners reach ±2");
}

// ── Test 3: collinear miter propagation + wrap-around ────────────
//
// The right edge is sampled into three collinear pieces; their offset
// lines are parallel, so the corner miters must PROPAGATE into them.

bool test_miter_collinear_propagation() {
  const XY pts[] = {{-1, -1}, {1, -1}, {1, 0}, {1, 1}, {-1, 1}};
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < 5; ++i) {
    BaseSegment segment;
    segment.start = pts[i];
    segment.end = pts[(i + 1) % 5];
    base.push_back(segment);
  }
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(base, 1.0, out, /*round_joins=*/false),
              "propagation: offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "propagation: loop connected")) {
    return false;
  }
  for (const auto& segment : out) {
    if (segment.is_arc) {
      return expect(false, "propagation: no arcs");
    }
  }
  double minX = 1e9;
  double maxX = -1e9;
  double minY = 1e9;
  double maxY = -1e9;
  for (const auto& segment : out) {
    minX = std::min({minX, segment.start.x, segment.end.x});
    maxX = std::max({maxX, segment.start.x, segment.end.x});
    minY = std::min({minY, segment.start.y, segment.end.y});
    maxY = std::max({maxY, segment.start.y, segment.end.y});
  }
  return expect(minX <= -2.0 + 1e-9 && maxX >= 2.0 - 1e-9 &&
                    minY <= -2.0 + 1e-9 && maxY >= 2.0 - 1e-9,
                "propagation: propagated miters reach ±2");
}

// ── Test 4: round mode miter-trims shallow corners ───────────────

bool test_round_mode_shallow_miter() {
  // A square with a shallow dent in the bottom edge.  The dent corner
  // at (5, 0) is a shallow REFLEX corner (~5.7° right turn): the two
  // offset lines cross INSIDE both truncated segments (the miter
  // point IS the true boundary), so round mode miter-trims there.
  // The neighbouring shallow CONVEX corner at (5.1, -0.01) crosses
  // past the segment ends, so it still gets a join arc — convex
  // corners never miter.
  const XY pts[] = {{0, 0}, {5, 0}, {5.1, -0.01}, {10, 0}, {10, 10}, {0, 10}};
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < 6; ++i) {
    BaseSegment segment;
    segment.start = pts[i];
    segment.end = pts[(i + 1) % 6];
    base.push_back(segment);
  }
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(base, 0.5, out, /*round_joins=*/true),
              "dent: offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "dent: loop connected")) {
    return false;
  }
  int joins = 0;
  int lines = 0;
  for (const auto& segment : out) {
    if (segment.is_arc && segment.is_join) {
      ++joins;
    } else {
      ++lines;
    }
  }
  return expect(joins == 5 && lines == 6,
                "dent: 5 join arcs, the reflex dent corner miter-trimmed");
}

// ── Test 5: sampling sagitta bound ───────────────────────────────

bool test_sampling_sagitta() {
  BaseSegment circle;
  circle.is_arc = true;
  circle.center = {0.0, 0.0};
  circle.radius = 10.0;
  circle.start = {10.0, 0.0};
  circle.end = {10.0, 0.0};
  circle.ccw = true;
  std::vector<BaseSegment> base{circle};
  std::vector<OffsetSegment> offset;
  if (!expect(offset_closed_loop(base, 1.0, offset), "sagitta: offset ok")) {
    return false;
  }
  const auto samples = sample_offset_loop(offset, /*tolerance=*/0.05);
  if (!expect(samples.size() >= 20, "sagitta: circle sampled densely")) {
    return false;
  }
  for (const auto& p : samples) {
    if (!near(std::hypot(p.x, p.y), 11.0, 0.05 + 1e-9)) {
      std::cerr << "  sampled point at radius " << std::hypot(p.x, p.y)
                << "\n";
      return expect(false, "sagitta: every sample within tolerance");
    }
  }
  return true;
}

// ── Test 6: segment clipping ─────────────────────────────────────

bool test_clip_segment() {
  const std::vector<XY> square{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  // Consumers (milling rows) read clipped.front()/back() as row
  // start/end, so the points must come back in p1→p2 order without
  // duplicated intersections.
  const auto inside =
      clip_segment_to_polygon({-2, 0}, {2, 0}, square);
  if (!expect(inside.size() == 2, "clip: crossing segment gives 2 points")) {
    std::cerr << "  got " << inside.size() << " points\n";
    return false;
  }
  if (!expect(near(inside[0].x, -1.0, 1e-9) && near(inside[0].y, 0.0, 1e-9),
              "clip: entry point is the left intersection")) {
    std::cerr << "  entry (" << inside[0].x << ", " << inside[0].y << ")\n";
    return false;
  }
  if (!expect(near(inside[1].x, 1.0, 1e-9) && near(inside[1].y, 0.0, 1e-9),
              "clip: exit point is the right intersection")) {
    std::cerr << "  exit (" << inside[1].x << ", " << inside[1].y << ")\n";
    return false;
  }
  const auto outside =
      clip_segment_to_polygon({5, 0}, {6, 0}, square);
  return expect(outside.empty(), "clip: outside segment dropped");
}

// ── Test 7: loop containment ─────────────────────────────────────

std::vector<XY> square_points(double half) {
  return {{-half, -half}, {half, -half}, {half, half}, {-half, half}};
}

bool test_loop_contains() {
  const auto outer = square_points(2.0);
  if (!expect(loop_contains(outer, square_points(1.0)),
              "contains: nested inner")) {
    return false;
  }
  if (!expect(loop_contains(outer, square_points(0.5)),
              "contains: smaller nested inner")) {
    return false;
  }
  // Shifted inner, disjoint from the outer.
  std::vector<XY> shifted{{4, -1}, {6, -1}, {6, 1}, {4, 1}};
  if (!expect(!loop_contains(outer, shifted),
              "contains: disjoint inner rejected")) {
    return false;
  }
  // Nested-in-nested still resolves against the outermost.
  if (!expect(loop_contains(square_points(4.0), square_points(0.5)),
              "contains: nested-in-nested inner")) {
    return false;
  }
  // Inner centroid OUTSIDE the outer (vertices straddle the edge) is
  // rejected — ray casting on the centroid, not the vertices.
  std::vector<XY> straddling{{2, -1}, {2, 1}, {2.3, 0}};
  return expect(!loop_contains(outer, straddling),
                "contains: centroid-outside rejected");
}

// ── Test 8: area centroid vs vertex centroid ─────────────────────

bool test_area_centroid() {
  const std::vector<XY> quad{{0, 0}, {4, 0}, {4, 1}, {0, 2}};
  const XY area = xy_area_centroid(quad);
  const XY vertex = xy_centroid(quad);
  if (!expect(near(area.x, 16.0 / 9.0, 1e-9) && near(area.y, 7.0 / 9.0, 1e-9),
              "area centroid: shoelace value")) {
    std::cerr << "  area centroid (" << area.x << ", " << area.y << ")\n";
    return false;
  }
  return expect(!near(area.x, vertex.x, 1e-3) || !near(area.y, vertex.y, 1e-3),
                "area centroid: differs from the vertex mean");
}

// ── Test 9: chord-tolerance wire sampling (OCCT) ─────────────────

TopoDS_Wire make_quad_wire(bool reverse_second_edge) {
  gp_Pnt p0(0, 0, 0);
  gp_Pnt p1(2, 0, 0);
  gp_Pnt p2(2, 2, 0);
  gp_Pnt p3(0, 2, 0);
  TopoDS_Edge e01 = BRepBuilderAPI_MakeEdge(p0, p1);
  TopoDS_Edge e12 = BRepBuilderAPI_MakeEdge(p1, p2);
  TopoDS_Edge e23 = BRepBuilderAPI_MakeEdge(p2, p3);
  TopoDS_Edge e30 = BRepBuilderAPI_MakeEdge(p3, p0);
  BRepBuilderAPI_MakeWire builder;
  builder.Add(e01);
  // Wire edge orientations vary in real geometry — feed one edge
  // reversed to exercise the match-and-flip chain.
  builder.Add(reverse_second_edge ? TopoDS::Edge(e12.Reversed()) : e12);
  builder.Add(e23);
  builder.Add(e30);
  return builder.Wire();
}

TopoDS_Wire make_circle_wire(double radius) {
  gp_Circ circ(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), radius);
  const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
  BRepBuilderAPI_MakeWire builder(edge);
  return builder.Wire();
}

TopoDS_Wire make_spline_quad_wire() {
  gp_Pnt p0(0, 0, 0);
  gp_Pnt p1(3, 0, 0);
  gp_Pnt p2(3, 2, 0);
  gp_Pnt p3(0, 2, 0);
  BRepBuilderAPI_MakeWire builder;
  builder.Add(BRepBuilderAPI_MakeEdge(p0, p1));
  builder.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  builder.Add(BRepBuilderAPI_MakeEdge(p2, p3));
  // Closing spline edge from (0,2,0) back to (0,0,0).
  NCollection_Array1<gp_Pnt> poles(1, 4);
  poles.SetValue(1, p3);
  poles.SetValue(2, gp_Pnt(-0.5, 1.5, 0));
  poles.SetValue(3, gp_Pnt(0.5, 0.5, 0));
  poles.SetValue(4, p0);
  NCollection_Array1<double> knots(1, 2);
  knots.SetValue(1, 0.0);
  knots.SetValue(2, 1.0);
  NCollection_Array1<int> mults(1, 2);
  mults.SetValue(1, 4);
  mults.SetValue(2, 4);
  Handle(Geom_BSplineCurve) spline =
      new Geom_BSplineCurve(poles, knots, mults, /*degree=*/3);
  const TopoDS_Edge splineEdge =
      BRepBuilderAPI_MakeEdge(Handle(Geom_Curve)(spline));
  builder.Add(splineEdge);
  return builder.Wire();
}

bool test_wire_sampling() {
  // Reversed-edge square wire: still chains into a closed 2×2 loop.
  {
    std::vector<XY> loop;
    if (!expect(polysmith::core::cam_planning::sample_planar_wire(
                    make_quad_wire(/*reverse_second_edge=*/true),
                    /*chord_tolerance=*/0.05, loop),
                "wire: reversed-edge quad samples")) {
      return false;
    }
    if (!expect(loop.size() == 4,
                "wire: line edges sample to their 4 vertices")) {
      return false;
    }
    if (!expect(near(std::abs(xy_signed_area(loop)), 4.0, 1e-9),
                "wire: quad area is 2×2")) {
      return false;
    }
  }
  // Circle wire: sagitta bound holds on every sample.
  {
    std::vector<XY> loop;
    if (!expect(polysmith::core::cam_planning::sample_planar_wire(
                    make_circle_wire(10.0), /*chord_tolerance=*/0.05, loop),
                "wire: circle samples")) {
      return false;
    }
    if (!expect(loop.size() >= 20, "wire: circle sampled densely")) {
      return false;
    }
    for (const auto& p : loop) {
      if (!near(std::hypot(p.x, p.y), 10.0, 0.05 + 1e-9)) {
        std::cerr << "  wire sample at radius " << std::hypot(p.x, p.y)
                  << "\n";
        return expect(false, "wire: circle sagitta within tolerance");
      }
    }
  }
  // Spline edge: sampled within tolerance instead of a fixed 30.
  {
    std::vector<XY> loop;
    if (!expect(polysmith::core::cam_planning::sample_planar_wire(
                    make_spline_quad_wire(), /*chord_tolerance=*/0.05, loop),
                "wire: spline quad samples")) {
      return false;
    }
    if (!expect(loop.size() >= 4, "wire: spline quad has 4+ points")) {
      return false;
    }
    if (!expect(xy_signed_area(loop) > 5.0,
                "wire: spline quad area close to 3×2")) {
      return false;
    }
  }
  return true;
}

// ── Test 10: self-intersection scan ──────────────────────────────

bool test_self_intersection_scan() {
  // Bow-tie polygon: the scan must catch the crossing.
  const std::vector<XY> bowtie{{0, 0}, {2, 2}, {2, 0}, {0, 2}};
  if (!expect(offset_loop_self_intersects(bowtie),
              "scan: bow-tie detected")) {
    return false;
  }
  const std::vector<XY> square{{0, 0}, {2, 0}, {2, 2}, {0, 2}};
  return expect(!offset_loop_self_intersects(square),
                "scan: plain square clean");
}

// ── Test 11: CW clip = subtraction ───────────────────────────────
//
// The pocket generator subtracts avoidance regions by clipping rows
// against CW-walked polygons: for a CW loop the "left of each edge"
// test keeps the EXTERIOR.  The result pairs are the parts of the
// segment that stay OUTSIDE the polygon, still in p1→p2 order.

bool test_clip_segment_subtraction() {
  // Subtraction runs against BOTH orientations: the outside clip is
  // orientation-independent, the CW/CCW fixtures pin that.
  const std::vector<XY> cwSquare{{-1, -1}, {-1, 1}, {1, 1}, {1, -1}};
  const std::vector<XY> ccwSquare{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  if (!expect(xy_signed_area(cwSquare) < 0 && xy_signed_area(ccwSquare) > 0,
              "sub: fixture polygons have opposite orientations")) {
    return false;
  }
  const auto checkAgainstBoth = [&](const XY& p1, const XY& p2,
                                    const std::vector<XY>& expected,
                                    const char* label) {
    for (const auto& square : {cwSquare, ccwSquare}) {
      const auto clipped = clip_segment_outside_polygon(p1, p2, square);
      if (!expect(clipped.size() == expected.size(), label)) {
        std::cerr << "  got " << clipped.size() << " points\n";
        return false;
      }
      for (size_t i = 0; i < expected.size(); ++i) {
        if (!expect(near(clipped[i].x, expected[i].x) &&
                        near(clipped[i].y, expected[i].y),
                    label)) {
          return false;
        }
      }
    }
    return true;
  };
  // Segment crossing the square: the two outside spans survive.
  if (!checkAgainstBoth({-2, 0}, {2, 0},
                        {{-2, 0}, {-1, 0}, {1, 0}, {2, 0}},
                        "sub: crossing segment keeps 2 outside spans")) {
    return false;
  }
  // Segment fully inside: nothing survives.
  if (!checkAgainstBoth({-0.5, 0}, {0.5, 0}, {},
                        "sub: interior segment fully subtracted")) {
    return false;
  }
  // Segment fully outside: survives whole as one pair.
  if (!checkAgainstBoth({3, 0}, {4, 0}, {{3, 0}, {4, 0}},
                        "sub: exterior segment kept as one span")) {
    return false;
  }
  // Segment through a vertex (tangent touch): both outside spans
  // survive, sharing the touching point.
  if (!checkAgainstBoth({0, 2}, {2, 0},
                        {{0, 2}, {1, 1}, {1, 1}, {2, 0}},
                        "sub: vertex tangency keeps both spans")) {
    return false;
  }
  return true;
}

// ── Test 12: join arcs sweep the SHORT way around corners ─────────
//
// Regresses a sweep-normalization bug: join arcs at convex corners
// swept the LONG (270°) way around the vertex, dipping through the
// base polygon's interior (a grown island loop then let milling rows
// cross into the island).  The grown loop of a square must stay
// OUTSIDE the base square — no sampled point strictly inside (0,8)².
// The long-way arc at (0,0) passes through (≈2.12, ≈2.12), inside.

bool test_round_join_short_sweep() {
  const XY pts[] = {{0, 0}, {8, 0}, {8, 8}, {0, 8}};
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < 4; ++i) {
    BaseSegment segment;
    segment.start = pts[i];
    segment.end = pts[(i + 1) % 4];
    base.push_back(segment);
  }
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(base, 3.0, out, /*round_joins=*/true),
              "sweep: offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "sweep: loop connected")) {
    return false;
  }
  const auto samples = sample_offset_loop(out, /*tolerance=*/0.05);
  for (const auto& p : samples) {
    if (p.x > 0.01 && p.x < 7.99 && p.y > 0.01 && p.y < 7.99) {
      std::cerr << "  grown point inside the base square: (" << p.x << ", "
                << p.y << ")\n";
      return expect(false,
                    "sweep: grown loop stays outside the base square");
    }
  }
  // The short arcs hug the corners: the grown loop must still reach
  // the rounded-square extents (weak sanity — bbox is not the pin).
  double minX = 1e9;
  double maxX = -1e9;
  double minY = 1e9;
  double maxY = -1e9;
  for (const auto& p : samples) {
    minX = std::min(minX, p.x);
    maxX = std::max(maxX, p.x);
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }
  return expect(minX <= -2.95 && maxX >= 10.95 && minY <= -2.95 &&
                    maxY >= 10.95,
                "sweep: grown loop reaches the rounded-square extents");
}

}  // namespace

namespace {

using polysmith::core::cam2d::SegmentCleanupStats;
using polysmith::core::cam2d::cleanup_base_segments;

// ── Test 14: shallow corners snap instead of degenerate join arcs ─

// A CW loop with a shallow REFLEX notch at (0.5, −dy): the notch
// corner turns by θ = 2·atan(dy/0.5).  The 0.5 mm notch legs are
// shorter than the miter crossing distance (d/sin θ ≈ 0.9 mm at
// d = 0.075), so the corner reaches the round-join path — exactly the
// facet-corner situation the user's mesh file hit (convex corners
// miter at the corner point, which is why a plain V fixture never
// exercises this branch).
std::vector<BaseSegment> make_shallow_corner_loop(double dy) {
  const XY pts[] = {{0, 0}, {0.5, -dy}, {1, 0}, {1, -1}, {0, -1}};
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < 5; ++i) {
    BaseSegment segment;
    segment.start = pts[i];
    segment.end = pts[(i + 1) % 5];
    base.push_back(segment);
  }
  return base;
}

// Geometric SHORT-way angle between a join arc's endpoints around its
// center — the quantity the snap threshold actually compares (the cw
// convention of offset_arc_sweep wraps it the long way, so measure it
// directly).
double join_short_sweep(const OffsetSegment& segment) {
  const double startAngle = std::atan2(segment.start.y - segment.center.y,
                                       segment.start.x - segment.center.x);
  const double endAngle = std::atan2(segment.end.y - segment.center.y,
                                     segment.end.x - segment.center.x);
  double sweep = endAngle - startAngle;
  const double kPiConst = polysmith::core::cam2d::kPi;
  while (sweep > kPiConst) {
    sweep -= 2.0 * kPiConst;
  }
  while (sweep < -kPiConst) {
    sweep += 2.0 * kPiConst;
  }
  return std::abs(sweep);
}

bool test_shallow_corner_join_snap() {
  // The steep corners may miter or join depending on convexity — only
  // the SHALLOW corner's behavior is pinned: below 5° it snaps (no
  // join arc), above it keeps its join.  Both signs of the boundary
  // matter — the tangency rule.
  const double kSnapSweep =
      5.0 * polysmith::core::cam2d::kPi / 180.0;  // 5° in radians
  const double kSixDeg =
      6.0 * polysmith::core::cam2d::kPi / 180.0;
  // θ ≈ 4.58° (dy = 0.02) — below the threshold: no join arc sweeps
  // under 5°.
  {
    std::vector<OffsetSegment> out;
    if (!expect(offset_closed_loop(make_shallow_corner_loop(0.02), 0.075, out,
                                   /*round_joins=*/true),
                "snap: offset succeeds")) {
      return false;
    }
    int tinyJoins = 0;
    for (const auto& segment : out) {
      if (segment.is_arc && segment.is_join &&
          join_short_sweep(segment) < kSnapSweep) {
        ++tinyJoins;
      }
    }
    if (!expect(tinyJoins == 0,
                "snap: no join arc below the 5° threshold")) {
      return false;
    }
    if (!expect(loop_is_connected(out), "snap: loop connected")) {
      return false;
    }
  }
  // θ ≈ 5.15° (dy = 0.0225) — just above the threshold: exactly one
  // join arc in [5°, 6°] (the shallow corner's), none below 5°.
  {
    std::vector<OffsetSegment> out;
    if (!expect(offset_closed_loop(make_shallow_corner_loop(0.0225), 0.075,
                                   out,
                                   /*round_joins=*/true),
                "join: offset succeeds")) {
      return false;
    }
    int tinyJoins = 0;
    int shallowJoins = 0;
    for (const auto& segment : out) {
      if (!segment.is_arc || !segment.is_join) {
        continue;
      }
      const double sweep = join_short_sweep(segment);
      if (sweep < kSnapSweep) {
        ++tinyJoins;
      } else if (sweep <= kSixDeg) {
        ++shallowJoins;
      }
    }
    if (!expect(tinyJoins == 0 && shallowJoins == 1,
                "join: exactly one join in [5°, 6°], none below 5°")) {
      return false;
    }
    if (!expect(loop_is_connected(out), "join: loop connected")) {
      return false;
    }
  }
  return true;
}

BaseSegment make_line_seg(double x1, double y1, double x2, double y2) {
  BaseSegment segment;
  segment.start = {x1, y1};
  segment.end = {x2, y2};
  return segment;
}

BaseSegment make_arc_seg(double cx, double cy, double radius,
                         double startDeg, double endDeg, bool ccw) {
  const double start = startDeg * polysmith::core::cam2d::kPi / 180.0;
  const double end = endDeg * polysmith::core::cam2d::kPi / 180.0;
  BaseSegment segment;
  segment.is_arc = true;
  segment.center = {cx, cy};
  segment.radius = radius;
  segment.start = {cx + radius * std::cos(start), cy + radius * std::sin(start)};
  segment.end = {cx + radius * std::cos(end), cy + radius * std::sin(end)};
  segment.ccw = ccw;
  return segment;
}

// ── Test 13: contour cleanup (collinear/co-circular merges, drops) ─

bool test_contour_cleanup_units() {
  // Consecutive collinear lines merge into one.
  {
    std::vector<BaseSegment> segs = {make_line_seg(0, 0, 10, 0),
                                     make_line_seg(10, 0, 20, 0)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.merged_lines == 1,
                "cleanup: collinear lines merge")) {
      return false;
    }
    if (!expect(near(segs[0].start.x, 0.0) && near(segs[0].end.x, 20.0) &&
                    near(segs[0].end.y, 0.0),
                "cleanup: merged line spans both pieces")) {
      return false;
    }
  }
  // A bend just above the angle epsilon stays separate — BOTH signs
  // (the tangency rule: regressions hide at the boundary).
  {
    std::vector<BaseSegment> segs = {make_line_seg(0, 0, 10, 0),
                                     make_line_seg(10, 0, 20, 0.01)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 2 && stats.merged_lines == 0,
                "cleanup: 1e-3 rad bend does not merge (+)")) {
      return false;
    }
  }
  {
    std::vector<BaseSegment> segs = {make_line_seg(0, 0, 10, 0),
                                     make_line_seg(10, 0, 20, -0.01)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 2 && stats.merged_lines == 0,
                "cleanup: 1e-3 rad bend does not merge (-)")) {
      return false;
    }
  }
  {
    std::vector<BaseSegment> segs = {make_line_seg(0, 0, 10, 0),
                                     make_line_seg(10, 0, 20, 0.0001)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.merged_lines == 1,
                "cleanup: 1e-5 rad bend merges (+)")) {
      return false;
    }
  }
  {
    std::vector<BaseSegment> segs = {make_line_seg(0, 0, 10, 0),
                                     make_line_seg(10, 0, 20, -0.0001)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.merged_lines == 1,
                "cleanup: 1e-5 rad bend merges (-)")) {
      return false;
    }
  }
  // Consecutive co-circular arcs merge; four quarters become one full
  // circle (start == end, the synthesized-circle convention).
  {
    std::vector<BaseSegment> segs = {make_arc_seg(0, 0, 10, 0, 90, true),
                                     make_arc_seg(0, 0, 10, 90, 180, true)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.merged_arcs == 1,
                "cleanup: co-circular arcs merge")) {
      return false;
    }
    if (!expect(near(segs[0].start.x, 10.0, 1e-6) &&
                    near(segs[0].end.x, -10.0, 1e-6),
                "cleanup: merged arc spans both sweeps")) {
      return false;
    }
  }
  {
    std::vector<BaseSegment> segs = {
        make_arc_seg(0, 0, 10, 0, 90, true),
        make_arc_seg(0, 0, 10, 90, 180, true),
        make_arc_seg(0, 0, 10, 180, 270, true),
        make_arc_seg(0, 0, 10, 270, 360, true)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.merged_arcs == 3,
                "cleanup: quarters merge into a full circle")) {
      return false;
    }
    if (!expect(near(segs[0].start.x, segs[0].end.x) &&
                    near(segs[0].start.y, segs[0].end.y),
                "cleanup: merged circle closes (start == end)")) {
      return false;
    }
  }
  // The same arc walked back immediately is an out-and-back spur —
  // both halves go.
  {
    std::vector<BaseSegment> segs = {make_arc_seg(0, 0, 10, 0, 90, true),
                                     make_arc_seg(0, 0, 10, 90, 0, false)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.empty() && stats.dropped_spurs == 1,
                "cleanup: arc retrace spur is dropped")) {
      return false;
    }
  }
  // Different circles stay separate even when chained.
  {
    std::vector<BaseSegment> segs = {make_arc_seg(0, 0, 10, 0, 90, true),
                                     make_arc_seg(1, 0, 10, 0, 90, true)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 2 && stats.merged_arcs == 0,
                "cleanup: different centers do not merge")) {
      return false;
    }
  }
  // Exact consecutive duplicates drop (the double-line case).
  {
    std::vector<BaseSegment> segs = {make_line_seg(0, 0, 10, 0),
                                     make_line_seg(0, 0, 10, 0)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.dropped_duplicates == 1,
                "cleanup: consecutive duplicate drops")) {
      return false;
    }
  }
  // A zero-length line vanishes without breaking the chain; a
  // full-circle arc (start == end by convention) is preserved.
  {
    std::vector<BaseSegment> segs = {make_line_seg(5, 5, 5, 5),
                                     make_line_seg(5, 5, 10, 5)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && stats.dropped_degenerate == 1 &&
                    near(segs[0].end.x, 10.0),
                "cleanup: zero-length line drops, chain survives")) {
      return false;
    }
  }
  {
    std::vector<BaseSegment> segs = {make_arc_seg(0, 0, 5, 0, 360, true)};
    const auto stats = cleanup_base_segments(segs);
    if (!expect(segs.size() == 1 && !stats.any(),
                "cleanup: full-circle arc preserved")) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cam2d_test\n";
  const auto run = [&](const char* label, bool (*test)()) {
    std::cout << "  " << label << "... ";
    bool ok = false;
    try {
      ok = test();
    } catch (const std::exception& error) {
      std::cerr << "\n  EXCEPTION: " << error.what() << "\n";
    } catch (...) {
      std::cerr << "\n  UNKNOWN EXCEPTION\n";
    }
    if (ok) {
      std::cout << "PASS\n";
    } else {
      std::cout << "FAIL\n";
      allPassed = false;
    }
  };
  run("Test 1: round joins on a square", test_round_join_square);
  run("Test 2: miter mode on a square", test_miter_square);
  run("Test 3: collinear miter propagation", test_miter_collinear_propagation);
  run("Test 4: round mode shallow miter", test_round_mode_shallow_miter);
  run("Test 5: sampling sagitta", test_sampling_sagitta);
  run("Test 6: segment clipping", test_clip_segment);
  run("Test 7: loop containment", test_loop_contains);
  run("Test 8: area centroid", test_area_centroid);
  run("Test 9: chord-tolerance wire sampling", test_wire_sampling);
  run("Test 10: self-intersection scan", test_self_intersection_scan);
  run("Test 11: outside clip = subtraction", test_clip_segment_subtraction);
  run("Test 12: join arcs sweep short", test_round_join_short_sweep);
  run("Test 13: contour cleanup unit rules", test_contour_cleanup_units);
  run("Test 14: shallow corners snap (no degenerate joins)",
      test_shallow_corner_join_snap);

  if (allPassed) {
    std::cout << "cam2d_test passed\n";
    return 0;
  }
  return 1;
}
