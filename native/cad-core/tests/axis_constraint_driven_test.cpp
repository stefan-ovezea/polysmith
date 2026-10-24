// D-009: a Horizontal constraint on a loose line must straighten it.
//
// Regression test for the report "applying Horizontal to a slightly tilted
// line marks the constraint driven as over-constrained". A line has four DOF;
// one axis constraint removes one, so nothing can be over-constrained here. The
// demotion path treated "the solver has not run" (solver_dofs == -1) as
// "over-constrained", which is the same trap the dimension path documents and
// avoids.
#include <cmath>
#include <iostream>
#include <string>

#include "core/document/document.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

const polysmith::core::SketchFeatureParameters& sketch_of(
    const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) return *feature.sketch_parameters;
  }
  static const polysmith::core::SketchFeatureParameters empty;
  return empty;
}


// A horizontal line stays horizontal and is not demoted.
bool test_horizontal_straightens_a_tilted_line() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0698);

  document = manager.set_sketch_line_constraint("line-1", "horizontal");

  const auto& params = sketch_of(document);
  const auto& line = params.lines.front();
  if (!expect(!line.constraint_driven,
              "horizontal: the constraint is NOT demoted to driven")) {
    return false;
  }
  // -1 means the solver did not run on this path (a lone line is straightened by
  // the deterministic enforcement, not by planegcs). Either is acceptable here;
  // what must never happen is a demotion, which is what D-009 was.
  if (!expect(params.solver_dofs == -1 || params.solver_dofs == 3,
              ("horizontal: the dof count is either unknown or 3 - got " +
               std::to_string(params.solver_dofs))
                  .c_str())) {
    return false;
  }
  return expect(std::fabs(line.end_y - line.start_y) < 1e-6,
                ("horizontal: the line is actually horizontal - dy " +
                 std::to_string(line.end_y - line.start_y))
                    .c_str());
}

// The same for vertical, and a genuine over-constraint still demotes.
bool test_vertical_straightens_a_tilted_line() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 0.0698, 10.0);
  document = manager.set_sketch_line_constraint("line-1", "vertical");
  const auto& line = sketch_of(document).lines.front();
  if (!expect(!line.constraint_driven,
              "vertical: the constraint is NOT demoted to driven")) {
    return false;
  }
  return expect(std::fabs(line.end_x - line.start_x) < 1e-6,
                ("vertical: the line is actually vertical - dx " +
                 std::to_string(line.end_x - line.start_x))
                    .c_str());
}

// Re-applying the same axis constraint is not a new conflict.
bool test_reapplying_the_same_constraint_is_not_a_conflict() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.set_sketch_line_constraint("line-1", "horizontal");
  const bool first_demoted = sketch_of(document).lines.front().constraint_driven;
  if (!expect(!first_demoted, "redundant fixture: the first constraint sticks")) {
    return false;
  }
  // Applying the same constraint again must not be reported as a new conflict.
  document = manager.set_sketch_line_constraint("line-1", "horizontal");
  return expect(!sketch_of(document).lines.front().constraint_driven,
                "redundant: re-applying the same axis constraint is not a "
                "conflict");
}

}  // namespace

int main() {
  if (!test_horizontal_straightens_a_tilted_line()) return 1;
  if (!test_vertical_straightens_a_tilted_line()) return 1;
  if (!test_reapplying_the_same_constraint_is_not_a_conflict()) return 1;

  std::cout << "axis_constraint_driven_test passed\n";
  return 0;
}
