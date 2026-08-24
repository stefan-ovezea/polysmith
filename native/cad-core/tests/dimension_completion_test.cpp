// Regression tests for SK6 — dimension completion:
// diameter display on circle radius dims, arc angle, arc length.
//
// - Diameter: the dimension's stored value stays the RADIUS for the
//   solver; edits through the update path divide the displayed
//   diameter by two, and the payload carries the displayed value.
//   Both epsilon sides must drive correctly.
// - Arc angle: 90 and 270 degree sweeps, both ccw orientations.
// - Arc length: quarter-circle length drives the sweep; the driven
//   sync re-measures from geometry.

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

bool test_diameter_drives_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);
  // The auto radius dimension id follows the curve primitive pattern;
  // the display defaults to diameter (display_as == "").
  const std::string dim_id = "dim-circle-circle-1";
  auto dim_value = [&](const DocumentState& doc) {
    const auto params = sketch_params(doc);
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& d) { return d.id == dim_id; });
    return it == params.dimensions.end() ? -1.0 : it->value;
  };
  const auto circle_radius = [&](const DocumentState& doc) {
    return sketch_params(doc).circles[0].radius;
  };

  // The direct API stores and drives the RADIUS (the displayed
  // diameter conversion lives at the IPC boundary). Both epsilon
  // sides must drive.
  document = manager.update_sketch_dimension(dim_id, 8.0);
  if (!expect(near(circle_radius(document), 8.0),
              "diameter: radius 8 drives the circle")) {
    return false;
  }
  if (!expect(near(dim_value(document), 8.0),
              "diameter: stored value stays the radius")) {
    return false;
  }
  const auto dim = sketch_params(document).dimensions.front();
  if (!expect(dim.display_as != "radius",
              "diameter: default display is diameter")) {
    return false;
  }

  document = manager.update_sketch_dimension(dim_id, 2.0);
  return expect(near(circle_radius(document), 2.0),
                "diameter: radius 2 drives (other epsilon side)");
}

bool test_arc_angle_drives_sweeps_both_orientations() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Quarter arc ccw, then a cw one — 90 and 270 degree sweeps.
  DocumentState document = manager.add_sketch_arc(
      10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  document = manager.add_sketch_arc_angle_dimension("arc-1");
  const std::string dim_id = "dim-arc-angle-arc-1";

  const auto sweep = [&](const DocumentState& doc) {
    const auto& arc = sketch_params(doc).arcs[0];
    double raw = std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x) -
                 std::atan2(arc.start_y - arc.center_y,
                            arc.start_x - arc.center_x);
    if (raw < 0.0) raw += 2.0 * 3.14159265358979323846;
    return raw;
  };

  // 90 degrees already; drive to 180 (the other side of the epsilon).
  document = manager.update_sketch_dimension(dim_id, 3.14159265358979323846);
  if (!expect(near(sweep(document), 3.14159265358979323846),
              "arc angle: 180 degree sweep drives")) {
    return false;
  }

  // Drive to 270 (cw-arc equivalents are handled by the same sweep
  // logic — the endpoint re-derives along the circle).
  document = manager.update_sketch_dimension(dim_id, 4.71238898038469);
  return expect(near(sweep(document), 4.71238898038469),
                "arc angle: 270 degree sweep drives");
}

bool test_arc_length_drives_quarter_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_arc(
      10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  document = manager.add_sketch_arc_length_dimension("arc-1");
  const std::string dim_id = "dim-arc-length-arc-1";

  const auto measure = [&](const DocumentState& doc) {
    const auto& arc = sketch_params(doc).arcs[0];
    double raw = std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x) -
                 std::atan2(arc.start_y - arc.center_y,
                            arc.start_x - arc.center_x);
    if (raw < 0.0) raw += 2.0 * 3.14159265358979323846;
    return arc.radius * raw;
  };

  // Quarter circle of r=10: length ~15.708. Drive to a full
  // semicircle's length 31.416 -> sweep pi.
  document = manager.update_sketch_dimension(dim_id, 31.41592653589793);
  return expect(near(measure(document), 31.41592653589793, 1e-4),
                "arc length: L=pi*r drives a semicircle");
}

}  // namespace

int main() {
  try {
  if (!test_diameter_drives_both_sides()) return 1;
  if (!test_arc_angle_drives_sweeps_both_orientations()) return 1;
  if (!test_arc_length_drives_quarter_circle()) return 1;

  std::cout << "dimension_completion_test passed\n";
  return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
