// Trim regression suite — 2026-08 user report: with dense circle
// arrangements the trim "floods": the red highlight shows one segment
// while the trim deletes another (sometimes more, sometimes less),
// because the trim re-derived the segment from the click point instead
// of using the highlighted one.
//
// Fix under test:
// 1. trim_sketch_entity accepts an optional segment_index (the
//    hovered index from the core's trim_preview result). When
//    present, the trim deletes EXACTLY the highlighted segment —
//    the click point no longer participates in segment selection.
// 2. The distance fallback in select_clicked_segment now measures
//    against the ARC itself instead of the chord. The old chord
//    metric scores a click on a long arc at the sagitta distance
//    (r for a semicircle), which exceeds the selection tolerance
//    and hands the click to a neighbouring segment.
//
// Geometry: the user's flower — a big circle with small circles on
// its perimeter, trimming the notch segments the small circles cut
// out of the big circle.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/document/feature.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_profile.h"
#include "core/sketch/trim_engine.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::SketchArc;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::TrimSegment;
using polysmith::core::add_sketch_arc;
using polysmith::core::build_sketch_profile_regions;
using polysmith::core::create_sketch_feature;
using polysmith::core::find_all_intersections;
using polysmith::core::point_trim_segment_distance_sq;
using polysmith::core::select_clicked_segment;
using polysmith::core::split_circle_at_intersections;
using polysmith::core::split_line_at_intersections;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

constexpr double kPi = 3.14159265358979323846;

double wrap_angle(double a) {
  const double k2Pi = 2.0 * kPi;
  while (a < 0.0) a += k2Pi;
  while (a >= k2Pi) a -= k2Pi;
  return a;
}

// True when the wrapped angle `a` lies inside the arc's material sweep.
bool arc_contains_angle(const SketchArc& arc, double a) {
  const double s = wrap_angle(std::atan2(arc.start_y - arc.center_y,
                                         arc.start_x - arc.center_x));
  const double e = wrap_angle(std::atan2(arc.end_y - arc.center_y,
                                         arc.end_x - arc.center_x));
  a = wrap_angle(a);
  if (arc.ccw) {
    double aa = a < s ? a + 2.0 * kPi : a;
    double ee = e <= s ? e + 2.0 * kPi : e;
    return aa >= s - 1e-9 && aa <= ee + 1e-9;
  }
  double aa = a > s ? a - 2.0 * kPi : a;
  double ee = e >= s ? e - 2.0 * kPi : e;
  return aa <= s + 1e-9 && aa >= ee - 1e-9;
}

// Asserts a line's endpoints against expected coordinates (±eps).
bool expect_line_endpoints(const SketchFeatureParameters& params,
                           const std::string& id, double sx, double sy,
                           double ex, double ey, double eps, const char* what) {
  for (const auto& line : params.lines) {
    if (line.id != id) continue;
    const bool ok =
        std::abs(line.start_x - sx) <= eps && std::abs(line.start_y - sy) <= eps &&
        std::abs(line.end_x - ex) <= eps && std::abs(line.end_y - ey) <= eps;
    if (!ok) {
      std::cerr << "  " << what << ": line " << id << " is ("
                << line.start_x << "," << line.start_y << ")->("
                << line.end_x << "," << line.end_y << ") expected ("
                << sx << "," << sy << ")->(" << ex << "," << ey << ")\n";
      return false;
    }
    return true;
  }
  std::cerr << "  " << what << ": line " << id << " not found\n";
  return false;
}

bool test_long_line_crossing_near_endpoint_keeps_line() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // A 1000 mm line crossed at t = 0.005 — inside the old endpoint
  // filter's 1% band, which made the crossing invisible and turned the
  // trim into a full delete of the whole line.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 1000.0, 0.0);
  document = manager.add_sketch_line(5.0, -10.0, 5.0, 10.0);

  document = manager.trim_sketch_entity("line-1", 900.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  if (!expect(params.lines.size() == 2,
              "long line: trim shortens, it does not delete")) {
    std::cerr << "  lines=" << params.lines.size() << "\n";
    return false;
  }
  return expect_line_endpoints(params, "line-1", 0.0, 0.0, 5.0, 0.0, 1e-6,
                               "long line") &&
         expect_line_endpoints(params, "line-2", 5.0, -10.0, 5.0, 10.0, 1e-6,
                               "crossing line untouched");
}

