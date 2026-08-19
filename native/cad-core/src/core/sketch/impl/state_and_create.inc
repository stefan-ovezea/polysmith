void refresh_sketch_derived_state(FeatureEntry& feature) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  // Drop anchors whose host line has been deleted.  Anchors whose
  // anchored point is no longer referenced by any live entity are
  // harmless (they are a no‑op during enforcement and the point is
  // dropped by `rebuild_sketch_vertices`), but cleaning them up here
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
                     [&](const SketchVertexLineAnchor& a) {
                       return std::none_of(
                           p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& l) { return l.id == a.line_id; });
                     }),
      p.point_line_anchors.end());

  // ── Text expansion ───────────────────────────────────────────
  // Re-derive glyph geometry from the parametric `texts` records FIRST,
  // before the zero-length cleanup, stale-dim scan, and solver run, so
  // every downstream pass sees current text lines. Generated entities
  // are always re-created here — never persisted between refreshes.
  refresh_sketch_texts(p);

  #include "core/sketch/impl/refresh_zero_length_line_cleanup.inc"

  // Re-anchor midpoint-bound points to their host line's current
  // midpoint before rebuilding the points list. The cascade may
  // shift other line endpoints which `rebuild_sketch_vertices` then
  // mirrors into the points vector.

  // --- Stale dimension cleanup ---
  // Runs BEFORE the constraints_exist check so orphaned dimensions
  // (referencing deleted entities) are marked driven before we decide
  // whether to run the solver.  Without this, an empty sketch with
  // orphaned driving dims would run the solver on 0 parameters and
  // incorrectly report dofs=0 ("fully constrained").
  //
  // IMPORTANT: check against live lines/circles, NOT the points
  // array.  rebuild_sketch_vertices hasn't run yet at this point,
  // so parameters.vertices still contains stale entries from
  // before the deletion.
  {
    auto point_is_alive = [&](const std::string& pid) -> bool {
      for (const auto& l : feature.sketch_parameters->lines) {
        if (l.start_vertex_id == pid || l.end_vertex_id == pid) return true;
      }
      // Phase 4.2: use geometry_owner_ids instead of prefix matching
      if (find_circle_for_point(*feature.sketch_parameters, pid) != nullptr) return true;
      if (find_arc_for_point(*feature.sketch_parameters, pid) != nullptr) return true;
      return false;
    };
    for (auto& dim : feature.sketch_parameters->dimensions) {
      if (dim.driven) continue;
      if (dim.kind == "line_length" || dim.kind == "line_angle") {
        bool found = std::any_of(
            feature.sketch_parameters->lines.begin(),
            feature.sketch_parameters->lines.end(),
            [&](const auto& l) { return l.id == dim.entity_id; });
        if (!found) dim.driven = true;
      } else if (dim.kind == "circle_radius") {
        bool found = std::any_of(
            feature.sketch_parameters->circles.begin(),
            feature.sketch_parameters->circles.end(),
            [&](const auto& c) { return c.id == dim.entity_id; });
        if (!found) dim.driven = true;
      } else if (dim.kind == "arc_radius") {
        bool found = std::any_of(
            feature.sketch_parameters->arcs.begin(),
            feature.sketch_parameters->arcs.end(),
            [&](const auto& a) { return a.id == dim.entity_id; });
        if (!found) dim.driven = true;
      } else if (dim.kind == "polygon_radius") {
        bool found = std::any_of(
            feature.sketch_parameters->polygons.begin(),
            feature.sketch_parameters->polygons.end(),
            [&](const auto& p) { return p.id == dim.entity_id; });
        if (!found) dim.driven = true;
      } else if (!dim.secondary_entity_id.empty()) {
        bool found_primary = false, found_secondary = false;
        if (dim.kind == "angle") {
          found_primary = std::any_of(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const auto& l) { return l.id == dim.entity_id; });
          found_secondary = std::any_of(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const auto& l) { return l.id == dim.secondary_entity_id; });
        } else if (dim.kind == "line_line_distance") {
          found_primary = std::any_of(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const auto& l) { return l.id == dim.entity_id; });
          found_secondary = std::any_of(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const auto& l) { return l.id == dim.secondary_entity_id; });
        } else if (dim.kind == "circle_line_distance") {
          found_primary = std::any_of(
              feature.sketch_parameters->circles.begin(),
              feature.sketch_parameters->circles.end(),
              [&](const auto& c) { return c.id == dim.entity_id; });
          found_secondary = std::any_of(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const auto& l) { return l.id == dim.secondary_entity_id; });
        } else {
          // point_distance, circle_center_distance
          found_primary = point_is_alive(dim.entity_id);
          found_secondary = point_is_alive(dim.secondary_entity_id);
        }
        if (!found_primary || !found_secondary) dim.driven = true;
      }
    }
  }

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
        saved_fixed.reserve(feature.sketch_parameters->vertices.size());
        for (const auto& pt : feature.sketch_parameters->vertices) {
          saved_fixed.push_back(pt.is_fixed);
        }

        // Build a lookup set of point IDs that belong to focus entities.
        std::unordered_set<std::string> unfrozen_point_ids;
        for (const auto& focus_id :
             feature.sketch_parameters->pending_append_focus_ids) {
          // Focus is a line → unfreeze both endpoints.
          for (const auto& line : feature.sketch_parameters->lines) {
            if (line.id == focus_id) {
              unfrozen_point_ids.insert(line.start_vertex_id);
              unfrozen_point_ids.insert(line.end_vertex_id);
            }
          }
          // Focus is a circle → unfreeze its center point.
          for (const auto& circle : feature.sketch_parameters->circles) {
            if (circle.id == focus_id) {
              unfrozen_point_ids.insert(circle_center_vertex_id(circle));
            }
          }
          // Focus is a point explicitly.
          unfrozen_point_ids.insert(focus_id);
        }

        // Freeze all points, then unfreeze only the focus set.
        for (auto& pt : feature.sketch_parameters->vertices) {
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
        // Snapshot pre-solve line coordinates so we can roll back
        // if the solver produces runaway geometry (a mathematically
        // valid solution that explodes the sketch into a degenerate
        // shape — e.g. flipping a dimension to the far side).
        struct LineSnapshot { double sx, sy, ex, ey; };
        std::vector<LineSnapshot> pre_snapshot;
        pre_snapshot.reserve(feature.sketch_parameters->lines.size());
        double pre_total_length = 0.0;
        for (const auto& line : feature.sketch_parameters->lines) {
          double dx = line.end_x - line.start_x;
          double dy = line.end_y - line.start_y;
          pre_total_length += std::sqrt(dx * dx + dy * dy);
          pre_snapshot.push_back(
              {line.start_x, line.start_y, line.end_x, line.end_y});
        }
        solver.apply(*feature.sketch_parameters);

        // Post-solve sanity checks.
        double post_total_length = 0.0;
        int zero_count = 0;
        std::string zero_ids;
        for (const auto& line : feature.sketch_parameters->lines) {
          double dx = line.end_x - line.start_x;
          double dy = line.end_y - line.start_y;
          double len = std::sqrt(dx * dx + dy * dy);
          post_total_length += len;
          if (len < 1e-6) {
            if (zero_count < 3) {
              if (!zero_ids.empty()) zero_ids += ", ";
              zero_ids += line.id;
            }
            zero_count++;
          }
        }
        if (zero_count > 0) {
          fprintf(stderr,
                  "ERROR solver produced %d zero-length line(s): %s%s\n",
                  zero_count, zero_ids.c_str(),
                  zero_count > 3 ? " ..." : "");
        }

        bool solver_exploded = false;
        if (pre_total_length > 1.0 &&
            (post_total_length > pre_total_length * 100.0 ||
             post_total_length < pre_total_length / 100.0)) {
          fprintf(stderr,
                  "ERROR solver exploded geometry — "
                  "total line length %.0f → %.0f mm (%.0fx change), "
                  "reverting to pre-solve positions\n",
                  pre_total_length, post_total_length,
                  post_total_length / pre_total_length);
          solver_exploded = true;
        }

        // Roll back to pre-solve coordinates when the solver diverges
        // into a mathematically-valid but geometrically-wrong solution.
        // The ad-hoc enforce functions below this block will still run
        // and provide a reasonable fallback.
        if (solver_exploded && pre_snapshot.size() ==
            feature.sketch_parameters->lines.size()) {
          for (size_t i = 0; i < pre_snapshot.size(); ++i) {
            feature.sketch_parameters->lines[i].start_x =
                pre_snapshot[i].sx;
            feature.sketch_parameters->lines[i].start_y =
                pre_snapshot[i].sy;
            feature.sketch_parameters->lines[i].end_x =
                pre_snapshot[i].ex;
            feature.sketch_parameters->lines[i].end_y =
                pre_snapshot[i].ey;
          }
        }

        // Clean up any zero-length lines the solver produced before
        // running the ad‑hoc enforce functions below (which throw on
        // zero‑length geometry — see drive_line_parallel_to_reference,
        // drive_line_perpendicular_to_reference).
        {
          std::vector<std::string> solver_zero_ids;
          for (const auto &line : feature.sketch_parameters->lines) {
            if (points_match(line.start_x, line.start_y,
                             line.end_x, line.end_y)) {
              solver_zero_ids.push_back(line.id);
            }
          }
          if (!solver_zero_ids.empty()) {
            for (const auto &zid : solver_zero_ids) {
              fprintf(stderr, "ERROR deleted solver-produced zero-length line %s\n",
                      zid.c_str());

              feature.sketch_parameters->constraints.erase(
                  std::remove_if(
                      feature.sketch_parameters->constraints.begin(),
                      feature.sketch_parameters->constraints.end(),
                      [&](const SketchConstraint &c) {
                        for (const auto &tid : c.target_ids) {
                          if (tid == zid) return true;
                        }
                        return false;
                      }),
                  feature.sketch_parameters->constraints.end());
              feature.sketch_parameters->dimensions.erase(
                  std::remove_if(
                      feature.sketch_parameters->dimensions.begin(),
                      feature.sketch_parameters->dimensions.end(),
                      [&](const SketchDimension &d) {
                        return d.entity_id == zid ||
                               d.secondary_entity_id == zid;
                      }),
                  feature.sketch_parameters->dimensions.end());
              feature.sketch_parameters->line_relations.erase(
                  std::remove_if(
                      feature.sketch_parameters->line_relations.begin(),
                      feature.sketch_parameters->line_relations.end(),
                      [&](const SketchLineRelation &rel) {
                        return rel.first_line_id == zid ||
                               rel.second_line_id == zid;
                      }),
                  feature.sketch_parameters->line_relations.end());
              feature.sketch_parameters->fillets.erase(
                  std::remove_if(
                      feature.sketch_parameters->fillets.begin(),
                      feature.sketch_parameters->fillets.end(),
                      [&](const SketchFillet &f) {
                        return f.line_a_id == zid ||
                               f.line_b_id == zid;
                      }),
                  feature.sketch_parameters->fillets.end());
              feature.sketch_parameters->midpoint_anchors.erase(
                  std::remove_if(
                      feature.sketch_parameters->midpoint_anchors.begin(),
                      feature.sketch_parameters->midpoint_anchors.end(),
                      [&](const SketchMidpointAnchor &a) {
                        return a.line_id == zid;
                      }),
                  feature.sketch_parameters->midpoint_anchors.end());
              feature.sketch_parameters->point_line_anchors.erase(
                  std::remove_if(
                      feature.sketch_parameters->point_line_anchors.begin(),
                      feature.sketch_parameters->point_line_anchors.end(),
                      [&](const SketchVertexLineAnchor &a) {
                        return a.line_id == zid;
                      }),
                  feature.sketch_parameters->point_line_anchors.end());
              feature.sketch_parameters->lines.erase(
                  std::remove_if(
                      feature.sketch_parameters->lines.begin(),
                      feature.sketch_parameters->lines.end(),
                      [&](const SketchLine &line) {
                        return line.id == zid;
                      }),
                  feature.sketch_parameters->lines.end());
            }
          }
        }
      }
      feature.sketch_parameters->solver_dofs = result.dofs;
      feature.sketch_parameters->solver_ok = result.ok();
      feature.sketch_parameters->solver_conflicting_count =
          static_cast<int>(result.conflicting.size());
      feature.sketch_parameters->solver_redundant_count =
          static_cast<int>(result.redundant.size());

      if (result.conflicting.size() > 0 || result.redundant.size() > 0 ||
          result.dofs < 0) {
        fprintf(stderr,
                "ERROR sketch over-constrained — dofs=%d conflicting=%zu redundant=%zu\n",
                result.dofs, result.conflicting.size(),
                result.redundant.size());
      }
      if (!result.ok()) {
        const char* reason =
            result.status == GCS::Failed ? "solver failed to converge" :
            result.status == GCS::SuccessfulSolutionInvalid
                ? "solution invalid (redundant/conflicting constraints)"
                : "unknown error";
        fprintf(stderr,
                "ERROR sketch solver failed — %s (status=%d, dofs=%d)\n",
                reason, static_cast<int>(result.status), result.dofs);
      }

      // Redundant constraint detection is handled by each dimension/
      // constraint creation command individually — the command that adds
      // a new constraint checks solver stats after refresh_sketch_derived_state
      // and marks its own new entry as driven when over-constrained.
      // This avoids fragile heuristics and works uniformly for all
      // dimension kinds (line_length, point_distance, angle, etc.).

      // Restore original is_fixed values.
      if (append_mode) {
        for (size_t i = 0; i < feature.sketch_parameters->vertices.size(); ++i) {
          feature.sketch_parameters->vertices[i].is_fixed = saved_fixed[i];
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
  try { enforce_all_equal_length_relations(*feature.sketch_parameters); }
  catch (const std::exception& e) {
    fprintf(stderr, "ERROR equal-length relation enforcement failed: %s\n", e.what());
  }
  try { enforce_all_perpendicular_relations(*feature.sketch_parameters); }
  catch (const std::exception& e) {
    fprintf(stderr, "ERROR perpendicular relation enforcement failed: %s\n", e.what());
  }
  try { enforce_all_parallel_relations(*feature.sketch_parameters); }
  catch (const std::exception& e) {
    fprintf(stderr, "ERROR parallel relation enforcement failed: %s\n", e.what());
  }

  // Fillets must run *after* the tangent passes (so they see the
  // latest line endpoints) and *before* `rebuild_sketch_vertices`.
  enforce_sketch_fillets(*feature.sketch_parameters);

  // Propagate line endpoint positions to any arcs that share those
  // point IDs.  Trim-created arcs share endpoints with lines; when a
  // dimension edit moves the line, the arc's cached endpoint coords
  // must stay in sync so the arc doesn't visually detach.
  for (auto& arc : feature.sketch_parameters->arcs) {
    for (const auto& line : feature.sketch_parameters->lines) {
      if (line.start_vertex_id == arc.start_vertex_id) {
        arc.start_x = line.start_x;
        arc.start_y = line.start_y;
      }
      if (line.end_vertex_id == arc.start_vertex_id) {
        arc.start_x = line.end_x;
        arc.start_y = line.end_y;
      }
      if (line.start_vertex_id == arc.end_vertex_id) {
        arc.end_x = line.start_x;
        arc.end_y = line.start_y;
      }
      if (line.end_vertex_id == arc.end_vertex_id) {
        arc.end_x = line.end_x;
        arc.end_y = line.end_y;
      }
    }
  }

  // Sync auto-dimensions after solver and enforce passes so displayed
  // values always match the actual geometry.
  sync_all_line_dimensions(*feature.sketch_parameters);
  sync_angle_dimensions(*feature.sketch_parameters);

  // Force all lines sharing a point ID to agree on its position before
  // we rebuild the deduplicated points list.  Without this pass,
  // constraint enforcement (H/V, relations) can leave two lines with
  // different coords for the same shared endpoint, causing orphaned
  // spheres and visual divergence.
  reconcile_shared_point_positions(*feature.sketch_parameters);

  #include "core/sketch/impl/refresh_mirror_relation_state.inc"

  const std::vector<SketchVertex> previous_points = feature.sketch_parameters->vertices;
  rebuild_sketch_vertices(*feature.sketch_parameters);

  // ── Phase 5: entity cached coords now mirror canonical vertices ──
  sync_entity_coords_from_vertices(*feature.sketch_parameters);

  // Derived points (projected, quadrant, arc endpoints) are derived
  // geometry — lock them by default so new derived points start as fixed.
  // Must run before sync_fixed_point_flags so the user's explicit
  // unfix (set_sketch_vertex_fixed) survives the refresh.
  enforce_derived_points_fixed(*feature.sketch_parameters);

  // Copy user-set is_fixed values from the previous frame. Must run
  // AFTER enforce_derived_points_fixed so an explicit unfix overrides
  // the default lock on derived points.
  sync_fixed_point_flags(*feature.sketch_parameters, previous_points);

  // Text glyph vertices are derived geometry owned by the text entity —
  // always fixed, regardless of what sync_fixed_point_flags carried
  // over. This runs LAST (authoritative) so the flag can never be lost
  // by the derived-point or user-flag passes above.
  for (auto& vertex : feature.sketch_parameters->vertices) {
    if (vertex.id.rfind("vertex-text-", 0) == 0) {
      vertex.is_fixed = true;
    }
  }

  // Driven (reference-only) dimensions: re-measure from current geometry
  // so their displayed values stay correct without driving anything.
  // MUST run after rebuild_sketch_vertices so point_distance driven dims
  // read fresh point positions, not stale ones from the previous frame.
  sync_driven_dimensions(*feature.sketch_parameters);
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
