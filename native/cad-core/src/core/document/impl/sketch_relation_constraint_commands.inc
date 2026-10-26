DocumentState DocumentManager::set_sketch_tangent_constraint(
    const std::string& line_id, const std::string& circle_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_tangent_constraint(
      *feature_it, line_id, circle_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_parallel_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_parallel_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_coincident_constraint(
    const std::string& point_id, const std::string& other_point_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_coincident_constraint(
      *feature_it, point_id, other_point_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = other_point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_coincident_constraint(
    const std::string& constraint_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();

  // Route mirror constraint deletes to the mirror-specific handler.
  const std::string mirror_prefix = "mirror-rel-";
  if (constraint_id.size() > mirror_prefix.size() &&
      constraint_id.substr(0, mirror_prefix.size()) == mirror_prefix) {
    polysmith::core::delete_sketch_mirror_relation(*feature_it, constraint_id);
  } else {
    polysmith::core::delete_sketch_coincident_constraint(*feature_it, constraint_id);
  }
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_symmetric_constraint(
    const std::string& point_id,
    const std::string& other_point_id,
    const std::string& axis_line_id) {
  require_document();
  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_symmetric_constraint(
      *feature_it, point_id, other_point_id, axis_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_midpoint_constraint(
    const std::string& vertex_id, const std::string& line_id) {
  require_document();
  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_midpoint_constraint(
      *feature_it, vertex_id, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_collinear_constraint(
    const std::string& line_id, const std::string& other_line_id) {
  require_document();
  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_collinear_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_tangent_pair_constraint(
    const std::string& first_id, const std::string& second_id) {
  require_document();
  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_tangent_pair_constraint(
      *feature_it, first_id, second_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

// S11: run the try_add pass on the active sketch as ONE undo step.
DocumentState DocumentManager::auto_constrain_sketch(double tolerance_mm,
                                                     double angle_tolerance_deg,
                                                     bool add_dimensions,
                                                     int* added_count) {
  require_document();
  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  // Run the pass on a COPY, then commit it only if it changed something.
  //
  // Two reasons. A run that adds nothing must not push an undo step (an empty
  // "Auto Constrain Sketch" entry is noise) and must not bump the revision. And
  // the undo step has to capture the sketch as it was BEFORE the pass: calling
  // push_undo_state() after mutating recorded the already-constrained state, so
  // "one undo restores it" silently did nothing (caught by the fully-constrain
  // test: undo left the pin in place).
  FeatureEntry probe = *feature_it;
  const AutoConstrainReport report = apply_auto_constraints(
      probe, tolerance_mm, angle_tolerance_deg, add_dimensions);
  if (report.added.empty() && report.dimensions_added == 0 &&
      report.vertices_fixed == 0) {
    if (added_count != nullptr) {
      *added_count = 0;
    }
    return document_.value();
  }
  push_undo_state();
  *feature_it = probe;
  clear_redo_stack();
  if (added_count != nullptr) {
    *added_count = static_cast<int>(report.added.size());
  }
  // Publish the measurement the pass actually made. `solver_dofs` from the
  // refresh path is only populated when that path happens to run the solver
  // (D-009), so leaving it stale would make the UI badge disagree with the
  // constraints that were just added. dofs_after < 0 means the solve failed;
  // in that case leave the field alone rather than inventing a number.
  if (report.dofs_after >= 0) {
    feature_it->sketch_parameters->solver_dofs = report.dofs_after;
    feature_it->sketch_parameters->solver_ok = true;
  }
  polysmith::core::log_info(
      "sketch", "auto-constrain: dofs " + std::to_string(report.dofs_before) +
                    " -> " + std::to_string(report.dofs_after) + ", added " +
                    std::to_string(report.added.size()) + ", rejected " +
                    std::to_string(report.rejected.size()) + ", skipped " +
                    std::to_string(report.skipped.size()) + ", dimensions " +
                    std::to_string(report.dimensions_added) + ", fixed " +
                    std::to_string(report.vertices_fixed));
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}