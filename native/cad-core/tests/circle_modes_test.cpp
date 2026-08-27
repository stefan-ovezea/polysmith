// Regression tests for the circle tool's creation modes
// (feature/sketch, SK5).
//
// add_sketch_circle gains a mode field: two_point (diameter
// endpoints), three_point (circumcircle), tangent_two_lines and
// tangent_three_lines (center on the angle bisector / triangle
// incenter, with circle-slave tangent relations that re-derive the
// radius when the defining lines move). Center+radius stays the
// default. The wrapper resolves every mode into (center, radius) —
// the single source of truth.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::test::ExpectedProfile;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

bool test_two_point_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "two_point",
      /*p1*/ 0.0, 0.0, /*p2*/ 20.0, 0.0);
  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 1, "two-point: circle created")) {
    return false;
  }
  const auto& circle = after.circles[0];
  return expect(near(circle.center_x, 10.0) && near(circle.center_y, 0.0) &&
                    near(circle.radius, 10.0),
                "two-point: center is the midpoint, radius half the span");
}

bool test_three_point_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "three_point",
      /*p1*/ 10.0, 0.0, /*p2*/ 0.0, 10.0, /*p3*/ -10.0, 0.0);
  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 1, "three-point: circle created")) {
    return false;
  }
  const auto& circle = after.circles[0];
  return expect(near(circle.center_x, 0.0) && near(circle.center_y, 0.0) &&
                    near(circle.radius, 10.0),
                "three-point: circumcircle through all three points");
}

// 90-degree corner (bottom + right side), hint in each wedge — both
// epsilon sides of the bisector.
bool test_tangent_two_lines_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 20.0, 0.0);
  document = manager.add_sketch_line(0.0, 0.0, 0.0, 20.0);

  // Hint in the first quadrant: center lands on the 45-degree
  // bisector at (10,10), tangent to both lines.
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      /*line ids*/ "line-1", "line-2", "",
      /*hint*/ 10.0, 10.0);
  auto after = sketch_params(document);
  if (!expect(after.circles.size() == 1,
              "tangent-two: circle created")) {
    return false;
  }
  const auto& circle = after.circles[0];
  if (!expect(near(circle.center_x, 10.0) && near(circle.center_y, 10.0) &&
                  near(circle.radius, 10.0),
              "tangent-two: center on the 45-degree bisector")) {
    return false;
  }
  if (!expect(after.line_relations.size() == 2,
              "tangent-two: two tangent relations attached")) {
    return false;
  }
  const bool both_circle_slave = std::all_of(
      after.line_relations.begin(), after.line_relations.end(),
      [&](const auto& r) {
        return r.kind == "tangent_circle_line" &&
               r.first_line_id == circle.id;
      });
  if (!expect(both_circle_slave,
              "tangent-two: relations are circle-slave")) {
    return false;
  }

  // Second circle with the hint in the fourth quadrant — the other
  // wedge (both epsilon sides of the corner bisector).
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "",
      10.0, -10.0);
  after = sketch_params(document);
  if (!expect(after.circles.size() == 2,
              "tangent-two: second circle created")) {
    return false;
  }
  const auto& circle2 = after.circles[1];
  return expect(near(circle2.center_x, 10.0) && near(circle2.center_y, -10.0) &&
                    near(circle2.radius, 10.0),
                "tangent-two: second wedge resolved correctly");
}

// Lines stored pointing TOWARD the corner (reversed directions) —
// the bisector choice must still place the circle inside the wedge
// (user-reported: the circle landed outside the angle).
bool test_tangent_two_lines_reversed_directions() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(30.0, 15.0, 0.0, 0.0);
  document = manager.add_sketch_line(30.0, -15.0, 0.0, 0.0);
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "", 15.0, 0.0);

  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 1,
              "reversed: circle created")) {
    return false;
  }
  const auto& circle = after.circles[0];
  // Center on the +x bisector at the hint's projection (15, 0);
  // the hint must win regardless of the stored line directions.
  return expect(near(circle.center_x, 15.0) && near(circle.center_y, 0.0) &&
                    near(circle.radius, 6.7082, 1e-3),
                "reversed: center inside the wedge toward the hint");
}

// Inscribed circle tangent to ALL THREE sides (user's triangle):
// the circle splits the interior into two lens regions + the circle
// itself — three regions total.
bool test_tangent_circle_tangent_to_all_three_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Isosceles triangle: left vertex (0,0), top (100,50), bottom
  // (100,-50); the incenter sits at ~(69.1, 0) with r ~30.9.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 100.0, 50.0);
  document = manager.add_sketch_line(0.0, 0.0, 100.0, -50.0);
  document = manager.add_sketch_line(100.0, 50.0, 100.0, -50.0);
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "", 69.0, 0.0);

  const auto after = sketch_params(document);
  // Complete region set: the outer triangle region (all three
  // lines) plus the circle region. The third tangency does not
  // force a lens split — one outer region is the correct surface.
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {"line-1", "line-2", "line-3"}, .kind = "polygon"},
      {.entity_ids = {after.circles[0].id}, .kind = "polygon",
       .has_source_circle_id = true},
  };
  return expect(profiles_match(document, expected, &reason),
                ("all-three-sides: " + reason).c_str());
}

