// Regression tests for the sketch array tools (feature/sketch, SK4).
//
// create_linear_array / create_circular_array commit exploded copies
// (count - 1 copies through the transform copy path): fresh unique
// ids, vertex topology preserved among copies, originals untouched,
// a single undo removes the whole array. v1 is direct-commit — the
// pending preview workflow is deferred.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;

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

bool test_linear_array_three_copies() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(0.0, 0.0, 2.0);

  document = manager.create_linear_array({"circle-1"}, 10.0, 0.0, 3);
  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 3, "linear array: 3 circles")) {
    return false;
  }
  // Original stays; copies land at +10 and +20 along x.
  if (!expect(near(after.circles[0].center_x, 0.0) &&
                  near(after.circles[1].center_x, 10.0) &&
                  near(after.circles[2].center_x, 20.0) &&
                  near(after.circles[0].radius, 2.0) &&
                  near(after.circles[1].radius, 2.0) &&
                  near(after.circles[2].radius, 2.0),
              "linear array: copies land at +10/+20 with equal radii")) {
    return false;
  }
  // Unique ids across the array.
  std::set<std::string> ids;
  for (const auto& circle : after.circles) ids.insert(circle.id);
  return expect(ids.size() == 3, "linear array: unique circle ids");
}

bool test_circular_array_six_on_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Small circle offset from the array center so rotation is visible.
  DocumentState document = manager.add_sketch_circle(20.0, 0.0, 2.0);

  // 6 copies over a full 360-degree sweep around (0,0).
  document = manager.create_circular_array({"circle-1"}, 0.0, 0.0, 6, 360.0);
  const auto after = sketch_params(document);
  if (!expect(after.circles.size() == 6, "circular array: 6 circles")) {
    return false;
  }
  // Every copy stays exactly 20 mm from the array center (on-circle
  // 1e-6), with equal radii.
  for (const auto& circle : after.circles) {
    if (!expect(near(std::hypot(circle.center_x, circle.center_y), 20.0) &&
                    near(circle.radius, 2.0),
                "circular array: copy lies on the 20mm ring")) {
      return false;
    }
  }
  // Fusion-style spacing: 6 instances including the original at
  // 60 degrees per step (360 / 6) — the last copy stops at 300 so
  // it never overlaps the source at angle 0.
  const auto& second = after.circles[1];
  const double step = 360.0 / 6.0;
  if (!expect(near(second.center_x, 20.0 * std::cos(step * M_PI / 180.0)) &&
                  near(second.center_y, 20.0 * std::sin(step * M_PI / 180.0)),
              "circular array: 60-degree spacing")) {
    return false;
  }
  // No copy may coincide with the original's position.
  for (size_t i = 1; i < after.circles.size(); ++i) {
    const double dist = std::hypot(after.circles[i].center_x - 20.0,
                                   after.circles[i].center_y);
    if (!expect(dist > 1.0e-6,
                "circular array: no copy overlaps the source")) {
      return false;
    }
  }
  return true;
}

bool test_array_undo_restores_and_ids_unique() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 5.0, 0.0);

  document = manager.create_linear_array({"line-1"}, 0.0, 10.0, 4);
  auto after = sketch_params(document);
  if (!expect(after.lines.size() == 4, "array undo: 4 lines before undo")) {
    return false;
  }
  std::set<std::string> ids;
  for (const auto& line : after.lines) ids.insert(line.id);
  if (!expect(ids.size() == 4, "array undo: unique line ids")) {
    return false;
  }

  // A single undo removes the whole array.
  document = manager.undo();
  const auto undone = sketch_params(document);
  return expect(undone.lines.size() == 1,
                "array undo: single undo removes all copies");
}

}  // namespace

int main() {
  if (!test_linear_array_three_copies()) return 1;
  if (!test_circular_array_six_on_circle()) return 1;
  if (!test_array_undo_restores_and_ids_unique()) return 1;

  std::cout << "array_test passed\n";
  return 0;
}
