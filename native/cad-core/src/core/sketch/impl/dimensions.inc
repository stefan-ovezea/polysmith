// --- Shared dimension finalization ------------------------------------------
// Moved here from sketch_dimension_create_commands.inc so they are visible
// before dimension_distance_commands.inc and dimension_angle_commands.inc.


// Every dimension creation command MUST call this after adding a new dimension.
// It runs the solver, checks for over-constraint, and marks the new dimension
// as driven when appropriate.  The optional point-pair provides a fallback
// check for types that constrain a point pair (line_length, point_distance,
// circle_center_distance, etc.).
//
// Returns true if the dimension was marked driven.
static bool finalize_new_dimension(
    FeatureEntry& feature,
    SketchFeatureParameters& parameters,
    const std::string& dimension_id,
    std::optional<std::string> pair_a = std::nullopt,
    std::optional<std::string> pair_b = std::nullopt) {

  refresh_sketch_derived_state(feature);

  auto mark_driven = [&](const char* reason) {
    auto it = std::find_if(parameters.dimensions.begin(),
                           parameters.dimensions.end(),
        [&](SketchDimension& d) { return d.id == dimension_id; });
    if (it == parameters.dimensions.end()) return false;
    it->driven = true;
    polysmith::core::log_info(
        "sketch", "dimension " + it->id + " marked driven (" + reason + ")");
    refresh_sketch_derived_state(feature);
    return true;
  };

  // 1) Solver-reported over-constraint or failure to converge.
  // With DogLeg as the default solver, failure is a strong signal —
  // DogLeg rarely fails on under-constrained sketches.
  //
  // solver_dofs == -1 means the solver hasn't run yet (default);
  // it must not be mistaken for over-constrained.
  bool solver_ran = feature.sketch_parameters->solver_dofs != -1;
  if (solver_ran &&
      (feature.sketch_parameters->solver_conflicting_count > 0 ||
       feature.sketch_parameters->solver_redundant_count > 0 ||
       feature.sketch_parameters->solver_dofs < 0 ||
       !feature.sketch_parameters->solver_ok)) {
    return mark_driven("solver reported over-constrained");
  }

  // 2) The old point-pair heuristic lived here: ">2 driving constraints on a
  // point pair means one is redundant", counting a pair as 2 DOF. That is wrong
  // for the pairs these dimensions actually constrain - two POINTS have four
  // DOF (two coordinates each), so a line with a horizontal constraint, a length
  // and an angle is three constraints on four DOF, not over-constrained. The
  // symptom was a perfectly valid dimension being marked driven ("dimension
  // dim-line-line-1 marked driven (>2 constraints on point pair)") whenever it
  // was edited on a fully constrained sketch.
  //
  // The solver already answers this question with real numbers - conflicting and
  // redundant counts after a solve - so the heuristic is gone rather than
  // retuned: one source of truth, and it is the one that can see the whole
  // system instead of a single pair.
  (void)pair_a;
  (void)pair_b;

  return false;
}

#include "core/sketch/impl/dimension_update.inc"
#include "core/sketch/impl/dimension_expression_reify.inc"
#include "core/sketch/impl/dimension_angle_commands.inc"
#include "core/sketch/impl/dimension_distance_commands.inc"