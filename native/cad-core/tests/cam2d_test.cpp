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
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
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
  // The polygon-clip form may repeat the intersection points; the
  // consumers (milling rows) read clipped.front() and clipped.back().
  const auto inside =
      clip_segment_to_polygon({-2, 0}, {2, 0}, square);
  double minX = 1e9;
  double maxX = -1e9;
  for (const auto& p : inside) {
    minX = std::min(minX, p.x);
    maxX = std::max(maxX, p.x);
    if (!near(p.y, 0.0, 1e-9)) {
      return expect(false, "clip: clipped points stay on the line");
    }
  }
  if (!expect(inside.size() >= 2 && near(minX, -1.0, 1e-9) &&
                  near(maxX, 1.0, 1e-9),
              "clip: crossing segment trimmed to [-1, 1]")) {
    return false;
  }
  const auto outside =
      clip_segment_to_polygon({5, 0}, {6, 0}, square);
  return expect(outside.size() < 2, "clip: outside segment dropped");
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
  TColgp_Array1OfPnt poles(1, 4);
  poles.SetValue(1, p3);
  poles.SetValue(2, gp_Pnt(-0.5, 1.5, 0));
  poles.SetValue(3, gp_Pnt(0.5, 0.5, 0));
  poles.SetValue(4, p0);
  TColStd_Array1OfReal knots(1, 2);
  knots.SetValue(1, 0.0);
  knots.SetValue(2, 1.0);
  TColStd_Array1OfInteger mults(1, 2);
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

  if (allPassed) {
    std::cout << "cam2d_test passed\n";
    return 0;
  }
  return 1;
}
