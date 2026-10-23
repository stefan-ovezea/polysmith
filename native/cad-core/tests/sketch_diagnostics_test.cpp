// Regression tests for the loose-end diagnostics (S5).
//
// "Sketch profile not found" is the most common extrude refusal, and the user
// gets no hint about WHICH ends are wrong. compute_sketch_diagnostics names
// them from the vertex table the profile walk already builds, so it can never
// disagree with the walk about what is connected.
#include <cmath>
#include <iostream>
#include <string>

#include "core/document/document.h"`n#include "core/sketch/sketch_feature.h"

namespace {

using polysmith::core::SketchDiagnostics;
using polysmith::core::compute_sketch_diagnostics;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-9) {
  return std::abs(a - b) < tolerance;
}

// A closed rectangle: every corner carries two owners, so nothing is open.
bool test_closed_rectangle_has_no_loose_ends() {
  polysmith::core::DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  const auto document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);
  const SketchDiagnostics diagnostics = compute_sketch_diagnostics(
      document.feature_history.back().sketch_parameters.value());
  return expect(diagnostics.open.empty(),
                "closed rectangle: no open ends") &&
         expect(diagnostics.untied.empty(),
                "closed rectangle: nothing untied");
}

// One line: two open ends, but they are far apart so nothing is untied.
bool test_single_line_is_open_but_not_untied() {
  polysmith::core::DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  const auto document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  const SketchDiagnostics diagnostics = compute_sketch_diagnostics(
      document.feature_history.back().sketch_parameters.value());
  return expect(diagnostics.open.size() == 2,
                "single line: both ends are open") &&
         expect(diagnostics.untied.empty(),
                "single line: its own ends are not an untied pair");
}

// The acceptance case from the plan: two lines 0.02 mm apart look joined.
bool test_two_lines_point_zero_two_mm_apart_are_untied() {
  polysmith::core::DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  const auto document = manager.add_sketch_line(10.02, 0.0, 20.0, 0.0);

  const SketchDiagnostics diagnostics = compute_sketch_diagnostics(
      document.feature_history.back().sketch_parameters.value());
  if (!expect(diagnostics.untied.size() == 1,
              "0.02 mm gap: exactly one untied pair")) {
    return false;
  }
  const auto& pair = diagnostics.untied.front();
  if (!expect(near(pair.gap, 0.02, 1e-6),
              ("0.02 mm gap: the reported gap is the real one - got " +
               std::to_string(pair.gap))
                  .c_str())) {
    return false;
  }
  return expect(diagnostics.open.size() == 4,
                "0.02 mm gap: all four ends are open (two chains)");
}

// A gap wider than the tolerance is not reported as untied.
bool test_wide_gap_is_not_untied() {
  polysmith::core::DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  const auto document = manager.add_sketch_line(12.0, 0.0, 20.0, 0.0);
  const SketchDiagnostics diagnostics = compute_sketch_diagnostics(
      document.feature_history.back().sketch_parameters.value());
  return expect(diagnostics.untied.empty(),
                "2 mm gap: not reported as untied (the user can see it)");
}

}  // namespace

int main() {
  if (!test_closed_rectangle_has_no_loose_ends()) return 1;
  if (!test_single_line_is_open_but_not_untied()) return 1;
  if (!test_two_lines_point_zero_two_mm_apart_are_untied()) return 1;
  if (!test_wide_gap_is_not_untied()) return 1;

  std::cout << "sketch_diagnostics_test passed\n";
  return 0;
}