bool test_short_lines_at_angle_are_not_parallel() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Two SHORT perpendicular lines: cross product = 0.005 mm², which is
  // less than the old 0.01 mm parallel threshold — the crossing was
  // reported "parallel", the intersection list came back empty, and the
  // trim deleted line-2 outright. The parallel test must be relative to
  // the segment lengths (it is really the sine of the angle).
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 0.05, 0.0);
  document = manager.add_sketch_line(0.025, -0.05, 0.025, 0.05);

  document = manager.trim_sketch_entity("line-2", 0.025, 0.03);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  if (!expect(params.lines.size() == 2,
              "short lines: perpendicular lines cross, line survives")) {
    std::cerr << "  lines=" << params.lines.size() << "\n";
    return false;
  }
  return expect_line_endpoints(params, "line-2", 0.025, -0.05, 0.025, 0.0,
                               1e-9, "short lines");
}

bool test_circle_tangent_single_intersection_keeps_circle_whole() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // A tangent line touches the circle without crossing it. There is
  // nothing to trim: the circle must survive untouched, not become a
  // degenerate coincident-endpoint arc (the old splitter emitted one
  // "segment" spanning the full 2π from the tangent point back to
  // itself, and the profile walker later read that as a full circle).
  DocumentState document = manager.add_sketch_circle(0.0, 0.0, 20.0);
  document = manager.add_sketch_line(-40.0, 20.0, 40.0, 20.0);

  document = manager.trim_sketch_entity("circle-1", 0.0, -20.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  return expect(params.circles.size() == 1,
                "tangent circle: circle survives a tangent trim") &&
         expect(params.arcs.empty(),
                "tangent circle: no degenerate arc is created") &&
         expect(std::abs(params.circles.front().radius - 20.0) <= 1e-9,
                "tangent circle: radius unchanged");
}

