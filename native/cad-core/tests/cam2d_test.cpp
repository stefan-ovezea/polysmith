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
  // A square with a shallow dent in the bottom edge.  The dent corners
  // (~5.7° turns) are NEAR-COLLINEAR: the offset snaps them instead
  // of emitting a sliver join arc (the micro-arc machine-killer —
  // see Test 13).  Only the four real square corners keep join arcs.
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
  return expect(joins == 4 && lines == 6,
                "dent: 4 join arcs (square corners); near-collinear dent "
                "corners snapped with no sliver arcs");
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

// ── Test 13: near-collinear corners get no micro join arcs ───────
//
// Tessellated chord outlines (projected STL geometry) bend by a
// fraction of a degree at every chord junction.  The kerf-offset
// round join at such a corner is an arc a few microns long — a
// machine-killer (user-reported: GRBL stalls on sub-0.05mm arcs).
// The offset must snap near-collinear corners so no emitted segment
// is shorter than 0.05mm.
bool test_no_micro_joins_on_near_collinear_corners() {
  // Two 2° corners on the bottom edge (tessellation-like), sharp
  // closing corners elsewhere.
  const XY pts[] = {{0, 0}, {10, 0}, {20, 0.35}, {30, 1.05}, {30, 10}, {0, 10}};
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < 6; ++i) {
    BaseSegment segment;
    segment.start = pts[i];
    segment.end = pts[(i + 1) % 6];
    base.push_back(segment);
  }
  const double d = 0.075;  // kerf/2 — the user's laser kerf
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(base, d, out, /*round_joins=*/true),
              "micro-join: offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "micro-join: loop connected")) {
    return false;
  }
  for (const auto& segment : out) {
    double length = 0.0;
    if (!segment.is_arc) {
      length = xy_length(segment.end.x - segment.start.x,
                         segment.end.y - segment.start.y);
    } else {
      const double startAngle =
          std::atan2(segment.start.y - segment.center.y,
                     segment.start.x - segment.center.x);
      const double endAngle =
          std::atan2(segment.end.y - segment.center.y,
                     segment.end.x - segment.center.x);
      double sweep = endAngle - startAngle;
      if (segment.cw && sweep > 0) sweep -= 6.28318530717958647692;
      if (!segment.cw && sweep < 0) sweep += 6.28318530717958647692;
      length = segment.radius * std::abs(sweep);
    }
    if (!expect(length >= 0.05,
                "micro-join: no segment shorter than 0.05mm")) {
      return false;
    }
  }
  return true;
}

// ── Test 14: sharp arc corners miter-trim instead of round-joining ─
//
// User-reported regression (laser-cut wheel): every hole bounded by
// trimmed circles was skipped as "self-intersects after the kerf
// offset".  The old arc-corner path always drew a round join — for a
// corner that turns toward the offset side the two offset curves cross
// ahead of the corner, so the join bulged into the stock and crossed
// the neighbouring offset arc.  The true boundary is the miter at the
// crossing.  Corners that turn away keep the round join (rolling ball).

