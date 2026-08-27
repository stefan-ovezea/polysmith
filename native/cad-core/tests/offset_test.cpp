// Regression tests for the sketch offset tool (feature/sketch, SK4).
//
// offset_sketch_entity creates a NEW entity at a signed distance:
// line -> parallel line (positive = left of start->end), circle ->
// concentric circle radius + distance, arc -> same sweep angles at
// radius + distance (ccw preserved). Both distance signs must work,
// collapsing/inverting offsets must throw, and construction /
// generated / unsupported entities must be rejected.

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

bool test_line_offset_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);

  // +2 lands on the LEFT of start->end (+y).
  document = manager.offset_sketch_entity("line-1", 2.0);
  auto after = sketch_params(document);
  if (!expect(after.lines.size() == 2, "line offset: copy created")) {
    return false;
  }
  const auto& copy = after.lines[1];
  if (!expect(near(copy.start_x, 0.0) && near(copy.start_y, 2.0) &&
                  near(copy.end_x, 10.0) && near(copy.end_y, 2.0),
              "line offset: +2 lands left (y=2)")) {
    return false;
  }
  // The offset is a derived copy: no inferred constraint, no auto dim.
  if (!expect(!copy.constraint.has_value(),
              "line offset: no inferred constraint on the copy")) {
    return false;
  }

  // -2 lands on the right (-y).
  document = manager.offset_sketch_entity("line-1", -2.0);
  after = sketch_params(document);
  if (!expect(after.lines.size() == 3, "line offset: second copy created")) {
    return false;
  }
  const auto& copy2 = after.lines[2];
  return expect(near(copy2.start_y, -2.0) && near(copy2.end_y, -2.0),
                "line offset: -2 lands right (y=-2)");
}

bool test_circle_offset_both_sides_and_collapse() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);

  document = manager.offset_sketch_entity("circle-1", 2.0);
  auto after = sketch_params(document);
  if (!expect(after.circles.size() == 2 && near(after.circles[1].radius, 7.0) &&
                  near(after.circles[1].center_x, 10.0) &&
                  near(after.circles[1].center_y, 10.0),
              "circle offset: +2 grows radius to 7, concentric")) {
    return false;
  }

  document = manager.offset_sketch_entity("circle-1", -2.0);
  after = sketch_params(document);
  if (!expect(after.circles.size() == 3 && near(after.circles[2].radius, 3.0),
              "circle offset: -2 shrinks radius to 3")) {
    return false;
  }

  bool threw = false;
  try {
    (void)manager.offset_sketch_entity("circle-1", -6.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "circle offset: collapsing distance throws");
}

bool test_arc_offset_sweep_preserved() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Quarter arc: center (0,0), start (10,0), end (0,10), ccw.
  DocumentState document =
      manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  const auto before = sketch_params(document);
  if (!expect(before.arcs.size() == 1 && before.arcs[0].ccw,
              "arc offset: source arc is ccw")) {
    return false;
  }

  document = manager.offset_sketch_entity("arc-1", 2.0);
  auto after = sketch_params(document);
  if (!expect(after.arcs.size() == 2, "arc offset: copy created")) {
    return false;
  }
  const auto& copy = after.arcs[1];
  return expect(near(copy.radius, 12.0) &&
                    near(copy.start_x, 12.0) && near(copy.start_y, 0.0) &&
                    near(copy.end_x, 0.0) && near(copy.end_y, 12.0) &&
                    copy.ccw,
                "arc offset: +2 keeps the 90-degree ccw sweep");
}

bool test_arc_offset_negative_and_collapse() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");

  document = manager.offset_sketch_entity("arc-1", -2.0);
  auto after = sketch_params(document);
  if (!expect(after.arcs.size() == 2 && near(after.arcs[1].radius, 8.0) &&
                  near(after.arcs[1].start_x, 8.0) &&
                  near(after.arcs[1].end_y, 8.0),
              "arc offset: -2 shrinks the radius, sweep preserved")) {
    return false;
  }

  bool threw = false;
  try {
    (void)manager.offset_sketch_entity("arc-1", -12.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "arc offset: inverting distance throws");
}

bool test_construction_and_generated_rejected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_line(0.0, 0.0, 10.0, 0.0, /*is_construction=*/true);

  bool threw = false;
  try {
    (void)manager.offset_sketch_entity("line-1", 2.0);
  } catch (const std::exception&) {
    threw = true;
  }
  if (!expect(threw, "offset: construction line rejected")) {
    return false;
  }

  // Slot outlines are generated geometry — rejected too.
  document = manager.add_sketch_slot(0.0, 0.0, 10.0, 2.0, 0.0, false);
  const auto before = sketch_params(document);
  const auto generated = std::find_if(
      before.lines.begin(), before.lines.end(),
      [&](const auto& l) { return l.generated_by.has_value(); });
  if (!expect(generated != before.lines.end(),
              "offset: slot produced generated lines")) {
    return false;
  }
  threw = false;
  try {
    (void)manager.offset_sketch_entity(generated->id, 2.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "offset: generated entities rejected");
}

bool test_ellipse_rejected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_ellipse(
      10.0, 10.0, 20.0, 10.0, 10.0, 16.0, false);

  bool threw = false;
  try {
    (void)manager.offset_sketch_entity(
        sketch_params(document).ellipses[0].id, 2.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "offset: ellipses are not offsettable");
}

}  // namespace

int main() {
  if (!test_line_offset_both_sides()) return 1;
  if (!test_circle_offset_both_sides_and_collapse()) return 1;
  if (!test_arc_offset_sweep_preserved()) return 1;
  if (!test_arc_offset_negative_and_collapse()) return 1;
  if (!test_construction_and_generated_rejected()) return 1;
  if (!test_ellipse_rejected()) return 1;

  std::cout << "offset_test passed\n";
  return 0;
}