bool test_construction_line_acts_as_cutting_edge() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Construction geometry participates as a cutting edge (the original
  // tool design and mainstream CAD behaviour). The old code skipped it,
  // so the horizontal line read as isolated and was deleted whole.
  DocumentState document = manager.add_sketch_line(-50.0, 0.0, 50.0, 0.0);
  document = manager.add_sketch_line(0.0, -40.0, 0.0, 40.0,
                                     /*is_construction=*/true);

  document = manager.trim_sketch_entity("line-1", 25.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  if (!expect(params.lines.size() == 2,
              "construction: the line survives, cut by the construction line")) {
    std::cerr << "  lines=" << params.lines.size() << "\n";
    return false;
  }
  // Clicking the right half (x = 25) deletes it; the left half from the
  // line start up to the construction crossing survives.
  return expect_line_endpoints(params, "line-1", -50.0, 0.0, 0.0, 0.0, 1e-6,
                               "construction cut");
}

bool test_concentric_equal_radius_circles_no_nan() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Two coincident circles used to produce 0/0 → NaN intersection
  // parameters, and sorting on NaN violates std::sort's strict weak
  // ordering (undefined behaviour). Coincident circles have no discrete
  // intersections, so trimming one deletes only that one — and every
  // survivor must be finite.
  DocumentState document = manager.add_sketch_circle(0.0, 0.0, 50.0);
  document = manager.add_sketch_circle(0.0, 0.0, 50.0);

  document = manager.trim_sketch_entity("circle-1", 50.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  if (!expect(params.circles.size() == 1,
              "coincident circles: trimming one deletes only that one")) {
    std::cerr << "  circles=" << params.circles.size() << "\n";
    return false;
  }
  const auto& c = params.circles.front();
  return expect(std::isfinite(c.center_x) && std::isfinite(c.center_y) &&
                    std::isfinite(c.radius),
                "coincident circles: all surviving fields finite");
}

const SketchLine* find_line_by_id(const SketchFeatureParameters& params,
                                  const std::string& id) {
  for (const auto& line : params.lines) {
    if (line.id == id) return &line;
  }
  return nullptr;
}

const polysmith::core::SketchVertex* find_vertex_by_id(
    const SketchFeatureParameters& params, const std::string& id) {
  for (const auto& vertex : params.vertices) {
    if (vertex.id == id) return &vertex;
  }
  return nullptr;
}

bool test_trim_split_point_exactly_on_intersection() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Clicking the LEFT piece of line-1 deletes it; line-1 keeps its id
  // and becomes the right portion, whose start sits exactly on the
  // analytic intersection with line-2.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
  document = manager.add_sketch_line(50.0, -20.0, 50.0, 20.0);

  document = manager.trim_sketch_entity("line-1", 25.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  const auto* right = find_line_by_id(params, "line-1");
  if (!expect(right != nullptr, "split point: trimmed line survives")) {
    return false;
  }
  if (!expect(std::abs(right->start_x - 50.0) <= 1e-9 &&
                  std::abs(right->start_y) <= 1e-9 &&
                  std::abs(right->end_x - 100.0) <= 1e-9,
              "split point: new start sits exactly on the intersection")) {
    std::cerr << "  line-1 = (" << right->start_x << "," << right->start_y
              << ")->(" << right->end_x << "," << right->end_y
              << ") expected (50,0)->(100,0)\n";
    return false;
  }
  // The freshly minted vertex must be frozen against the solver pass —
  // a moved split point re-opens the loop the trim just welded.
  const auto* vertex = find_vertex_by_id(params, right->start_vertex_id);
  if (!expect(vertex != nullptr && vertex->is_fixed,
              "split point: minted vertex is frozen (is_fixed)")) {
    if (vertex != nullptr) {
      std::cerr << "  vertex " << vertex->id << " kind=" << vertex->kind
                << " fixed=" << vertex->is_fixed << " at (" << vertex->x
                << "," << vertex->y << ")\n";
    } else {
      std::cerr << "  vertex " << right->start_vertex_id << " not found\n";
      for (const auto& v : params.vertices) {
        std::cerr << "    " << v.id << " fixed=" << v.is_fixed << " ("
                  << v.x << "," << v.y << ")\n";
      }
    }
    return false;
  }
  return true;
}

bool test_trim_split_point_adopts_existing_vertex_id() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // line-2's START endpoint sits exactly at (50, 0). Trimming line-1
  // there must ADOPT line-2's start vertex id instead of minting a new
  // one — a shared id is how the wire walk welds the two curves.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
  document = manager.add_sketch_line(50.0, 0.0, 50.0, 20.0);

  document = manager.trim_sketch_entity("line-1", 25.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  const auto* right = find_line_by_id(params, "line-1");
  const auto* vertical = find_line_by_id(params, "line-2");
  if (!expect(right != nullptr && vertical != nullptr,
              "shared vertex: both lines exist")) {
    return false;
  }
  return expect(right->start_vertex_id == vertical->start_vertex_id,
                "shared vertex: trimmed endpoint adopts the crossing "
                "line's start vertex id");
}

bool test_rectangle_vertical_trim_leaves_two_closed_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Rectangle + a vertical line crossing it from outside. Trimming the
  // stub pieces leaves the middle span, which must close TWO regions
  // (left and right halves). The wire walk welds the line ends to the
  // rectangle corners only if the trim placed them exactly there and
  // shared the corner vertex ids — a drifted endpoint leaves a degree-1
  // node, the dangling-curve pass strips the line, and the regions
  // silently stop closing.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
  document = manager.add_sketch_line(100.0, 0.0, 100.0, 50.0);
  document = manager.add_sketch_line(100.0, 50.0, 0.0, 50.0);
  document = manager.add_sketch_line(0.0, 50.0, 0.0, 0.0);
  document = manager.add_sketch_line(50.0, -10.0, 50.0, 60.0);

  // Click the stub above the rectangle, then the stub below.
  document = manager.trim_sketch_entity("line-5", 50.0, 55.0);
  document = manager.trim_sketch_entity("line-5", 50.0, -5.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  std::string reason;
  const std::vector<polysmith::test::ExpectedProfile> expected = {
      {{"line-1", "line-2", "line-3", "line-5"}, "polygon"},
      {{"line-1", "line-4", "line-3", "line-5"}, "polygon"},
  };
  if (!polysmith::test::profiles_match(document, expected, &reason)) {
    std::cerr << "  rectangle trim profiles: " << reason << "\n";
    for (const auto& p : params.profiles) {
      std::cerr << "    kind=" << p.kind << " ids=";
      for (const auto& id : p.line_ids) std::cerr << id << " ";
      std::cerr << "\n";
    }
    return false;
  }
  return true;
}

bool test_circle_trim_line_split_uses_noncolliding_ids() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Circle on a long line; two vertical lines cross the circle. After
  // the circle→arc trim splits the long line at the arc's endpoints,
  // every line id must be unique. The old scheme minted the new id from
  // lines.size() + 1, which collides with a surviving line after any
  // earlier delete — a duplicated id corrupts wires and faces.
  DocumentState document = manager.add_sketch_line(-50.0, 0.0, 150.0, 0.0);
  document = manager.add_sketch_line(0.0, -40.0, 0.0, 40.0);
  document = manager.add_sketch_line(80.0, -40.0, 80.0, 40.0);
  document = manager.add_sketch_circle(0.0, 0.0, 20.0);

  // Delete the middle vertical line so a size-based scheme would mint
  // its id again (the vertical lines are line-2 and line-3; deleting
  // line-2 leaves 2 existing lines and size+1 = 3 = line-3's id).
  document = manager.delete_sketch_selection({"line-2"}, {}, {});

  // Trim the circle's right piece: the circle becomes an arc whose
  // endpoints land at (20, 0) and (-20, 0) on the long line.
  document = manager.trim_sketch_entity("circle-1", 20.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  std::set<std::string> seen;
  for (const auto& line : params.lines) {
    if (!expect(seen.insert(line.id).second,
                "circle trim: line ids must be pairwise distinct")) {
      std::cerr << "  duplicate id: " << line.id << "\n";
      return false;
    }
  }
  return true;
}

bool test_trim_survives_solver_pass() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // A rectangle with a driving dimension (so planegcs runs after the
  // trim) plus a crossing line. After trimming both stubs, the solver
  // pass must not move the freshly minted split points — the two
  // closed profiles must survive.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
  document = manager.add_sketch_line(100.0, 0.0, 100.0, 50.0);
  document = manager.add_sketch_line(100.0, 50.0, 0.0, 50.0);
  document = manager.add_sketch_line(0.0, 50.0, 0.0, 0.0);
  document = manager.add_sketch_line(50.0, -10.0, 50.0, 60.0);
  document = manager.add_sketch_line_length_dimension("line-1");

  document = manager.trim_sketch_entity("line-5", 50.0, 55.0);
  document = manager.trim_sketch_entity("line-5", 50.0, -5.0);

  std::string reason;
  const std::vector<polysmith::test::ExpectedProfile> expected = {
      {{"line-1", "line-2", "line-3", "line-5"}, "polygon"},
      {{"line-1", "line-4", "line-3", "line-5"}, "polygon"},
  };
  if (!polysmith::test::profiles_match(document, expected, &reason)) {
    std::cerr << "  solver-pass trim profiles: " << reason << "\n";
    return false;
  }
  return true;
}

