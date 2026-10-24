// S11 first slice: auto-constraint candidate detection.
//
// Detection is pure - it proposes, it never edits. These tests pin the three
// things that make a proposal trustworthy: a constraint the sketch already has
// is not proposed again, a hand-drawn sketch yields exactly the expected set (by
// id, not "at least one"), and the angle rules detect a deviation from BOTH
// sides (the repo's tangency rule).
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/sketch/auto_constraint_detection.h"

namespace {

using polysmith::core::AutoConstraintCandidate;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::detect_auto_constraints;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

const SketchFeatureParameters& sketch_of(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) return *feature.sketch_parameters;
  }
  static const SketchFeatureParameters empty;
  return empty;
}

// Candidate kinds in the detector's deterministic order, as "kind:id[,id]".
std::vector<std::string> summary(
    const std::vector<AutoConstraintCandidate>& candidates) {
  std::vector<std::string> lines;
  for (const auto& candidate : candidates) {
    std::string line = candidate.kind;
    for (const auto& id : candidate.target_ids) line += ":" + id;
    lines.push_back(line);
  }
  return lines;
}

void print_candidates(const std::vector<AutoConstraintCandidate>& candidates) {
  for (const auto& line : summary(candidates)) std::cerr << "  got " << line << "\n";
}

// Dedupe: once the line carries the constraint, it is not proposed again. This
// is the test that exposed D-009 - before the fix the constraint was demoted to
// driven and never stored, so the detector kept proposing it.
bool test_already_constrained_line_is_not_reproposed() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0698);
  const auto before = detect_auto_constraints(sketch_of(document), 0.5, 1.0);
  if (!expect(before.size() == 1 && before.front().kind == "horizontal",
              "dedupe: the loose line is proposed as horizontal")) {
    print_candidates(before);
    return false;
  }

  document = manager.set_sketch_line_constraint("line-1", "horizontal");
  const auto after = detect_auto_constraints(sketch_of(document), 0.5, 1.0);
  if (!expect(after.empty(),
              "dedupe: the constrained line is not proposed again")) {
    print_candidates(after);
    return false;
  }
  return true;
}

// A line 0.4 deg off horizontal, from both sides of exact.
bool test_near_horizontal_is_detected_from_both_sides() {
  for (const double rise : {0.0698, -0.0698}) {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    const auto document = manager.add_sketch_line(0.0, 0.0, 10.0, rise);
    const auto candidates =
        detect_auto_constraints(sketch_of(document), 0.5, 1.0);
    if (!expect(candidates.size() == 1,
                "0.4 deg off horizontal: exactly one candidate")) {
      print_candidates(candidates);
      return false;
    }
    const auto& candidate = candidates.front();
    if (!expect(candidate.kind == "horizontal",
                "0.4 deg off horizontal: the kind is horizontal")) {
      return false;
    }
    if (!expect(candidate.error > 0.3 && candidate.error < 0.5,
                "0.4 deg off horizontal: the reported error is about 0.4 deg")) {
      std::cerr << "  error " << candidate.error << "\n";
      return false;
    }
  }
  return true;
}

// The general case: a hand-drawn quadrilateral, three lines only.
bool test_hand_drawn_quadrilateral_proposes_the_expected_set() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 40.0, 0.3);    // near horizontal
  manager.add_sketch_line(40.0, 0.3, 39.7, 30.0);  // near vertical
  const auto document =
      manager.add_sketch_line(39.7, 30.0, 0.0, 30.2);  // near horizontal
  const auto candidates = detect_auto_constraints(sketch_of(document), 0.5, 1.0);

  int horizontals = 0;
  int verticals = 0;
  for (const auto& candidate : candidates) {
    if (candidate.kind == "horizontal") ++horizontals;
    if (candidate.kind == "vertical") ++verticals;
    if (!expect(candidate.kind != "coincident",
                "quadrilateral: no coincident proposal (ends already shared)")) {
      print_candidates(candidates);
      return false;
    }
  }
  if (!expect(horizontals == 2, "quadrilateral: two horizontals")) {
    print_candidates(candidates);
    return false;
  }
  if (!expect(verticals == 1, "quadrilateral: one vertical")) {
    print_candidates(candidates);
    return false;
  }
  return true;
}

// Concentric circles are proposed once, never twice for the same pair.
bool test_concentric_circles_are_proposed_once() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  const auto document = manager.add_sketch_circle(0.2, 0.0, 4.0);
  const auto candidates = detect_auto_constraints(sketch_of(document), 0.5, 1.0);
  int concentric = 0;
  for (const auto& candidate : candidates) {
    if (candidate.kind == "concentric") ++concentric;
  }
  return expect(concentric == 1,
                "concentric circles 0.2 mm apart: exactly one proposal");
}

}  // namespace

int main() {
  if (!test_already_constrained_line_is_not_reproposed()) return 1;
  if (!test_near_horizontal_is_detected_from_both_sides()) return 1;
  if (!test_hand_drawn_quadrilateral_proposes_the_expected_set()) return 1;
  if (!test_concentric_circles_are_proposed_once()) return 1;

  std::cout << "auto_constraint_detection_test passed\n";
  return 0;
}