bool test_arc_corner_miter() {
  // Lens hole: circle A (0,0) r=10 and circle B (12,0) r=10 intersect
  // at (6,±8).  CW walk (interior on the right): down the right arc of
  // A through (10,0), up the left arc of B through (2,0).  The tips
  // are 73.7° on the interior side.
  const auto make_lens = []() {
    std::vector<BaseSegment> base;
    BaseSegment a;
    a.is_arc = true;
    a.center = {0.0, 0.0};
    a.radius = 10.0;
    a.start = {6.0, 8.0};
    a.end = {6.0, -8.0};
    a.ccw = false;
    base.push_back(a);
    BaseSegment b;
    b.is_arc = true;
    b.center = {12.0, 0.0};
    b.radius = 10.0;
    b.start = {6.0, -8.0};
    b.end = {6.0, 8.0};
    b.ccw = false;
    base.push_back(b);
    return base;
  };

  // Inward (hole-side) offset: the tips must miter — no join arcs, no
  // self-intersection, and the whole offset stays inside the lens.
  const auto lens = make_lens();
  std::vector<OffsetSegment> in;
  if (!expect(offset_closed_loop(lens, 0.075, in, /*round_joins=*/true),
              "arc-miter: inward offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(in), "arc-miter: inward loop connected")) {
    return false;
  }
  int joins = 0;
  for (const auto& segment : in) {
    if (segment.is_join) {
      ++joins;
    }
  }
  if (!expect(joins == 0, "arc-miter: no join arcs at the sharp tips")) {
    return false;
  }
  const auto inSamples = sample_offset_loop(in, /*tolerance=*/0.05);
  if (!expect(!offset_loop_self_intersects(inSamples),
              "arc-miter: inward offset must not self-intersect")) {
    return false;
  }
  for (const auto& p : inSamples) {
    if (xy_length(p.x, p.y) > 10.0 + 1e-9 &&
        xy_length(p.x - 12.0, p.y) > 10.0 + 1e-9) {
      std::cerr << "  inward sample outside the lens: (" << p.x << ", "
                << p.y << ")\n";
      return expect(false, "arc-miter: inward offset stays inside the lens");
    }
  }

  // Outward (stock-side) offset: the tips are reflex there — the round
  // join is the rolling ball and must survive, still without
  // self-intersecting, and no sample may fall inside the lens.
  std::vector<OffsetSegment> out;
  if (!expect(offset_closed_loop(lens, -0.075, out, /*round_joins=*/true),
              "arc-miter: outward offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(out), "arc-miter: outward loop connected")) {
    return false;
  }
  joins = 0;
  for (const auto& segment : out) {
    if (segment.is_join) {
      ++joins;
    }
  }
  if (!expect(joins == 2, "arc-miter: outward keeps join arcs at the tips")) {
    return false;
  }
  const auto outSamples = sample_offset_loop(out, /*tolerance=*/0.05);
  if (!expect(!offset_loop_self_intersects(outSamples),
              "arc-miter: outward offset must not self-intersect")) {
    return false;
  }
  for (const auto& p : outSamples) {
    if (xy_length(p.x, p.y) < 10.0 - 1e-9 &&
        xy_length(p.x - 12.0, p.y) < 10.0 - 1e-9) {
      std::cerr << "  outward sample inside the lens: (" << p.x << ", "
                << p.y << ")\n";
      return expect(false, "arc-miter: outward offset stays outside the lens");
    }
  }

  // The user's wheel hole, exact literals (one petal hole between the
  // hub circles, trimmed by two row-2 circles).  Must offset cleanly
  // at the laser kerf.
  std::vector<BaseSegment> wheel;
  BaseSegment w1;
  w1.is_arc = true;
  w1.center = {-105.08492923935869, 36.344119490276988};
  w1.radius = 67.725904377979504;
  w1.start = {-42.797058105468253, 9.7543182373047248};
  w1.end = {-49.720581054687251, -2.6634063720702734};
  w1.ccw = false;
  wheel.push_back(w1);
  BaseSegment w2;
  w2.is_arc = true;
  w2.center = {-54.715822198817875, 52.109290715970374};
  w2.radius = 54.999994801713491;
  w2.start = {-49.720581054687251, -2.6634063720702734};
  w2.end = {-75.138923016298421, 1.0417130934194034};
  w2.ccw = false;
  wheel.push_back(w2);
  BaseSegment w3;
  w3.is_arc = true;
  w3.center = {-105.23047599194608, 63.324531239794212};
  w3.radius = 69.171171717273978;
  w3.start = {-75.138923016298421, 1.0417130934194034};
  w3.end = {-62.631344404574548, 8.8271461214015261};
  w3.ccw = true;
  wheel.push_back(w3);
  BaseSegment w4;
  w4.is_arc = true;
  w4.center = {-54.715822198817875, 52.109290715970374};
  w4.radius = 43.999994687442772;
  w4.start = {-62.631344404574548, 8.8271461214015261};
  w4.end = {-42.797058105468253, 9.7543182373047248};
  w4.ccw = true;
  wheel.push_back(w4);

  std::vector<OffsetSegment> wheelOut;
  if (!expect(offset_closed_loop(wheel, 0.075, wheelOut,
                                 /*round_joins=*/true),
              "arc-miter: wheel hole offset succeeds")) {
    return false;
  }
  if (!expect(loop_is_connected(wheelOut), "arc-miter: wheel loop connected")) {
    return false;
  }
  const auto wheelSamples = sample_offset_loop(wheelOut, /*tolerance=*/0.05);
  return expect(!offset_loop_self_intersects(wheelSamples),
                "arc-miter: wheel hole must not self-intersect");
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
  run("Test 13: near-collinear corners get no micro joins",
      test_no_micro_joins_on_near_collinear_corners);
  run("Test 14: sharp arc corners miter-trim", test_arc_corner_miter);

  if (allPassed) {
    std::cout << "cam2d_test passed\n";
    return 0;
  }
  return 1;
}