bool test_line_crossing_only_ellipse_keeps_line() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Ellipse a=50 b=20 at the origin; a long line through it. Before
  // ellipse cutting edges, the line read as isolated and the trim
  // deleted it whole — this test pins the fix.
  DocumentState document = manager.add_sketch_line(-100.0, 0.0, 100.0, 0.0);
  document = manager.add_sketch_ellipse(0.0, 0.0, 50.0, 0.0, 0.0, 20.0);

  document = manager.trim_sketch_entity("line-1", 0.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  if (!expect(params.lines.size() == 2,
              "ellipse cut: the line survives, split at the ellipse")) {
    std::cerr << "  lines=" << params.lines.size() << "\n";
    return false;
  }
  for (const auto& line : params.lines) {
    if (line.id == "line-1") {
      // Clicking the middle piece splits the line; line-1 keeps its id
      // as the left portion, exactly from the line start to the
      // ellipse crossing.
      if (!expect(std::abs(line.start_x - (-100.0)) <= 1e-6 &&
                      std::abs(line.end_x - (-50.0)) <= 1e-6,
                  "ellipse cut: left piece runs (-100,0) -> (-50,0)")) {
        std::cerr << "  line-1 = (" << line.start_x << "," << line.start_y
                  << ")->(" << line.end_x << "," << line.end_y << ")\n";
        return false;
      }
    }
  }
  return true;
}

