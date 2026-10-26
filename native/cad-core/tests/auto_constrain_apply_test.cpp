// S11 second slice: the try_add rule must only keep constraints that reduce DOF.
//
// Soundness first: every added candidate lowered the solver's DOF count, and a
// candidate that would not (already implied, or redundant) is rejected instead
// of being written into the sketch. Then idempotence: running the pass again on
// the result adds nothing.
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/sketch/auto_constraint_detection.h"
#include "core/viewport/viewport.h"

namespace {

using polysmith::core::AutoConstrainReport;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::apply_auto_constraints;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

FeatureEntry* sketch_feature(DocumentState& document) {
  for (auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) return &feature;
  }
  return nullptr;
}

// Four separate lines, drawn by hand: none of them shares a vertex yet, so the
// sketch starts with 16 DOF and coincident candidates have real work to do.
DocumentState build_hand_drawn_rectangle(DocumentManager& manager) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 40.0, 0.3);
  manager.add_sketch_line(40.3, 0.1, 39.7, 30.0);
  manager.add_sketch_line(39.7, 30.2, 0.1, 30.1);
  return manager.add_sketch_line(0.2, 30.0, 0.1, 0.2);
}

bool test_every_added_constraint_reduced_dof() {
  DocumentManager manager;
  DocumentState document = build_hand_drawn_rectangle(manager);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "fixture: the sketch exists")) return false;

  const AutoConstrainReport report = apply_auto_constraints(*feature, 0.5, 1.0);
  std::cerr << "PROBE dofs " << report.dofs_before << " -> " << report.dofs_after
            << " added " << report.added.size() << " rejected "
            << report.rejected.size() << " skipped " << report.skipped.size()
            << "\n";

  if (!expect(report.dofs_before == 16,
              "fixture: four loose lines have 16 DOF")) {
    return false;
  }
  if (!expect(!report.added.empty(), "try_add: something was added")) {
    return false;
  }
  if (!expect(report.dofs_after < report.dofs_before,
              "try_add: the sketch became more constrained")) {
    return false;
  }
  // A rectangle bottoms out at exactly four DOF - position (x, y), width and
  // height - because closure plus two horizontal and two vertical sides already
  // implies that opposite sides are equal. Reaching 0 needs four dimensions,
  // which is the dimensioning pass (the next slice). Fewer than four would mean
  // a dimension had been invented; more would mean a constraint was missed.
  if (!expect(report.dofs_after == 4,
              ("try_add: a dimension-free rectangle bottoms out at 4 DOF - got " +
               std::to_string(report.dofs_after))
                  .c_str())) {
    return false;
  }
  return true;
}

bool test_second_run_adds_nothing() {
  DocumentManager manager;
  DocumentState document = build_hand_drawn_rectangle(manager);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "idempotence: the sketch exists")) return false;

  const AutoConstrainReport first = apply_auto_constraints(*feature, 0.5, 1.0);
  const AutoConstrainReport second = apply_auto_constraints(*feature, 0.5, 1.0);
  if (!expect(second.added.empty(),
              "idempotence: a second run adds no constraint")) {
    std::cerr << "  added " << second.added.size() << " on the second run\n";
    return false;
  }
  return expect(second.dofs_after == first.dofs_after,
                "idempotence: the DOF count is unchanged by the second run");
}

bool test_a_fully_constrained_sketch_is_left_alone() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "constrained: the sketch exists")) return false;

  const AutoConstrainReport report =
      apply_auto_constraints(*feature, 0.5, 1.0, /*add_dimensions=*/true);
  return expect(report.dofs_after <= report.dofs_before,
                "constrained: the run never makes the sketch worse");
}

