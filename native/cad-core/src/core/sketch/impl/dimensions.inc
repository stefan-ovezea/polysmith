// --- Shared dimension finalization ------------------------------------------
// Moved here from sketch_dimension_create_commands.inc so they are visible
// before dimension_distance_commands.inc and dimension_angle_commands.inc.

// Count driving (non-auto, non-driven) constraints on the given sorted
// point-id pair.  2 points have only 2 relative DOF (dx, dy), so >2
// constraints means at least one is redundant.
// Counts both dimensions AND line H/V constraints.
static int count_driving_on_point_pair(
    const SketchFeatureParameters& parameters,
    const std::string& pair_a, const std::string& pair_b) {
  int count = 0;
  // 1) Dimensions that constrain a point pair.
  for (const auto& dim : parameters.dimensions) {
    if (dim.driven) continue;
    if (dim.is_auto && dim.expression.empty()) continue;
    std::string pa, pb;
    if (dim.kind == "line_length") {
      auto lit = std::find_if(parameters.lines.begin(), parameters.lines.end(),
          [&](const SketchLine& l) { return l.id == dim.entity_id; });
      if (lit == parameters.lines.end()) continue;
      pa = lit->start_vertex_id;
      pb = lit->end_vertex_id;
    } else if (!dim.secondary_entity_id.empty()) {
      pa = dim.entity_id;
      pb = dim.secondary_entity_id;
    } else {
      continue;
    }
    if (pa > pb) std::swap(pa, pb);
    if (pa == pair_a && pb == pair_b) count++;
  }
  // 2) Line H/V constraints.  Each constrains one relative DOF (dx or dy)
  //    of the line's endpoint pair.
  for (const auto& line : parameters.lines) {
    if (!line.constraint.has_value()) continue;
    if (line.constraint_driven) continue;
    std::string pa = line.start_vertex_id, pb = line.end_vertex_id;
    if (pa > pb) std::swap(pa, pb);
    if (pa == pair_a && pb == pair_b) count++;
  }
  return count;
}

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
    fprintf(stderr, "[INFO] sketch: dimension %s marked driven (%s)\n",
            it->id.c_str(), reason);
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

  // 2) Point-pair fallback: >2 driving constraints on 2 DOF → redundant.
  if (pair_a.has_value() && pair_b.has_value()) {
    std::string pa = *pair_a, pb = *pair_b;
    if (pa > pb) std::swap(pa, pb);
    if (count_driving_on_point_pair(parameters, pa, pb) > 2) {
      return mark_driven(">2 constraints on point pair");
    }
  }

  return false;
}

#include "core/sketch/impl/dimension_update.inc"
#include "core/sketch/impl/dimension_expression_reify.inc"
#include "core/sketch/impl/dimension_angle_commands.inc"
#include "core/sketch/impl/dimension_distance_commands.inc"