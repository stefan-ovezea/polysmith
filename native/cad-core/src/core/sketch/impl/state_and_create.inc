void refresh_sketch_derived_state(FeatureEntry& feature) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  // Drop anchors whose host line has been deleted.  Anchors whose
  // anchored point is no longer referenced by any live entity are
  // harmless (they are a no‑op during enforcement and the point is
  // dropped by `rebuild_sketch_points`), but cleaning them up here
  // keeps the data model tidy and prevents stale viewport sprites.
  auto& p = *feature.sketch_parameters;
  p.midpoint_anchors.erase(
      std::remove_if(p.midpoint_anchors.begin(),
                     p.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& a) {
                       return std::none_of(
                           p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& l) { return l.id == a.line_id; });
                     }),
      p.midpoint_anchors.end());
  p.point_line_anchors.erase(
      std::remove_if(p.point_line_anchors.begin(),
                     p.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& a) {
                       return std::none_of(
                           p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& l) { return l.id == a.line_id; });
                     }),
      p.point_line_anchors.end());

  #include "core/sketch/impl/refresh_zero_length_line_cleanup.inc"

  // Re-anchor midpoint-bound points to their host line's current
  // midpoint before rebuilding the points list. The cascade may
  // shift other line endpoints which `rebuild_sketch_points` then
  // mirrors into the points vector.

  // --- planegcs constraint solver ---
  // Solve the geometric constraint system before running ad-hoc
  // enforce functions. If the solver succeeds, enforce functions
  // will see already-correct geometry and be no-ops. On failure,
  // the enforce functions provide a fallback.
  if (!feature.sketch_parameters->constraints.empty() ||
      !feature.sketch_parameters->line_relations.empty()) {
    ConstraintSolver solver;
    solver.build(*feature.sketch_parameters);
    auto result = solver.solve();
    if (result.ok()) {
      solver.apply(*feature.sketch_parameters);
    }
    feature.sketch_parameters->solver_dofs = result.dofs;
    if (!result.ok()) {
      fprintf(stderr, "constraint solver: status=%d dofs=%d conflicting=%zu redundant=%zu\n",
              static_cast<int>(result.status), result.dofs,
              result.conflicting.size(), result.redundant.size());
    }
  }

  // Anchor enforcement runs here — after the solver but before
  // constraint enforcement. Constraint enforcement (H/V, relations)
  // may need to adjust lines whose endpoints were moved by anchors,
  // so anchors must run first.
  enforce_midpoint_anchors(*feature.sketch_parameters);
  enforce_point_line_anchors(*feature.sketch_parameters);
  enforce_tangent_line_circle_relations(*feature.sketch_parameters);

  // Enforce stored constraints so badges always match geometry.
  // H/V runs first (axis alignment), then relations (inter-line).
  // Any constraint that would destroy geometry is cleared instead.
  enforce_line_HV_constraints(*feature.sketch_parameters);
  enforce_all_equal_length_relations(*feature.sketch_parameters);
  enforce_all_perpendicular_relations(*feature.sketch_parameters);
  enforce_all_parallel_relations(*feature.sketch_parameters);

  // Fillets must run *after* the tangent passes (so they see the
  // latest line endpoints) and *before* `rebuild_sketch_points`.
  enforce_sketch_fillets(*feature.sketch_parameters);

  // Sync auto-dimensions after solver and enforce passes so displayed
  // values always match the actual geometry.
  sync_all_line_dimensions(*feature.sketch_parameters);

  // Driven (reference-only) dimensions: re-measure from current geometry
  // so their displayed values stay correct without driving anything.
  sync_driven_dimensions(*feature.sketch_parameters);

  // Force all lines sharing a point ID to agree on its position before
  // we rebuild the deduplicated points list.  Without this pass,
  // constraint enforcement (H/V, relations) can leave two lines with
  // different coords for the same shared endpoint, causing orphaned
  // spheres and visual divergence.
  reconcile_shared_point_positions(*feature.sketch_parameters);

  #include "core/sketch/impl/refresh_mirror_relation_state.inc"

  const std::vector<SketchPoint> previous_points = feature.sketch_parameters->points;
  rebuild_sketch_points(*feature.sketch_parameters);
  sync_fixed_point_flags(*feature.sketch_parameters, previous_points);
  // Derived points (projected, quadrant) are derived geometry;
  // force them locked unconditionally even if `sync_fixed_point_flags`
  // happened to copy a transient false from the previous frame.
  enforce_derived_points_fixed(*feature.sketch_parameters);
  rebuild_sketch_profiles(*feature.sketch_parameters);
  feature.parameters_summary = make_parameters_summary(feature.sketch_parameters.value());
}

#include "core/sketch/impl/sketch_create_and_tool_commands.inc"
