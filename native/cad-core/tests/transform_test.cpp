// Regression tests for the sketch transform tool (feature/sketch, SK4).
//
// transform_sketch_entities applies translate/rotate/scale around a
// center, in place or as exploded copies (fresh ids, copies share
// vertices with each other but never with the originals). Scale
// keeps H/V constraints, scales circle/arc radii and ellipse/slot
// dimensions, flips circle/arc radius dims to driven, and
// re-measures line dimensions. move_sketch_entities remains the
// rigid wrapper.
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

bool test_scale_keeps_hv_and_scales_radii_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_circle(0.0, 5.0, 2.0);

  // Scale 2x about (0,0).
  document = manager.transform_sketch_entities(
      {"line-1", "circle-1"}, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, false);
  auto after = sketch_params(document);
  const auto& line = after.lines[0];
  const auto& circle = after.circles[0];
  if (!expect(near(line.start_x, 0.0) && near(line.end_x, 20.0) &&
                  line.constraint.has_value() &&
                  line.constraint.value() == "horizontal",
              "scale: H constraint survives 2x scale")) {
    return false;
  }
  if (!expect(near(circle.center_x, 0.0) && near(circle.center_y, 10.0) &&
                  near(circle.radius, 4.0),
              "scale: circle center and radius scale 2x")) {
    return false;
  }
  const auto radius_dim = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& d) { return d.kind == "circle_radius"; });
  if (!expect(radius_dim != after.dimensions.end() && radius_dim->driven,
              "scale: circle radius dim flips to driven")) {
    return false;
  }

  // Scale 0.5x about (0,0) — the other epsilon side.
  document = manager.transform_sketch_entities(
      {"line-1", "circle-1"}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, false);
  after = sketch_params(document);
  return expect(near(after.lines[0].end_x, 10.0) &&
                    near(after.circles[0].center_y, 5.0) &&
                    near(after.circles[0].radius, 2.0),
                "scale: 0.5x shrinks back to the original geometry");
}

bool test_rotate_copy_strips_hv_on_copies_only() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);

  document = manager.transform_sketch_entities(
      {"line-1"}, 0.0, 0.0, 0.0, 0.0, 90.0, 1.0, true);
  const auto after = sketch_params(document);
  if (!expect(after.lines.size() == 2, "rotate copy: second line created")) {
    return false;
  }
  const auto& original = after.lines[0];
  const auto& copy = after.lines[1];
  return expect(original.constraint.has_value() &&
                    original.constraint.value() == "horizontal" &&
                    near(original.end_x, 10.0) && near(original.end_y, 0.0) &&
                    !copy.constraint.has_value() &&
                    near(copy.start_x, 0.0) && near(copy.start_y, 0.0) &&
                    near(copy.end_x, 0.0) && near(copy.end_y, 10.0),
                "rotate copy: original keeps H, copy is vertical without H");
}

bool test_copy_circle_and_arc_scale() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);
  document = manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0,
                                    "center_start_end");

  document = manager.transform_sketch_entities(
      {"circle-1", "arc-1"}, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, true);
  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 2 && after.arcs.size() == 2,
              "copy: circle and arc copies exist")) {
    return false;
  }
  const auto& original_circle = after.circles[0];
  const auto& circle_copy = after.circles[1];
  const auto& original_arc = after.arcs[0];
  const auto& arc_copy = after.arcs[1];
  return expect(near(original_circle.radius, 5.0) &&
                    near(circle_copy.radius, 10.0) &&
                    near(circle_copy.center_x, 20.0) &&
                    near(original_arc.radius, 10.0) &&
                    near(arc_copy.radius, 20.0) &&
                    near(arc_copy.start_x, 20.0) && near(arc_copy.start_y, 0.0) &&
                    near(arc_copy.end_x, 0.0) && near(arc_copy.end_y, 20.0) &&
                    arc_copy.ccw == original_arc.ccw,
                "copy: circle and arc copies scale radii, originals untouched");
}

bool test_copy_rectangle_two_profiles_and_single_undo() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  auto before = sketch_params(document);
  std::vector<std::string> original_ids;
  for (const auto& line : before.lines) original_ids.push_back(line.id);

  // Copy the rectangle 20 mm to the right.
  document = manager.transform_sketch_entities(
      original_ids, 20.0, 0.0, 0.0, 0.0, 0.0, 1.0, true);
  const auto after = sketch_params(document);
  if (!expect(after.lines.size() == 8, "copy: 8 lines after rectangle copy")) {
    return false;
  }

  // Two polygon profiles: the original rectangle and the copy.
  std::vector<std::string> copy_ids;
  for (size_t i = 4; i < after.lines.size(); ++i) {
    copy_ids.push_back(after.lines[i].id);
  }
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = original_ids, .kind = "polygon"},
      {.entity_ids = copy_ids, .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("copy: exactly two profiles — " + reason).c_str())) {
    return false;
  }

  // One undo restores the single rectangle.
  document = manager.undo();
  const auto undone = sketch_params(document);
  return expect(undone.lines.size() == 4,
                "copy: a single undo removes the copy");
}

bool test_line_dim_remeasures_after_scale() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_line_length_dimension("line-1");

  document = manager.transform_sketch_entities(
      {"line-1"}, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, false);
  const auto after = sketch_params(document);
  if (!expect(near(after.lines[0].end_x, 20.0),
              "line dim: line scaled to 20")) {
    return false;
  }
  const auto dim_it = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& d) { return d.kind == "line_length"; });
  return expect(dim_it != after.dimensions.end() && near(dim_it->value, 20.0),
                "line dim: dimension re-measures to the scaled length");
}

bool test_move_wrapper_still_rigid() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.move_sketch_entities(
      {"line-1"}, 5.0, 5.0, 0.0, 0.0, 0.0);
  const auto& line = sketch_params(document).lines[0];
  return expect(near(line.start_x, 5.0) && near(line.start_y, 5.0) &&
                    near(line.end_x, 15.0) && near(line.end_y, 5.0),
                "move wrapper: rigid translate unchanged");
}

}  // namespace

int main() {
  if (!test_scale_keeps_hv_and_scales_radii_both_sides()) return 1;
  if (!test_rotate_copy_strips_hv_on_copies_only()) return 1;
  if (!test_copy_circle_and_arc_scale()) return 1;
  if (!test_copy_rectangle_two_profiles_and_single_undo()) return 1;
  if (!test_line_dim_remeasures_after_scale()) return 1;
  if (!test_move_wrapper_still_rigid()) return 1;

  std::cout << "transform_test passed\n";
  return 0;
}