bool test_line_crossing_only_spline_keeps_line() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Cubic arch spline crossing the x-axis twice; a long line through
  // it. Before spline cutting edges the line was deleted whole.
  const std::vector<std::pair<double, double>> poles = {
      {-50.0, 0.0}, {-25.0, 40.0}, {25.0, 40.0}, {50.0, 0.0}};
  DocumentState document = manager.add_sketch_line(-100.0, 0.0, 100.0, 0.0);
  document = manager.add_sketch_spline(poles);

  document = manager.trim_sketch_entity("line-1", 0.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  return expect(params.lines.size() == 2,
                "spline cut: the line survives, split at the spline");
}

bool test_circle_crossing_only_ellipse_keeps_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Circle r=30 at the origin inside an ellipse a=50 b=20 — four
  // crossings. Trimming one circle piece converts the circle to an
  // arc instead of deleting it as isolated.
  DocumentState document = manager.add_sketch_circle(0.0, 0.0, 30.0);
  document = manager.add_sketch_ellipse(0.0, 0.0, 50.0, 0.0, 0.0, 20.0);

  document = manager.trim_sketch_entity("circle-1", 30.0, 0.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  return expect(params.circles.empty() && params.arcs.size() == 1,
                "ellipse cut: circle converts to one complementary arc");
}

bool test_line_ellipse_profile_closes() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // An ellipse plus a chord whose endpoints land EXACTLY on the
  // ellipse — the two lens regions must be detected. The walk had no
  // ellipse touch records before the shared-curve layer, so the chord
  // stayed dangling and this profile set was empty.
  DocumentState document =
      manager.add_sketch_ellipse(0.0, 0.0, 50.0, 0.0, 0.0, 20.0);
  document = manager.add_sketch_line(-50.0, 0.0, 50.0, 0.0);

  std::string reason;
  const std::vector<polysmith::test::ExpectedProfile> expected = {
      {{"ellipse-1", "line-1"}, "polygon"},
      {{"ellipse-1", "line-1"}, "polygon"},
  };
  if (!polysmith::test::profiles_match(document, expected, &reason)) {
    std::cerr << "  ellipse-chord profiles: " << reason << "\n";
    const auto& params =
        document.feature_history.back().sketch_parameters.value();
    for (const auto& p : params.profiles) {
      std::cerr << "    kind=" << p.kind << " ids=";
      for (const auto& id : p.line_ids) std::cerr << id << " ";
      std::cerr << "\n";
    }
    return false;
  }
  return true;
}

