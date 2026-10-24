  // Populate DOF statuses for the active sketch.
  // (see the measurement note below)
  int solver_dofs = -1;
  int solver_conflicting_count = -1;
  int solver_redundant_count = -1;
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        dof_statuses = count_sketch_dof(feat.sketch_parameters.value());
        solver_dofs = feat.sketch_parameters->solver_dofs;
        // The refresh path only fills solver_dofs when it happens to run the
        // solver, so a sketch whose constraints are all axis/relation kinds can
        // still read -1. The badge prefers solver_dofs and falls back to summing
        // the per-entity heuristic - which ignores closure and reported a
        // freshly drawn rectangle as 12 DOF when the solver says 4. Measure it
        // here instead of guessing: one solve on a read-only path is cheap next
        // to showing a number that is wrong by eight.
        if (solver_dofs < 0) {
          polysmith::core::ConstraintSolver solver;
          solver.build(feat.sketch_parameters.value());
          const auto measured = solver.solve();
          if (measured.ok()) {
            solver_dofs = measured.dofs;
          }
        }
        solver_conflicting_count =
            feat.sketch_parameters->solver_conflicting_count;
        solver_redundant_count =
            feat.sketch_parameters->solver_redundant_count;
        break;
      }
    }
  }
