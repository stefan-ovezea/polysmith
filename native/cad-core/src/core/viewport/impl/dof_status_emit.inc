  // Populate DOF statuses for the active sketch.
  int solver_dofs = -1;
  int solver_conflicting_count = -1;
  int solver_redundant_count = -1;
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        dof_statuses = count_sketch_dof(feat.sketch_parameters.value());
        solver_dofs = feat.sketch_parameters->solver_dofs;
        solver_conflicting_count =
            feat.sketch_parameters->solver_conflicting_count;
        solver_redundant_count =
            feat.sketch_parameters->solver_redundant_count;
        break;
      }
    }
  }
