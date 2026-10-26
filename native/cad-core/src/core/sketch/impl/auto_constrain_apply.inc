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
                                           double angle_tolerance_deg,
                                           bool add_dimensions) {
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

  // ── Dimensioning pass ────────────────────────────────────────
  // Same try_add rule as the constraints above: a dimension is kept only if the
  // solver's DOF drops, so a dimension that the existing constraints already
  // imply (a rectangle's opposite side, once equal_length holds) is rolled back
  // instead of pinning a value the user never asked for.
  if (add_dimensions && dofs > 0 && feature.sketch_parameters.has_value()) {
    const auto& lines = feature.sketch_parameters->lines;
    for (std::size_t index = 0; index < lines.size() && dofs > 0; ++index) {
      const std::string line_id = lines[index].id;
      const SketchFeatureParameters before = feature.sketch_parameters.value();
      add_sketch_line_length_dimension(feature, line_id);
      const int next = measure_solver_dofs(feature.sketch_parameters.value());
      if (next >= 0 && next < dofs) {
        dofs = next;
        ++report.dimensions_added;
      } else {
        feature.sketch_parameters = before;
        refresh_sketch_derived_state(feature);
      }
    }
  }

  // ── Position pass ────────────────────────────────────────────
  // Whatever DOF survives the dimensions is the sketch's POSITION: nothing has
  // anchored it to the plane, so x/y are free. Dimensions cannot close that (a
  // width does not say where the rectangle is), and adding a distance-to-origin
  // dimension would mean teaching the solver a new kind for a statement the
  // sketch can already make - a fixed vertex pins x AND y.
  //
  // So pin one corner per surviving DOF. Measured: a 100x50 rectangle goes
  // 4 DOF -> 2 (width, height) -> 0 with one fixed corner, with no new solver
  // plumbing at all. The pin is a first-class constraint the user can see (FIX
  // badge) and delete, which is why this is honest rather than magic: "Fully
  // constrain" anchors the sketch where it stands.
  if (add_dimensions && dofs > 0 && feature.sketch_parameters.has_value()) {
    for (const auto& vertex :
         std::vector<SketchVertex>(feature.sketch_parameters->vertices)) {
      if (dofs <= 0) break;
      if (vertex.is_fixed) continue;
      const SketchFeatureParameters before = feature.sketch_parameters.value();
      set_sketch_vertex_fixed(feature, vertex.id, true);
      const int next = measure_solver_dofs(feature.sketch_parameters.value());
      if (next >= 0 && next < dofs) {
        dofs = next;
        ++report.vertices_fixed;
      } else {
        feature.sketch_parameters = before;
        refresh_sketch_derived_state(feature);
      }
    }
  }
  if (!report.added.empty() || report.dimensions_added > 0 ||
      report.vertices_fixed > 0) {
    refresh_sketch_derived_state(feature);
  }
  report.dofs_after = dofs;
  return report;
}