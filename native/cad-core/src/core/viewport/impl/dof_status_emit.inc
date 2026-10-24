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
        // Measured at most ONCE per document revision: a viewport build happens
        // on every drag frame and every dimension keystroke, and a full solver
        // pass per build is what made editing crawl. The cache key covers what
        // can change the answer; a new revision invalidates it.
        if (solver_dofs < 0) {
          static std::string measured_key;
          static int measured_dofs = -1;
          // Keyed on STRUCTURE only. DOF depends on which constraints exist, not
          // on where the geometry currently is, so including geometry_revision
          // re-solved on every drag frame - which is what made drawing crawl.
          const std::string key =
              feat.id + ":" + std::to_string(feat.sketch_parameters->lines.size()) +
              ":" + std::to_string(feat.sketch_parameters->constraints.size()) +
              ":" + std::to_string(feat.sketch_parameters->dimensions.size()) +
              ":" + std::to_string(feat.sketch_parameters->line_relations.size());
          if (key != measured_key || measured_dofs < 0) {
            polysmith::core::ConstraintSolver solver;
            solver.build(feat.sketch_parameters.value());
            const auto measured = solver.solve();
            measured_key = key;
            measured_dofs = measured.ok() ? measured.dofs : -1;
          }
          if (measured_dofs >= 0) {
            solver_dofs = measured_dofs;
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
