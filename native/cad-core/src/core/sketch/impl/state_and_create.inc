void refresh_sketch_derived_state(FeatureEntry& feature) {
  // ── Flight recorder ────────────────────────────────────────────────
  // A refresh can move geometry in a dozen places (solver, anchors, H/V, the
  // relation passes, fillets, chamfers, point reconciliation, vertex rebuild).
  // Every one of them is silent on success, so when a sketch "explodes" the user
  // has nothing to report — that is literally what "exploded but nothing is in
  // the log" means. This watches the whole refresh and, when the geometry
  // changes drastically in ONE pass, names the vertices that moved and the
  // constraint inventory, so a single occurrence is diagnosable.
  struct RecorderVertex {
    std::string id;
    double x;
    double y;
  };
  std::vector<RecorderVertex> recorder_before;
  double recorder_length_before = 0.0;
  if (feature.sketch_parameters.has_value()) {
    const auto& params = *feature.sketch_parameters;
    recorder_before.reserve(params.vertices.size());
    for (const auto& vertex : params.vertices) {
      recorder_before.push_back({vertex.id, vertex.x, vertex.y});
    }
    for (const auto& line : params.lines) {
      recorder_length_before +=
          std::hypot(line.end_x - line.start_x, line.end_y - line.start_y);
    }
  }
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

  // ── Text + slot expansion ────────────────────────────────────
  // Re-derive glyph geometry from the parametric `texts` records FIRST,
  // before the zero-length cleanup, stale-dim scan, and solver run, so
  // every downstream pass sees current text lines. Generated entities
  // are always re-created here — never persisted between refreshes.
  // Slot expansion runs immediately after: the text pass strips ALL
  // generated entities (any generated_by value), then the slot pass
  // strips only its own "slot:"-generated entities and re-pushes them.
  refresh_sketch_texts(p);
  refresh_sketch_slots(p);

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
      // Slot centers are standalone vertices — they have no owning
      // line/circle/arc, so check the slot records directly.
      for (const auto& s : feature.sketch_parameters->slots) {
        if (s.center_vertex_id == pid) return true;
      }
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
      } else if (dim.kind == "arc_radius" || dim.kind == "arc_angle") {
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
      // Arc dimensions are enforced deterministically by
      // enforce_arc_dimensions (ad-hoc), not through planegcs — see
      // the pass for why (solver null-space drift on unanchored arcs).
      if (dim.kind == "arc_radius" || dim.kind == "arc_angle") continue;
      ++driving_dims;
    }
    // Do NOT count per-line axis constraints here. Making them count meant every
    // edit of an axis-constrained sketch ran a solve WITH diagnose() (planegcs'
    // redundancy analysis, which re-solves subsystems per candidate) - and an
    // axis-constrained sketch is exactly what the rectangle and line tools
    // produce. That turned drawing from instant into about a second per segment.
    // The DOF badge reads the heuristic for such sketches instead; giving it a
    // real number is S3's job (a cheap rank, not a diagnose per edit).
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

      // Anchored points are solver unknowns: a point anchored to a host line's
      // midpoint carries that midpoint as a constraint, so the solver expects it
      // to already BE there. Any ad-hoc drive (a dimension edit, a relation
      // command) moves the host and leaves the anchored point behind, and the
      // solver then has to absorb that residual — which it does by moving things
      // that did not need to move. Measured: shrinking a rectangle from 20 to 10
      // tall left the anchored centre line at its old height, and the solver
      // "resolved" the 10 mm mismatch by translating the whole rectangle up by
      // 5.76 mm. Pulling the anchors onto their hosts first leaves it with
      // nothing to fix.
      enforce_midpoint_anchors(*feature.sketch_parameters);
      enforce_point_line_anchors(*feature.sketch_parameters);

      const auto solve_start = std::chrono::steady_clock::now();
      ConstraintSolver solver;
      solver.build(*feature.sketch_parameters);
      if (sketch_solve_trace_enabled()) {
        trace_sketch_geometry("before solve", *feature.sketch_parameters);
      }
      // Capture what the solver is allowed to touch before it runs: geometry
      // that no constraint can reach must come back untouched (see
      // solver_touch_guard.inc — DogLeg drags unconstrained lines around).
      const SolverGeometrySnapshot solver_snapshot =
          capture_solver_geometry(*feature.sketch_parameters);
      auto result = solver.solve();
      const auto solve_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - solve_start)
              .count();
      if (solve_ms >= 100) {
        // Timing evidence, not chatter: the solver's diagnose() is the expensive
        // part and it grows with the constraint count, so a sketch that feels
        // slow per edit will show its cost here.
        polysmith::core::log_warn(
            "sketch", "solve took " + std::to_string(solve_ms) + " ms for " +
                          std::to_string(feature.sketch_parameters->lines.size()) +
                          " lines, " +
                          std::to_string(
                              feature.sketch_parameters->constraints.size()) +
                          " constraints, " +
                          std::to_string(
                              feature.sketch_parameters->dimensions.size()) +
                          " dimensions");
      }
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
        if (sketch_solve_trace_enabled()) {
          trace_sketch_geometry("after solve", *feature.sketch_parameters);
        }
        // Put back geometry the solver had no business moving. This is the
        // "one dimension destroyed my sketch" guard: a driven dimension on one
        // entity makes DogLeg drag unrelated loose lines (measured: two free
        // lines, one collapsed, the other jumped to the origin).
        {
          const double restored_max = restore_unreachable_solver_geometry(
              *feature.sketch_parameters, solver_snapshot);
          if (restored_max > 1e-3) {
            polysmith::core::log_warn(
                "sketch",
                "solver moved unconstrained geometry by " +
                    std::to_string(restored_max) +
                    " mm; restored it (only constraint-connected geometry may "
                    "move)");
          }
        }
        // Verify the solution before trusting it. planegcs drops redundant
        // constraints from the system it solves, so the answer can violate
        // constraints the user can see (see solver_touch_guard.inc). A rejected
        // solution keeps the pre-solve geometry: the sketch does not move,
        // rather than moving wrongly, and the log names what could not be met.
        {
          const std::string violation =
              solver_constraint_violation(*feature.sketch_parameters);
          // Degenerate geometry comes first: a collapsed line satisfies the
          // relations trivially, so the constraint check above would pass it and
          // the cleanup would then delete the line and its relations.
          const std::string degenerate = solver_solution_collapses_geometry(
              *feature.sketch_parameters, solver_snapshot);
          if (!violation.empty() || !degenerate.empty()) {
            restore_all_solver_geometry(*feature.sketch_parameters,
                                        solver_snapshot);
            // Report the failure as a solver failure, so the command that
            // added the offending constraint demotes/refuses it instead of
            // leaving a constraint the solver can only satisfy by destroying
            // the sketch. This is what turns "the sketch is protected but still
            // deformed" into "the constraint is refused with a message".
            feature.sketch_parameters->solver_ok = false;
            polysmith::core::log_warn(
                "sketch",
                "solver solution rejected (" +
                    (degenerate.empty() ? violation : degenerate) +
                    "); kept the geometry from before the solve");
          }
        }

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
          polysmith::core::log_error(
              "sketch",
              "solver produced " + std::to_string(zero_count) +
                  " zero-length line(s): " + zero_ids.c_str() +
                  (zero_count > 3 ? " ..." : ""));
        }

        bool solver_exploded = false;
        if (pre_total_length > 1.0 &&
            (post_total_length > pre_total_length * 100.0 ||
             post_total_length < pre_total_length / 100.0)) {
          polysmith::core::log_error(
              "sketch",
              "solver exploded geometry - total line length " +
                  std::to_string(static_cast<long long>(pre_total_length)) +
                  " -> " +
                  std::to_string(static_cast<long long>(post_total_length)) +
                  " mm (" +
                  std::to_string(static_cast<long long>(
                      post_total_length / pre_total_length)) +
                  "x change), reverting to pre-solve positions");
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

        // Clean up any zero-length lines the solver produced before running the
        // ad-hoc enforce functions below (which throw on zero-length geometry).
        // S6: the removal is reported, not silent.
        {
          std::vector<std::string> solver_zero_ids;
          for (const auto &line : feature.sketch_parameters->lines) {
            if (points_match(line.start_x, line.start_y, line.end_x,
                             line.end_y)) {
              solver_zero_ids.push_back(line.id);
            }
          }
          for (const auto &zid : solver_zero_ids) {
            const CollapsedLineCleanupReport report =
                remove_collapsed_line_records(*feature.sketch_parameters, zid);
            if (report.total_removed() > 0) {
              polysmith::core::log_warn(
                  "sketch", format_collapsed_line_warning(zid, report));
            } else {
              polysmith::core::log_warn(
                  "sketch", "deleted solver-produced zero-length line " + zid);
            }
          }
        }
      }      feature.sketch_parameters->solver_dofs = result.dofs;
      feature.sketch_parameters->solver_ok = result.ok();
      feature.sketch_parameters->solver_conflicting_count =
          static_cast<int>(result.conflicting.size());
      feature.sketch_parameters->solver_redundant_count =
          static_cast<int>(result.redundant.size());

      // A negative dofs count is NOT evidence of over-constraint: planegcs
      // returns -1 when it could not diagnose the system at all (no unknowns /
      // empty diagnosis matrix) and params-minus-nonredundant otherwise. The
      // signal that something is actually wrong is a conflicting or redundant
      // constraint that the solver identified, so only those are reported as an
      // error — this used to log "sketch over-constrained" (a red line the user
      // sees in the Logs panel) for perfectly healthy sketches.
      if (result.conflicting.size() > 0 || result.redundant.size() > 0) {
        polysmith::core::log_error(
            "sketch",
            "sketch over-constrained - dofs=" + std::to_string(result.dofs) +
                " conflicting=" +
                std::to_string(result.conflicting.size()) + " redundant=" +
                std::to_string(result.redundant.size()));
      } else if (result.dofs < 0) {
        polysmith::core::log_debug(
            "sketch",
            "solver could not count the remaining degrees of freedom (" +
                std::to_string(result.dofs) +
                "); no conflicting or redundant constraint was found");
      }
      if (!result.ok()) {
        const char* reason =
            result.status == GCS::Failed ? "solver failed to converge" :
            result.status == GCS::SuccessfulSolutionInvalid
                ? "solution invalid (redundant/conflicting constraints)"
                : "unknown error";
        polysmith::core::log_error(
            "sketch",
            std::string("sketch solver failed - ") + reason + " (status=" +
                std::to_string(static_cast<int>(result.status)) +
                ", dofs=" + std::to_string(result.dofs) + ")");
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

        // Append mode freezes every point outside the focus entity, so the DOF
        // count above belongs to a FROZEN system, not to the sketch: a rectangle
        // reported 1° of freedom after any constraint command, and the badge
        // showed that number. Measure the real count on the unfrozen sketch with
        // a throwaway solver — it is only asked for a diagnosis, and its solution
        // is never applied, so the geometry the freeze protected stays put.
        //
        // The extra solve only happens for COMMANDS (they are what set an append
        // focus); dragging and dimension edits go through the plain path.
        ConstraintSolver dof_solver;
        dof_solver.build(*feature.sketch_parameters);
        const auto dof_result = dof_solver.solve(GCS::DogLeg, /*diagnose=*/true);
        if (dof_result.ok()) {
          feature.sketch_parameters->solver_dofs = dof_result.dofs;
          feature.sketch_parameters->solver_conflicting_count =
              static_cast<int>(dof_result.conflicting.size());
          feature.sketch_parameters->solver_redundant_count =
              static_cast<int>(dof_result.redundant.size());
        }
      }
    }

    // Consume the append-focus ids on EVERY refresh, not only when the
    // solver ran.  A geometry-only commit (no driving constraints yet)
    // skips the solver block above; leaving the focus ids behind makes
    // a later refresh (e.g. when a dimension turns driving) freeze every
    // point — append mode freezes all points outside the focus set, and
    // a stale entity id matches nothing.
    feature.sketch_parameters->pending_append_focus_ids.clear();
  }

  // Anchor enforcement runs here — after the solver but before
  // constraint enforcement. Constraint enforcement (H/V, relations)
  // may need to adjust lines whose endpoints were moved by anchors,
  // so anchors must run first.
  enforce_midpoint_anchors(*feature.sketch_parameters);
  enforce_point_line_anchors(*feature.sketch_parameters);
  enforce_tangent_line_circle_relations(*feature.sketch_parameters);
  enforce_tangent_circle_line_relations(*feature.sketch_parameters);
  enforce_constraint_completion(*feature.sketch_parameters);

  // Enforce stored constraints so badges always match geometry.
  // H/V runs first (axis alignment), then relations (inter-line).
  // Any constraint that would destroy geometry is cleared instead.
  enforce_line_HV_constraints(*feature.sketch_parameters);
  try { enforce_all_equal_length_relations(*feature.sketch_parameters); }
  catch (const std::exception& e) {
    polysmith::core::log_error(
        "sketch",
        std::string("equal-length relation enforcement failed: ") + e.what());
  }
  try { enforce_all_perpendicular_relations(*feature.sketch_parameters); }
  catch (const std::exception& e) {
    polysmith::core::log_error(
        "sketch",
        std::string("perpendicular relation enforcement failed: ") + e.what());
  }
  try { enforce_all_parallel_relations(*feature.sketch_parameters); }
  catch (const std::exception& e) {
    polysmith::core::log_error(
        "sketch",
        std::string("parallel relation enforcement failed: ") + e.what());
  }

  // Fillets must run *after* the tangent passes (so they see the
  // latest line endpoints) and *before* `rebuild_sketch_vertices`.
  enforce_sketch_fillets(*feature.sketch_parameters);
  enforce_sketch_chamfers(*feature.sketch_parameters);

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

  // Re-impose driving arc dimensions deterministically (see
  // enforce_arc_dimensions in dimension_update.inc).  Must run before
  // sync_all_line_dimensions / rebuild_sketch_vertices so the profile
  // walk sees the dimensioned geometry.
  enforce_arc_dimensions(*feature.sketch_parameters);

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

  // Text glyph vertices and slot boundary vertices are derived geometry
  // owned by their parametric entities — always fixed, regardless of
  // what sync_fixed_point_flags carried over. This runs LAST
  // (authoritative) so the flag can never be lost by the derived-point
  // or user-flag passes above. Slot CENTER vertices carry a regular
  // "vertex-N" id and are deliberately not covered here (they are
  // movable, like circle centers).
  for (auto& vertex : feature.sketch_parameters->vertices) {
    if (vertex.id.rfind("vertex-text-", 0) == 0 ||
        vertex.id.rfind("vertex-slot-", 0) == 0) {
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

  // ── Flight recorder report ─────────────────────────────────────────
  if (feature.sketch_parameters.has_value() && !recorder_before.empty()) {
    const auto& parameters = *feature.sketch_parameters;
    double recorder_length_after = 0.0;
    for (const auto& line : parameters.lines) {
      recorder_length_after +=
          std::hypot(line.end_x - line.start_x, line.end_y - line.start_y);
    }
    double worst_move = 0.0;
    std::string worst_id;
    std::string worst_detail;
    for (const auto& vertex : parameters.vertices) {
      const auto it = std::find_if(
          recorder_before.begin(), recorder_before.end(),
          [&](const RecorderVertex& before) { return before.id == vertex.id; });
      if (it == recorder_before.end()) continue;
      const double moved =
          std::hypot(vertex.x - it->x, vertex.y - it->y);
      if (moved > worst_move) {
        worst_move = moved;
        worst_id = vertex.id;
        worst_detail = "(" + std::to_string(it->x) + ", " +
                       std::to_string(it->y) + ")->(" +
                       std::to_string(vertex.x) + ", " +
                       std::to_string(vertex.y) + ")";
      }
    }
    // "Drastic" is measured against what the sketch could legitimately be: its
    // own size plus the largest dimension the user asked for. A dimension edit
    // moves geometry by design (a 400 mm width on a 160 mm sketch is fine); an
    // explosion leaves the sketch many times bigger than anything requested.
    double max_driving_dimension = 0.0;
    int driving_dimensions = 0;
    for (const auto& dimension : parameters.dimensions) {
      if (dimension.driven ||
          (dimension.is_auto && dimension.expression.empty())) {
        continue;
      }
      ++driving_dimensions;
      max_driving_dimension =
          std::max(max_driving_dimension, std::abs(dimension.value));
    }
    const double expected_scale = recorder_length_before + max_driving_dimension;
    const double explosion_limit = 4.0 * expected_scale + 100.0;
    const bool exploded =
        (recorder_length_before > 1.0 &&
         recorder_length_after > explosion_limit) ||
        (recorder_length_before > 100.0 &&
         recorder_length_after < recorder_length_before / 4.0) ||
        worst_move > explosion_limit;
    if (exploded) {
      polysmith::core::log_error(
          "sketch",
          "sketch geometry changed drastically in one refresh: total line "
          "length " + std::to_string(recorder_length_before) + " -> " +
              std::to_string(recorder_length_after) + " mm, worst vertex " +
              worst_id + " moved " + std::to_string(worst_move) + " mm " +
              worst_detail + " | lines=" +
              std::to_string(parameters.lines.size()) + " circles=" +
              std::to_string(parameters.circles.size()) + " arcs=" +
              std::to_string(parameters.arcs.size()) + " driving_dims=" +
              std::to_string(driving_dimensions) + " relations=" +
              std::to_string(parameters.line_relations.size()) + " anchors=" +
              std::to_string(parameters.midpoint_anchors.size() +
                             parameters.point_line_anchors.size()) +
              " fillets=" + std::to_string(parameters.fillets.size()) +
              " constraints=" +
              std::to_string(parameters.constraints.size()));
    }
  }
}

void refresh_sketch_with_append_focus(FeatureEntry& feature,
                                       const std::vector<std::string>& focus_ids) {
  if (feature.sketch_parameters.has_value()) {
    feature.sketch_parameters->pending_append_focus_ids = focus_ids;
    refresh_sketch_derived_state(feature);
  }
}

// ── Collapsed-line cleanup (S6) ───────────────────────────────
// Removes a line the solver collapsed and every record that referenced it.
// Extracted from the refresh so the counts and the resulting warning are
// testable without a solver run or a logger sink.
CollapsedLineCleanupReport remove_collapsed_line_records(
    SketchFeatureParameters& parameters, const std::string& line_id) {
  CollapsedLineCleanupReport report;

  const auto erase_and_count = [](auto& container, auto&& predicate,
                                  int& counter) {
    const std::size_t before = container.size();
    container.erase(
        std::remove_if(container.begin(), container.end(), predicate),
        container.end());
    counter += static_cast<int>(before - container.size());
  };

  erase_and_count(
      parameters.constraints,
      [&](const SketchConstraint& constraint) {
        return std::find(constraint.target_ids.begin(),
                         constraint.target_ids.end(),
                         line_id) != constraint.target_ids.end();
      },
      report.constraints);
  erase_and_count(
      parameters.dimensions,
      [&](const SketchDimension& dimension) {
        return dimension.entity_id == line_id ||
               dimension.secondary_entity_id == line_id;
      },
      report.dimensions);
  erase_and_count(
      parameters.line_relations,
      [&](const SketchLineRelation& relation) {
        return relation.first_line_id == line_id ||
               relation.second_line_id == line_id;
      },
      report.relations);
  erase_and_count(
      parameters.fillets,
      [&](const SketchFillet& fillet) {
        return fillet.line_a_id == line_id || fillet.line_b_id == line_id;
      },
      report.relations);
  erase_and_count(
      parameters.midpoint_anchors,
      [&](const SketchMidpointAnchor& anchor) {
        return anchor.line_id == line_id;
      },
      report.relations);
  erase_and_count(
      parameters.point_line_anchors,
      [&](const SketchVertexLineAnchor& anchor) {
        return anchor.line_id == line_id;
      },
      report.relations);
  erase_and_count(
      parameters.lines, [&](const SketchLine& line) { return line.id == line_id; },
      report.lines);

  return report;
}

// ── Loose-end diagnostics (S5) ───────────────────────────────
// Names the chain ends and the near-miss pairs that keep a sketch from closing,
// from the vertex table the profile walk already builds. No solver involvement:
// this is a read of the same data the profile walk uses, so it can never
// disagree with it about what is connected.
SketchDiagnostics compute_sketch_diagnostics(
    const SketchFeatureParameters& parameters) {
  SketchDiagnostics diagnostics;

  // Generated / projected vertices belong to derived entities the user cannot
  // tie: text glyphs, slot corners, projected body edges.
  const auto is_derived_vertex = [](const SketchVertex& vertex) {
    return vertex.is_projected ||
           vertex.id.rfind("vertex-text-", 0) == 0 ||
           vertex.id.rfind("vertex-slot-", 0) == 0 ||
           vertex.id.rfind("vertex-project", 0) == 0;
  };

  for (const auto& vertex : parameters.vertices) {
    if (vertex.kind != "endpoint" || is_derived_vertex(vertex)) {
      continue;
    }
    // Exactly one owning curve = the end of a chain. A corner where two curves
    // meet carries two owners and is not an end.
    if (vertex.geometry_owner_ids.size() != 1) {
      continue;
    }
    diagnostics.open.push_back(SketchLooseEnd{
        .vertex_id = vertex.id,
        .x = vertex.x,
        .y = vertex.y,
        .owner_ids = vertex.geometry_owner_ids,
    });
  }

  // Two OPEN ends from different curves that sit within the tolerance of each
  // other: they look joined, the profile walk sees a gap.
  for (std::size_t a = 0; a < diagnostics.open.size(); ++a) {
    for (std::size_t b = a + 1; b < diagnostics.open.size(); ++b) {
      const SketchLooseEnd& end_a = diagnostics.open[a];
      const SketchLooseEnd& end_b = diagnostics.open[b];
      if (end_a.owner_ids == end_b.owner_ids) {
        continue;
      }
      const double gap =
          std::hypot(end_a.x - end_b.x, end_a.y - end_b.y);
      if (gap <= kUntiedEndToleranceMm) {
        diagnostics.untied.push_back(SketchUntiedPair{
            .vertex_a_id = end_a.vertex_id,
            .vertex_b_id = end_b.vertex_id,
            .gap = gap,
        });
      }
    }
  }

  return diagnostics;
}

#include "core/sketch/impl/auto_constraint_detection.inc"
#include "core/sketch/impl/auto_constrain_apply.inc"
#include "core/sketch/impl/sketch_create_and_tool_commands.inc"
