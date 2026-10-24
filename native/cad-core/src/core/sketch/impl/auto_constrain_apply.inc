// S11 second slice: apply auto-constraint candidates with the try_add rule.
//
// For each candidate: add it through the SAME setter a user click would use,
// measure the solver, keep it only if the DOF count actually DROPS, otherwise
// restore the whole parameter block and move on. One rejected candidate never
// abandons the run, and a candidate that does not reduce DOF never survives -
// which is what makes this safe to run on a sketch that is already partly
// constrained.
//
// The measurement is a direct solve, not `solver_dofs`: that field is only
// populated when the refresh path happens to run the solver (D-009 showed it can
// stay -1), and it must not be mistaken for "fully constrained".
#include "core/sketch/constraint_solver.h"

namespace {

// Solver DOF for a parameter set, or -1 when the solve failed / is unusable.
int measure_solver_dofs(const SketchFeatureParameters& parameters) {
  ConstraintSolver solver;
  solver.build(parameters);
  const auto result = solver.solve();
  if (!result.ok()) {
    return -1;
  }
  return result.dofs;
}

bool prepare_candidate(const AutoConstraintCandidate& candidate,
                       FeatureEntry& feature) {
  const auto& ids = candidate.target_ids;
  if (candidate.kind == "horizontal" || candidate.kind == "vertical") {
    if (ids.size() < 1) return false;
    set_sketch_line_constraint(feature, ids[0], candidate.kind);
    return true;
  }
  if (candidate.kind == "equal_length" || candidate.kind == "parallel" ||
      candidate.kind == "perpendicular") {
    if (ids.size() < 2) return false;
    if (candidate.kind == "equal_length") {
      set_sketch_equal_length_constraint(feature, ids[0], ids[1]);
    } else if (candidate.kind == "parallel") {
      set_sketch_parallel_constraint(feature, ids[0], ids[1]);
    } else {
      set_sketch_perpendicular_constraint(feature, ids[0], ids[1]);
    }
    return true;
  }
  if (candidate.kind == "coincident") {
    if (ids.size() < 2) return false;
    set_sketch_coincident_constraint(feature, ids[0], ids[1]);
    return true;
  }
  // concentric and anything added later: detection may propose it, the apply
  // pass does not know how to drive it yet - report it as skipped rather than
  // pretend it was added.
  return false;
}

}  // namespace

AutoConstrainReport apply_auto_constraints(FeatureEntry& feature,
                                           double tolerance_mm,
                                           double angle_tolerance_deg) {
  AutoConstrainReport report;
  if (!feature.sketch_parameters.has_value()) {
    return report;
  }
  report.dofs_before = measure_solver_dofs(feature.sketch_parameters.value());
  int dofs = report.dofs_before;
  if (dofs < 0) {
    // Without a solver measurement the try_add rule cannot be applied, and
    // "apply everything and hope" is exactly what this rule exists to prevent.
    return report;
  }

  const auto candidates =
      detect_auto_constraints(feature.sketch_parameters.value(), tolerance_mm,
                              angle_tolerance_deg);
  for (const auto& candidate : candidates) {
    if (dofs == 0) break;

    // Snapshot the ENTIRE parameter block: a setter also rebuilds vertices,
    // profiles and dimensions, so restoring only the constraint lists would
    // leave derived state from a rejected candidate behind.
    const SketchFeatureParameters before = feature.sketch_parameters.value();
    if (!prepare_candidate(candidate, feature)) {
      feature.sketch_parameters = before;
      report.skipped.push_back(candidate);
      continue;
    }

    const int next = measure_solver_dofs(feature.sketch_parameters.value());
    const bool progressed = next >= 0 && next < dofs;
    if (!progressed) {
      feature.sketch_parameters = before;
      refresh_sketch_derived_state(feature);
      report.rejected.push_back(candidate);
      continue;
    }
    dofs = next;
    report.added.push_back(candidate);
  }

  if (!report.added.empty()) {
    refresh_sketch_derived_state(feature);
  }
  report.dofs_after = dofs;
  return report;
}