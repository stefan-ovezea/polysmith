// S11 first slice: auto-constraint candidate DETECTION.
//
// Pure and read-only: it proposes constraints, it never edits the sketch. That
// is deliberate - the dangerous half of auto-constrain is APPLYING candidates,
// so the rules are built and tested where a bug can only produce a wrong
// suggestion, never moved geometry.
//
// Application (the try_add rule: add one candidate, re-solve, keep it only if
// the SOLVER's dof count drops, otherwise roll that one back) is the next slice;
// see wiki/Sketch-Auto-Constrain-Plan.md.
#pragma once

#include <string>
#include <vector>

#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct AutoConstraintCandidate {
  // Setter vocabulary: "horizontal" / "vertical" (taken by
  // set_sketch_line_constraint), "equal_length", "coincident", "concentric",
  // "parallel", "perpendicular".
  std::string kind;
  // In the order the matching setter expects: one line id; two line ids; two
  // vertex ids; or two curve ids.
  std::vector<std::string> target_ids;
  // How far from exact the candidate is: degrees for the angle rules,
  // millimetres for the distance rules. Reported so the UI can rank proposals.
  double error = 0.0;
  // Human-readable explanation for the log ("line is 0.4 deg off horizontal").
  std::string reason;
};

// Proposes constraints for the parameters, deterministically ordered (by kind,
// then by target ids) so tests and previews are stable. tolerance_mm bounds the
// distance rules and angle_tolerance_deg the angle rules. A constraint the
// sketch already carries is NOT proposed again - including the axis constraints
// stored on the line itself (SketchLine::constraint), which is where
// set_sketch_line_constraint puts them.
std::vector<AutoConstraintCandidate> detect_auto_constraints(
    const SketchFeatureParameters& parameters, double tolerance_mm,
    double angle_tolerance_deg);

// S11 second slice: the result of applying candidates with the try_add rule.
struct AutoConstrainReport {
  std::vector<AutoConstraintCandidate> added;
  std::vector<AutoConstraintCandidate> rejected;  // would not reduce DOF
  std::vector<AutoConstraintCandidate> skipped;   // no setter yet (concentric)
  int dimensions_added = 0;
  int anchored_points = 0;
  int dofs_before = -1;
  int dofs_after = -1;
};

// Adds candidates one at a time, keeping each only if the solver's DOF count
// drops (see the .inc). The caller owns the undo step.
AutoConstrainReport apply_auto_constraints(FeatureEntry& feature,
                                           double tolerance_mm,
                                           double angle_tolerance_deg,
                                           bool add_dimensions = false);

}  // namespace polysmith::core