bool test_notch_trim_deletes_the_highlighted_segment() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Big circle r=50 at the origin.
  DocumentState document = manager.add_sketch_circle(0.0, 0.0, 50.0);
  // Three small circles r=10 with centers ON the big circle, at 0,
  // 120 and 240 degrees — each cuts a notch into the big circle.
  document = manager.add_sketch_circle(50.0, 0.0, 10.0);
  document = manager.add_sketch_circle(-25.0, 43.30127018922193, 10.0);
  document = manager.add_sketch_circle(-25.0, -43.30127018922193, 10.0);

  const auto& feature = document.feature_history.back();
  const auto& params = feature.sketch_parameters.value();
  const auto& big = params.circles.front();  // circle-1

  // The hover preview computed by the core: hover over the notch at
  // angle 0 (inside the first small circle's cut).
  const auto intersections = find_all_intersections(big, params);
  if (!expect(!intersections.empty(),
              "notch trim: intersections detected on the big circle")) {
    return false;
  }
  const auto segments = split_circle_at_intersections(big, intersections);
  if (!expect(segments.size() >= 6,
              "notch trim: the 3 small circles split the big circle")) {
    return false;
  }
  // Click exactly on the notch: point at angle 0 on the big circle.
  const int hovered = select_clicked_segment(segments, big, 50.0, 0.0);
  if (!expect(hovered >= 0, "notch trim: notch segment hovered")) {
    return false;
  }

  // The user moves the pointer slightly before clicking: the click
  // lands in a DIFFERENT segment (angle 90 deg — the large piece
  // between two small circles). The trim must still delete the
  // HIGHLIGHTED segment, because the preview index rides along.
  document = manager.trim_sketch_entity("circle-1", 0.0, 50.0, hovered);

  const auto& after = document.feature_history.back()
                          .sketch_parameters.value();
  // The three small circles remain; only the big circle converted.
  if (!expect(after.circles.size() == 3,
              "notch trim: only the big circle was converted")) {
    std::cerr << "  circles=" << after.circles.size()
              << " arcs=" << after.arcs.size() << "\n";
    return false;
  }
  if (!expect(after.arcs.size() == 1,
              "notch trim: exactly one complementary arc remains")) {
    return false;
  }
  const auto& arc = after.arcs.front();
  // The notch (angle 0) must be gone; the clicked-but-NOT-highlighted
  // area (angle 90 deg) must remain.
  return expect(!arc_contains_angle(arc, 0.0),
                "notch trim: the highlighted notch was deleted") &&
         expect(arc_contains_angle(arc, kPi / 2.0),
                "notch trim: the un-highlighted segment survived");
}

bool test_arc_distance_metric_beats_chord_for_long_segments() {
  // Semicircle segment: click ON the arc at its middle (angle pi/2).
  // The chord distance there is the sagitta = r = 50 — far beyond the
  // 5 mm selection tolerance, which is how the old chord fallback
  // misattributed long-arc clicks to other segments. The arc-aware
  // metric scores ~0.
  TrimSegment semicircle{};
  semicircle.kind = TrimSegment::ARC_SEGMENT;
  semicircle.param_start = 0.0;
  semicircle.param_end = kPi;
  semicircle.start_x = 50.0;
  semicircle.start_y = 0.0;
  semicircle.end_x = -50.0;
  semicircle.end_y = 0.0;
  semicircle.center_x = 0.0;
  semicircle.center_y = 0.0;
  semicircle.radius = 50.0;
  semicircle.ccw = true;

  const double click_x = 0.0;
  const double click_y = 50.0;  // on the arc, at its middle
  const double dist_sq =
      point_trim_segment_distance_sq(semicircle, click_x, click_y);
  if (!expect(dist_sq < 1.0,
              "arc metric: click on a long arc scores near zero")) {
    std::cerr << "  arc distance = " << dist_sq
              << " (chord distance would be 2500)\n";
    return false;
  }

  // A point just outside the sweep measures to the nearer endpoint.
  const double outside_sq = point_trim_segment_distance_sq(
      semicircle, 50.0, -1.0);
  return expect(outside_sq > 0.0 && outside_sq < 2.0,
                "arc metric: outside the sweep measures to the endpoint");
}

