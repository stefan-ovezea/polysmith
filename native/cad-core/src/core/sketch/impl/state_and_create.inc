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
  // Only run when constraint-like data actually exists — prevents
  // re-solving on unrelated geometry additions.
  {
    size_t driving_dims = 0;
    for (const auto& dim : feature.sketch_parameters->dimensions) {
      if (dim.driven) continue;
      if (dim.is_auto && dim.expression.empty()) continue;
      ++driving_dims;
    }
    const bool constraints_exist =
        !feature.sketch_parameters->constraints.empty() ||
        !feature.sketch_parameters->line_relations.empty() ||
        !feature.sketch_parameters->midpoint_anchors.empty() ||
        !feature.sketch_parameters->point_line_anchors.empty() ||
        driving_dims > 0;

    if (constraints_exist) {
      // --- Append-mode freeze state machine ---
      // When pending_append_focus_ids is non-empty, freeze all existing
      // points before the solve so only the focus entities can move.
      // This prevents "ghost movements" when adding new geometry or
      // constraints to an already-constrained sketch.
      const bool append_mode =
          !feature.sketch_parameters->pending_append_focus_ids.empty();

      // Snapshot original is_fixed values before any temporary mutation.
      std::vector<bool> saved_fixed;
      if (append_mode) {
        saved_fixed.reserve(feature.sketch_parameters->points.size());
        for (const auto& pt : feature.sketch_parameters->points) {
          saved_fixed.push_back(pt.is_fixed);
        }

        // Build a lookup set of point IDs that belong to focus entities.
        std::unordered_set<std::string> unfrozen_point_ids;
        for (const auto& focus_id :
             feature.sketch_parameters->pending_append_focus_ids) {
          // Focus is a line → unfreeze both endpoints.
          for (const auto& line : feature.sketch_parameters->lines) {
            if (line.id == focus_id) {
              unfrozen_point_ids.insert(line.start_point_id);
              unfrozen_point_ids.insert(line.end_point_id);
            }
          }
          // Focus is a circle → unfreeze its center point.
          for (const auto& circle : feature.sketch_parameters->circles) {
            if (circle.id == focus_id) {
              unfrozen_point_ids.insert(
                  "point-circle-" + circle.id + "-center");
            }
          }
          // Focus is a point explicitly.
          unfrozen_point_ids.insert(focus_id);
        }

        // Freeze all points, then unfreeze only the focus set.
        for (auto& pt : feature.sketch_parameters->points) {
          if (unfrozen_point_ids.count(pt.id)) {
            pt.is_fixed = false;
          } else {
            pt.is_fixed = true;
          }
        }
      }

      ConstraintSolver solver;
      solver.build(*feature.sketch_parameters);
      auto result = solver.solve();
      if (result.ok()) {
        solver.apply(*feature.sketch_parameters);
      }
      feature.sketch_parameters->solver_dofs = result.dofs;
      feature.sketch_parameters->solver_conflicting_count =
          static_cast<int>(result.conflicting.size());
      feature.sketch_parameters->solver_redundant_count =
          static_cast<int>(result.redundant.size());
      if (!result.ok()) {
        fprintf(stderr, "constraint solver: status=%d dofs=%d conflicting=%zu redundant=%zu\n",
                static_cast<int>(result.status), result.dofs,
                result.conflicting.size(), result.redundant.size());
      }

      // Restore original is_fixed values.
      if (append_mode) {
        for (size_t i = 0; i < feature.sketch_parameters->points.size(); ++i) {
          feature.sketch_parameters->points[i].is_fixed = saved_fixed[i];
        }
        feature.sketch_parameters->pending_append_focus_ids.clear();
      }
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

void refresh_sketch_with_append_focus(FeatureEntry& feature,
                                       const std::vector<std::string>& focus_ids) {
  if (feature.sketch_parameters.has_value()) {
    feature.sketch_parameters->pending_append_focus_ids = focus_ids;
    refresh_sketch_derived_state(feature);
  }
}

#include "core/sketch/impl/sketch_create_and_tool_commands.inc"