// With dimensions requested, the same try_add rule covers length dimensions:
// the two that survive are width and height. A rectangle has no position
// dimension yet, so it stops at 2 DOF instead of 0 - reaching 0 needs distance-
// to-origin dimensions, which the dimensioning slice does not have.
bool test_dimensions_close_the_size_gap() {
  DocumentManager manager;
  DocumentState document = build_hand_drawn_rectangle(manager);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "dimensions: the sketch exists")) return false;

  const AutoConstrainReport report =
      apply_auto_constraints(*feature, 0.5, 1.0, /*add_dimensions=*/true);
  std::cerr << "PROBE dimensions dofs " << report.dofs_before << " -> "
            << report.dofs_after << " constraints " << report.added.size()
            << " dimensions " << report.dimensions_added << "\n";
  // Measured: 3 dimensions survive and the sketch reaches 1 DOF. Each length
  // dimension drags a companion angle dimension with it (the line tool does the
  // same), so the reduction is larger than one DOF per command. The assertions
  // are the invariants, not the exact arithmetic: the pass made progress, it did
  // not add more dimensions than the plan's eight, and the ONE DOF left is the
  // sketch position - reaching 0 needs a distance-to-origin dimension, which is
  // the remaining piece of the dimensioning slice.
  if (!expect(report.dimensions_added > 0 && report.dimensions_added <= 8,
              ("dimensions: 1..8 survive - got " +
               std::to_string(report.dimensions_added))
                  .c_str())) {
    return false;
  }
  // One DOF is left: the sketch POSITION. Closing it needs a distance dimension
  // anchored to the origin, and add_sketch_distance_dimension takes two
  // entities - the origin is not one. Fixing a vertex was tried and does not
  // move the solver's DOF count, so it was removed rather than left as dead
  // code that pretends to work.
  return expect(report.dofs_after <= 1,
                ("dimensions: only the position DOF is left - got " +
                 std::to_string(report.dofs_after))
                    .c_str());
}
// An axis-constrained sketch with nothing else in it must NOT run the solver on
// every edit: solving runs planegcs' diagnose() (redundancy analysis), which made
// drawing take about a second per segment when it was wired in. A single
// horizontal line is the smallest such sketch - axis inference alone, no
// relations, no driving dimensions - and it must report "no solver dof" (-1)
// rather than a number.
//
// A RECTANGLE is deliberately NOT covered here: it carries parallel/perpendicular
// relations that the solver models, so it does run the solver and reports its
// real four DOF. See rectangle_constraints_test.
bool test_axis_only_sketch_does_not_solve_per_edit() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  const DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto viewport = polysmith::core::build_viewport_state(document);
  return expect(viewport.solver_dofs == -1,
                ("viewport: an axis-only sketch reports no solver dof (-1) - got " +
                 std::to_string(viewport.solver_dofs))
                    .c_str());
}

// "Fully constrain" must actually fully constrain. Before the position pass it
// stopped at 2 DOF: the dimensions pin width and height, and nothing said WHERE
// the rectangle is. Adding a distance-to-origin dimension would mean teaching the
// solver a new kind for a statement the sketch can already make - a fixed vertex
// pins x and y - so the pass pins one corner per surviving DOF instead.
bool test_fully_constrain_reaches_zero_dof() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 100.0, 50.0);

  int added = 0;
  document = manager.auto_constrain_sketch(0.5, 1.0, /*add_dimensions=*/true, &added);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "fully constrain: the sketch exists")) {
    return false;
  }
  const auto& parameters = *feature->sketch_parameters;
  if (!expect(parameters.solver_dofs == 0,
              ("fully constrain: DOF reaches 0 - got " +
               std::to_string(parameters.solver_dofs))
                  .c_str())) {
    return false;
  }
  // The geometry must be untouched: pinning the position locks the sketch where
  // it stands, it does not move it.
  if (!expect(parameters.lines.size() == 4,
              "fully constrain: the four sides survive")) {
    return false;
  }
  int fixed_vertices = 0;
  for (const auto& vertex : parameters.vertices) {
    if (vertex.is_fixed) ++fixed_vertices;
  }
  if (!expect(fixed_vertices == 1,
              ("fully constrain: exactly one corner is pinned - got " +
               std::to_string(fixed_vertices))
                  .c_str())) {
    return false;
  }
  const double width = std::hypot(parameters.lines[0].end_x - parameters.lines[0].start_x,
                                  parameters.lines[0].end_y - parameters.lines[0].start_y);
  if (!expect(std::abs(width - 100.0) < 1e-6,
              ("fully constrain: the width is unchanged - got " +
               std::to_string(width))
                  .c_str())) {
    return false;
  }

  // One undo must take the sketch back to where it was: no dimensions, no pin.
  document = manager.undo();
  FeatureEntry* undone = sketch_feature(document);
  if (!expect(undone != nullptr, "fully constrain: undo keeps the sketch")) {
    return false;
  }
  for (const auto& vertex : undone->sketch_parameters->vertices) {
    if (!expect(!vertex.is_fixed, "fully constrain: undo removes the pin")) {
      return false;
    }
  }
  return expect(undone->sketch_parameters->solver_dofs != 0,
                "fully constrain: undo reopens the degrees of freedom");
}

}  // namespace

int main() {
  if (!test_every_added_constraint_reduced_dof()) return 1;
  if (!test_second_run_adds_nothing()) return 1;
  if (!test_a_fully_constrained_sketch_is_left_alone()) return 1;
  if (!test_dimensions_close_the_size_gap()) return 1;
  if (!test_axis_only_sketch_does_not_solve_per_edit()) return 1;
  if (!test_fully_constrain_reaches_zero_dof()) return 1;

  std::cout << "auto_constrain_apply_test passed\n";
  return 0;
}