// Replays the user's full flower workflow: a big circle with 10
// small circles arrayed on its perimeter, every small circle trimmed
// to its outer arc and every big-circle notch between petals trimmed.
// Must complete without a crash and leave no full-circle region.
bool test_full_flower_trim_workflow() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const double big_r = 50.0;
  const double petal_r = 8.75;
  DocumentState document = manager.add_sketch_circle(0.0, 0.0, big_r);
  for (int k = 0; k < 10; ++k) {
    const double a = k * 36.0 * kPi / 180.0;
    document = manager.add_sketch_circle(
        big_r * std::cos(a), big_r * std::sin(a), petal_r);
  }

  const auto sketch_params = [&]() -> const polysmith::core::SketchFeatureParameters& {
    return document.feature_history.back().sketch_parameters.value();
  };

  // 1. Trim each small circle's inner arc (toward the origin). Trim by
  // entity id — the circles vector shrinks as trims convert entities,
  // so index-based lookup would drift (and the app trims by id).
  {
    std::vector<std::string> petal_ids;
    for (const auto& c : sketch_params().circles) {
      if (c.radius < big_r - 1.0) petal_ids.push_back(c.id);
    }
    for (const auto& petal_id : petal_ids) {
      const auto& params = sketch_params();
      const auto small_it = std::find_if(
          params.circles.begin(), params.circles.end(),
          [&](const auto& c) { return c.id == petal_id; });
      if (small_it == params.circles.end()) {
        continue;  // already converted by an earlier trim
      }
      const auto& small = *small_it;
      const auto intersections = find_all_intersections(small, params);
      const auto segments =
          split_circle_at_intersections(small, intersections);
      const double dist = std::hypot(small.center_x, small.center_y);
      const double click_x = small.center_x - petal_r * small.center_x / dist;
      const double click_y = small.center_y - petal_r * small.center_y / dist;
      const int hovered =
          select_clicked_segment(segments, small, click_x, click_y);
      if (!expect(hovered >= 0, "flower: inner petal segment hovered")) {
        return false;
      }
      document =
          manager.trim_sketch_entity(petal_id, click_x, click_y, hovered);
    }
  }

  // 2. Trim each big-circle notch at the petal angles. The big circle
  // converts to an arc after the first notch trim; later notches are
  // trimmed on that arc (which splits / shrinks).
  for (int k = 0; k < 10; ++k) {
    const double a = k * 36.0 * kPi / 180.0;
    const auto& params = sketch_params();
    // Find the big entity (radius 50) whose material contains angle a.
    std::string entity_id;
    double click_x = big_r * std::cos(a);
    double click_y = big_r * std::sin(a);
    bool is_circle = false;
    for (const auto& c : params.circles) {
      if (std::abs(c.radius - big_r) > 1e-6) continue;
      const double da =
          std::atan2(click_y - c.center_y, click_x - c.center_x);
      (void)da;
      entity_id = c.id;
      is_circle = true;
      break;
    }
    if (entity_id.empty()) {
      for (const auto& arc : params.arcs) {
        if (std::abs(arc.radius - big_r) > 1e-6) continue;
        if (!arc_contains_angle(arc, a)) continue;
        entity_id = arc.id;
        break;
      }
    }
    if (entity_id.empty()) {
      continue;  // this notch was already consumed by an earlier trim
    }
    int hovered = -1;
    if (is_circle) {
      const auto& big = [&]() -> const polysmith::core::SketchCircle& {
        for (const auto& c : params.circles) {
          if (c.id == entity_id) return c;
        }
        throw std::runtime_error("circle vanished");
      }();
      const auto intersections = find_all_intersections(big, params);
      const auto segments =
          split_circle_at_intersections(big, intersections);
      hovered = select_clicked_segment(segments, big, click_x, click_y);
    } else {
      const auto& big_arc = [&]() -> const SketchArc& {
        for (const auto& arc : params.arcs) {
          if (arc.id == entity_id) return arc;
        }
        throw std::runtime_error("arc vanished");
      }();
      const auto intersections = find_all_intersections(big_arc, params);
      const auto segments =
          split_arc_at_intersections(big_arc, intersections);
      hovered = select_clicked_segment(segments, big_arc, click_x, click_y);
    }
    if (!expect(hovered >= 0, "flower: big-circle notch hovered")) {
      return false;
    }
    document = manager.trim_sketch_entity(entity_id, click_x, click_y, hovered);
  }

  // 3. No circle entity may remain (all converted), and the profile
  // set must contain no full-circle region.
  const auto& params = sketch_params();
  {
    std::cerr << "  post-trim: circles=" << params.circles.size()
              << " arcs=" << params.arcs.size()
              << " profiles=" << params.profiles.size() << "\n";
    for (const auto& c : params.circles) {
      std::cerr << "    circle " << c.id << " r=" << c.radius << "\n";
    }
    for (const auto& arc : params.arcs) {
      const double sa =
          std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
      const double ea =
          std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
      double sweep = arc.ccw ? (ea - sa) : (sa - ea);
      sweep = std::fmod(std::fmod(sweep, 2.0 * kPi) + 2.0 * kPi, 2.0 * kPi);
      std::cerr << "    arc " << arc.id << " r=" << arc.radius
                << " ccw=" << arc.ccw << " sweep=" << sweep * 180.0 / kPi
                << "deg s=(" << arc.start_x << "," << arc.start_y << ") e=("
                << arc.end_x << "," << arc.end_y << ")\n";
    }
    for (const auto& profile : params.profiles) {
      std::cerr << "    profile kind=" << profile.kind << " ids=";
      for (const auto& id : profile.line_ids) std::cerr << id << " ";
      std::cerr << " src=" << (profile.source_circle_id.value_or("none"))
                << "\n";
    }
  }
  for (const auto& profile : params.profiles) {
    if (profile.kind == "circle" || profile.source_circle_id.has_value()) {
      return expect(false,
                    "flower: no full-circle profile may survive the trims");
    }
  }
  return true;
}