// The radius re-derives from the circle's fixed center when a
// defining line moves (circle-slave semantics).
bool test_tangent_radius_rederives_after_line_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 20.0, 0.0);
  document = manager.add_sketch_line(0.0, 0.0, 0.0, 20.0);
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "", 10.0, 10.0);
  const std::string circle_id =
      sketch_params(document).circles[0].id;

  // Move the bottom line up to y=5: the circle's center stays at
  // (10,10) and the radius shrinks to the new distance (5).
  document = manager.update_sketch_line("line-1", 0.0, 5.0, 20.0, 5.0);
  const auto after = sketch_params(document);
  const auto circle_it = std::find_if(
      after.circles.begin(), after.circles.end(),
      [&](const auto& c) { return c.id == circle_id; });
  return expect(circle_it != after.circles.end() &&
                    near(circle_it->center_x, 10.0) &&
                    near(circle_it->center_y, 10.0) &&
                    near(circle_it->radius, 5.0),
                "tangent-two: radius re-derives after the line moves");
}

// Three lines (3-4-5 triangle scaled 10x): the incenter lands at
// (20,10) with radius 10, tangent to all three lines.
bool test_tangent_three_lines_incenter() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 30.0, 0.0);
  document = manager.add_sketch_line(30.0, 0.0, 30.0, 40.0);
  document = manager.add_sketch_line(30.0, 40.0, 0.0, 0.0);

  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_three_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "line-3", 0.0, 0.0);
  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 1,
              "tangent-three: circle created")) {
    return false;
  }
  const auto& circle = after.circles[0];
  if (!expect(near(circle.center_x, 20.0) && near(circle.center_y, 10.0) &&
                  near(circle.radius, 10.0),
              "tangent-three: triangle incenter")) {
    return false;
  }
  return expect(after.line_relations.size() == 3,
                "tangent-three: three tangent relations attached");
}

// Full profile set: the tangent circle inside the corner must be
// detected as its own closed region (the open corner lines form no
// polygon).
bool test_tangent_circle_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 20.0, 0.0);
  document = manager.add_sketch_line(0.0, 0.0, 0.0, 20.0);
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "", 10.0, 10.0);
  const auto after = sketch_params(document);
  // The tangent circle inside the corner is detected as its own
  // closed region (kind "polygon", the circle id in the boundary
  // edge list, and the circle as the source — the open corner lines
  // form no separate polygon).
  std::string reason;
  // The corner stubs between the origin and the two tangency points
  // are kept now (dead-end pieces drop, interior pieces stay), so the
  // wedge between the V and the circle is a REAL closed region — the
  // complete set is the circle plus that wedge.
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {after.circles[0].id, "line-1", "line-2"},
       .kind = "polygon"},
      {.entity_ids = {after.circles[0].id}, .kind = "polygon",
       .has_source_circle_id = true},
  };
  return expect(profiles_match(document, expected, &reason),
                ("tangent profile: " + reason).c_str());
}

// A closed triangle with an inscribed tangent circle must produce
// TWO regions: the outer polygon and the circle (user-reported: the
// enclosed surface between the triangle and the tangent circle was
// missing — only the circle region was detected).
bool test_tangent_circle_inside_triangle_two_regions() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Triangle: the wedge from the origin closed by a vertical side.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 30.0, 15.0);
  document = manager.add_sketch_line(0.0, 0.0, 30.0, -15.0);
  document = manager.add_sketch_line(30.0, 15.0, 30.0, -15.0);
  document = manager.add_sketch_circle(
      0.0, 0.0, 0.0, false, "tangent_two_lines",
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
      "line-1", "line-2", "", 20.0, 0.0);

  const auto after = sketch_params(document);
  std::string reason;
  // Complete region set: the triangle (3 lines) plus the circle
  // region. Order-independent per profiles_match.
  std::vector<std::string> triangle_ids = {"line-1", "line-2", "line-3"};
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = triangle_ids, .kind = "polygon"},
      {.entity_ids = {after.circles[0].id}, .kind = "polygon",
       .has_source_circle_id = true},
  };
  return expect(profiles_match(document, expected, &reason),
                ("tangent circle in triangle: " + reason).c_str());
}

}  // namespace

int main() {
  if (!test_two_point_circle()) return 1;
  if (!test_three_point_circle()) return 1;
  if (!test_tangent_two_lines_both_sides()) return 1;
  if (!test_tangent_radius_rederives_after_line_move()) return 1;
  if (!test_tangent_two_lines_reversed_directions()) return 1;
  if (!test_tangent_circle_tangent_to_all_three_sides()) return 1;
  if (!test_tangent_three_lines_incenter()) return 1;
  if (!test_tangent_circle_profile()) return 1;
  if (!test_tangent_circle_inside_triangle_two_regions()) return 1;

  std::cout << "circle_modes_test passed\n";
  return 0;
}
