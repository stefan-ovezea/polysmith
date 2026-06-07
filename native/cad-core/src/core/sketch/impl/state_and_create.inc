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

  // Clean up zero‑length lines — a symptom of a constraint‑resolver
  // or trim bug. Collect their ids first, then erase the lines and
  // every piece of associated data (constraints, dimensions, relations,
  // anchors, fillets) so they don't keep re‑logging on every refresh.
  {
    std::vector<std::string> zero_ids;
    for (const auto& line : p.lines) {
      if (points_match(line.start_x, line.start_y, line.end_x, line.end_y)) {
        zero_ids.push_back(line.id);
      }
    }
    if (!zero_ids.empty()) {
      for (const auto& zid : zero_ids) {
        fprintf(stderr, "ERROR deleted zero-length line %s\n", zid.c_str());

        // Erase constraints that target this line or its endpoints.
        std::string zsp_id;
        std::string zep_id;
        for (const auto& line : p.lines) {
          if (line.id == zid) {
            zsp_id = line.start_point_id;
            zep_id = line.end_point_id;
            break;
          }
        }
        p.constraints.erase(
            std::remove_if(p.constraints.begin(), p.constraints.end(),
                           [&](const SketchConstraint& c) {
                             for (const auto& tid : c.target_ids) {
                               if (tid == zid || tid == zsp_id ||
                                   tid == zep_id)
                                 return true;
                             }
                             return false;
                           }),
            p.constraints.end());

        // Erase dimensions on this line.
        p.dimensions.erase(
            std::remove_if(p.dimensions.begin(), p.dimensions.end(),
                           [&](const SketchDimension& d) {
                             return d.entity_id == zid ||
                                    d.secondary_entity_id == zid;
                           }),
            p.dimensions.end());

        // Erase relations involving this line.
        p.line_relations.erase(
            std::remove_if(p.line_relations.begin(), p.line_relations.end(),
                           [&](const SketchLineRelation& rel) {
                             return rel.first_line_id == zid ||
                                    rel.second_line_id == zid;
                           }),
            p.line_relations.end());

        // Erase fillets that involve this line.
        p.fillets.erase(
            std::remove_if(p.fillets.begin(), p.fillets.end(),
                           [&](const SketchFillet& f) {
                             return f.line_a_id == zid ||
                                    f.line_b_id == zid;
                           }),
            p.fillets.end());

        // Erase anchors hosted on this line.
        p.midpoint_anchors.erase(
            std::remove_if(p.midpoint_anchors.begin(),
                           p.midpoint_anchors.end(),
                           [&](const SketchMidpointAnchor& a) {
                             return a.line_id == zid;
                           }),
            p.midpoint_anchors.end());
        p.point_line_anchors.erase(
            std::remove_if(p.point_line_anchors.begin(),
                           p.point_line_anchors.end(),
                           [&](const SketchPointLineAnchor& a) {
                             return a.line_id == zid;
                           }),
            p.point_line_anchors.end());

        // Erase the line itself.
        p.lines.erase(
            std::remove_if(p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& line) {
                             return line.id == zid;
                           }),
            p.lines.end());
      }
    }
  }

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

  // Refresh persistent mirror relations: re-reflect source entities
  // across their axis lines and update the mirror entity positions.
  // Also mark mirrored entity dimensions as driven so the user can't
  // edit them directly — they always follow the source.
  for (const auto& rel : feature.sketch_parameters->mirror_relations) {
    // Find the axis line.
    const auto axis_it = std::find_if(
        feature.sketch_parameters->lines.begin(),
        feature.sketch_parameters->lines.end(),
        [&](const SketchLine& l) { return l.id == rel.axis_line_id; });
    if (axis_it == feature.sketch_parameters->lines.end()) continue;
    const double ax = axis_it->start_x, ay = axis_it->start_y;
    const double bx = axis_it->end_x, by = axis_it->end_y;

    // Reflect a line source.
    auto line_src = std::find_if(
        feature.sketch_parameters->lines.begin(),
        feature.sketch_parameters->lines.end(),
        [&](const SketchLine& l) { return l.id == rel.source_id; });
    auto line_dst = std::find_if(
        feature.sketch_parameters->lines.begin(),
        feature.sketch_parameters->lines.end(),
        [&](const SketchLine& l) { return l.id == rel.mirror_id; });
    if (line_src != feature.sketch_parameters->lines.end() &&
        line_dst != feature.sketch_parameters->lines.end()) {
      const auto reflected_start = reflect_point_across_line(
          line_src->start_x, line_src->start_y, ax, ay, bx, by);
      const auto reflected_end = reflect_point_across_line(
          line_src->end_x, line_src->end_y, ax, ay, bx, by);
      line_dst->start_x = reflected_start.first;
      line_dst->start_y = reflected_start.second;
      line_dst->end_x = reflected_end.first;
      line_dst->end_y = reflected_end.second;
    }

    // Reflect a circle source.
    auto circle_src = std::find_if(
        feature.sketch_parameters->circles.begin(),
        feature.sketch_parameters->circles.end(),
        [&](const SketchCircle& c) { return c.id == rel.source_id; });
    auto circle_dst = std::find_if(
        feature.sketch_parameters->circles.begin(),
        feature.sketch_parameters->circles.end(),
        [&](const SketchCircle& c) { return c.id == rel.mirror_id; });
    if (circle_src != feature.sketch_parameters->circles.end() &&
        circle_dst != feature.sketch_parameters->circles.end()) {
      const auto reflected_center = reflect_point_across_line(
          circle_src->center_x, circle_src->center_y, ax, ay, bx, by);
      circle_dst->center_x = reflected_center.first;
      circle_dst->center_y = reflected_center.second;
      circle_dst->radius = circle_src->radius;
    }

    // Mark all dimensions on the mirror entity as driven so edits
    // are rejected — the value always comes from the source.
    for (auto& dim : feature.sketch_parameters->dimensions) {
      if (dim.entity_id == rel.mirror_id) {
        dim.driven = true;
      }
    }
  }

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

FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame) {
  SketchFeatureParameters parameters{
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .active_tool = "select",
      .lines = {},
      .circles = {},
      .points = {},
      .dimensions = {},
      .line_relations = {},
      .profiles = {},
  };

  FeatureEntry feature{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "sketch",
      .name = "Sketch",
      .status = "editing",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = parameters,
  };

  refresh_sketch_derived_state(feature);
  return feature;
}

void set_sketch_tool(FeatureEntry& feature, const std::string& tool) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can change sketch tools");
  }

  validate_tool(tool);
  feature.sketch_parameters->active_tool = tool;
}