bool test_degenerate_arc_never_becomes_a_full_circle_profile() {
  // An arc whose endpoints coincide (a broken trim result) must never
  // be lifted into a 2π sweep by the profile walker: the user's
  // "trimmed to a half circle, detected as a full circle" symptom.
  // Before the guard, the coincident start/end lifted to a full
  // circle and the walker emitted a full-circle region for it.
  FeatureEntry feature = create_sketch_feature(2, "ref-plane-xy");
  add_sketch_arc(feature,
                 /*arc_index=*/1,
                 /*start_point_index=*/100,
                 /*end_point_index=*/101,
                 /*start_x=*/30.0,
                 /*start_y=*/40.0,
                 /*end_x=*/30.0,   // end == start
                 /*end_y=*/40.0,
                 /*center_x=*/0.0,
                 /*center_y=*/0.0,
                 /*radius=*/50.0,
                 /*ccw=*/true);

  feature.sketch_parameters->profiles =
      build_sketch_profile_regions(feature.sketch_parameters.value());

  for (const auto& profile : feature.sketch_parameters->profiles) {
    const bool uses_arc =
        std::find(profile.line_ids.begin(), profile.line_ids.end(), "arc-1") !=
        profile.line_ids.end();
    if (uses_arc || profile.kind == "circle") {
      return expect(false,
                    "degenerate arc: must not produce any profile region");
    }
  }
  return true;
}

}  // namespace

int main() {
  try {
    if (!test_long_line_crossing_near_endpoint_keeps_line()) return 1;
    if (!test_short_lines_at_angle_are_not_parallel()) return 1;
    if (!test_circle_tangent_single_intersection_keeps_circle_whole()) return 1;
    if (!test_construction_line_acts_as_cutting_edge()) return 1;
    if (!test_concentric_equal_radius_circles_no_nan()) return 1;
    if (!test_trim_split_point_exactly_on_intersection()) return 1;
    if (!test_trim_split_point_adopts_existing_vertex_id()) return 1;
    if (!test_rectangle_vertical_trim_leaves_two_closed_profiles()) return 1;
    if (!test_circle_trim_line_split_uses_noncolliding_ids()) return 1;
    if (!test_trim_survives_solver_pass()) return 1;
    if (!test_line_crossing_only_ellipse_keeps_line()) return 1;
    if (!test_line_crossing_only_spline_keeps_line()) return 1;
    if (!test_circle_crossing_only_ellipse_keeps_circle()) return 1;
    if (!test_line_ellipse_profile_closes()) return 1;
    if (!test_notch_trim_deletes_the_highlighted_segment()) return 1;
    if (!test_arc_distance_metric_beats_chord_for_long_segments()) return 1;
    if (!test_degenerate_arc_never_becomes_a_full_circle_profile()) return 1;
    if (!test_full_flower_trim_workflow()) return 1;
    std::cout << "trim_test passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
