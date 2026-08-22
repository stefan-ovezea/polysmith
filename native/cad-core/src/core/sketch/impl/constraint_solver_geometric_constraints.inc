    // --- Phase 3: add constraints ---

    // SketchLine inline constraints (horizontal / vertical).
    for (const auto& sl : params.lines) {
        auto it = line_id_to_index_.find(sl.id);
        if (it == line_id_to_index_.end()) continue;
        auto& gl = gcs_lines_[it->second];
        if (sl.constraint.has_value()) {
            if (sl.constraint.value() == "horizontal") {
                system_->addConstraintHorizontal(gl);
            } else if (sl.constraint.value() == "vertical") {
                system_->addConstraintVertical(gl);
            }
        }
    }

    // SketchLineRelation constraints (parallel, perpendicular, equal,
    // collinear, tangent pairs).
    for (const auto& rel : params.line_relations) {
        auto it1 = line_id_to_index_.find(rel.first_line_id);
        auto it2 = line_id_to_index_.find(rel.second_line_id);
        if (it1 == line_id_to_index_.end() ||
            it2 == line_id_to_index_.end()) continue;
        auto& gl1 = gcs_lines_[it1->second];
        auto& gl2 = gcs_lines_[it2->second];
        if (rel.kind == "parallel") {
            system_->addConstraintParallel(gl1, gl2);
        } else if (rel.kind == "perpendicular") {
            system_->addConstraintPerpendicular(gl1, gl2);
        } else if (rel.kind == "equal_length") {
            system_->addConstraintEqualLength(gl1, gl2);
        } else if (rel.kind == "collinear" ||
                   rel.kind == "tangent_line_line") {
            // Collinear = parallel + an endpoint of the DRIVEN line
            // (first_line_id) lying on the reference line's infinite
            // axis + the driven line's own length preserved (the
            // tangent_line_line relation uses the same formulation —
            // two tangent lines are collinear through a shared corner).
            // Without the length pin the solver is free to stretch the
            // driven line along the axis.
            system_->addConstraintParallel(gl1, gl2);
            system_->addConstraintPointOnLine(gl1.p1, gl2);
            const double driven_len =
                std::hypot(*gl1.p2.x - *gl1.p1.x, *gl1.p2.y - *gl1.p1.y);
            param_storage_.push_back(driven_len);
            system_->addConstraintP2PDistance(
                gl1.p1, gl1.p2,
                &param_storage_[param_storage_.size() - 1], 0);
        } else if (rel.kind == "tangent_line_circle") {
            // second_line_id is a circle or arc id. For circles the
            // solver has the native Tangent constraint; for arcs (which
            // register only when participating) tangency is expressed
            // as center-to-line distance = radius.
            auto cit = circle_id_to_index_.find(rel.second_line_id);
            if (cit != circle_id_to_index_.end()) {
                system_->addConstraintTangent(
                    gl1, gcs_circles_[cit->second]);
            } else {
                auto ait = arc_id_to_index_.find(rel.second_line_id);
                if (ait != arc_id_to_index_.end()) {
                    auto& garc = gcs_arcs_[ait->second];
                    system_->addConstraintP2LDistance(
                        garc.center, gl1, garc.rad, 0);
                }
            }
        }
    }

    // SketchConstraint entries (coincident, concentric).
    for (const auto& sc : params.constraints) {
        if (sc.kind == "coincident" && sc.target_ids.size() >= 2) {
            auto it1 = point_id_to_index_.find(sc.target_ids[0]);
            auto it2 = point_id_to_index_.find(sc.target_ids[1]);
            if (it1 != point_id_to_index_.end() &&
                it2 != point_id_to_index_.end()) {
                system_->addConstraintP2PCoincident(
                    gcs_points_[it1->second],
                    gcs_points_[it2->second]);
            }
        } else if (sc.kind == "concentric" && sc.target_ids.size() >= 2) {
            // Concentric = center points coincide. Find both circles'
            // center points and add a P2PCoincident between them.
            std::string cid1 = circle_center_vertex_id_for(params, sc.target_ids[0]);
            std::string cid2 = circle_center_vertex_id_for(params, sc.target_ids[1]);
            auto pit1 = point_id_to_index_.find(cid1);
            auto pit2 = point_id_to_index_.find(cid2);
            if (pit1 != point_id_to_index_.end() &&
                pit2 != point_id_to_index_.end()) {
                system_->addConstraintP2PCoincident(
                    gcs_points_[pit1->second],
                    gcs_points_[pit2->second]);
            }
        } else if (sc.kind == "symmetric" && sc.target_ids.size() >= 3) {
            // NOT solver-mapped: planegcs has no native mirror-symmetry
            // primitive (PointOnPerpBisector constrains equidistance
            // from the axis ENDPOINTS, not mirroring across the axis).
            // Deterministic enforcement handles it —
            // enforce_symmetric_constraints in
            // constraint_completion_enforcement.inc.
        } else if (sc.kind == "midpoint" && sc.target_ids.size() >= 2) {
            // target_ids = [vertex_id, host_line_id]. The point is the
            // host line's midpoint when it is equidistant from both
            // endpoints AND on the line.  The host's own length is
            // pinned so the solve cannot stretch/shrink the host along
            // its free axis (see the collinear branch for the rationale).
            auto pit = point_id_to_index_.find(sc.target_ids[0]);
            auto lit = line_id_to_index_.find(sc.target_ids[1]);
            if (pit != point_id_to_index_.end() &&
                lit != line_id_to_index_.end()) {
                auto& gline = gcs_lines_[lit->second];
                system_->addConstraintPointOnPerpBisector(
                    gcs_points_[pit->second], gline);
                system_->addConstraintPointOnLine(
                    gcs_points_[pit->second], gline);
                const double host_len = std::hypot(
                    *gline.p2.x - *gline.p1.x, *gline.p2.y - *gline.p1.y);
                param_storage_.push_back(host_len);
                system_->addConstraintP2PDistance(
                    gline.p1, gline.p2,
                    &param_storage_[param_storage_.size() - 1], 0);
            }
        } else if (sc.kind == "tangent_circle_circle" &&
                   sc.target_ids.size() >= 2) {
            auto cit1 = circle_id_to_index_.find(sc.target_ids[0]);
            auto cit2 = circle_id_to_index_.find(sc.target_ids[1]);
            if (cit1 != circle_id_to_index_.end() &&
                cit2 != circle_id_to_index_.end()) {
                // Pin both radii at their current values so the solve
                // cannot satisfy tangency by resizing the circles; only
                // the centers may move.
                param_storage_.push_back(*gcs_circles_[cit1->second].rad);
                param_storage_.push_back(*gcs_circles_[cit2->second].rad);
                system_->addConstraintCircleRadius(
                    gcs_circles_[cit1->second],
                    &param_storage_[param_storage_.size() - 2]);
                system_->addConstraintCircleRadius(
                    gcs_circles_[cit2->second],
                    &param_storage_[param_storage_.size() - 1]);
                system_->addConstraintTangentCircumf(
                    gcs_circles_[cit1->second].center,
                    gcs_circles_[cit2->second].center,
                    gcs_circles_[cit1->second].rad,
                    gcs_circles_[cit2->second].rad,
                    /*internal=*/sc.value > 0.5);
            }
        } else if (sc.kind == "tangent_arc_arc" &&
                   sc.target_ids.size() >= 2) {
            auto ait1 = arc_id_to_index_.find(sc.target_ids[0]);
            auto ait2 = arc_id_to_index_.find(sc.target_ids[1]);
            if (ait1 != arc_id_to_index_.end() &&
                ait2 != arc_id_to_index_.end()) {
                param_storage_.push_back(*gcs_arcs_[ait1->second].rad);
                param_storage_.push_back(*gcs_arcs_[ait2->second].rad);
                system_->addConstraintCircleRadius(
                    gcs_arcs_[ait1->second],
                    &param_storage_[param_storage_.size() - 2]);
                system_->addConstraintCircleRadius(
                    gcs_arcs_[ait2->second],
                    &param_storage_[param_storage_.size() - 1]);
                system_->addConstraintTangentCircumf(
                    gcs_arcs_[ait1->second].center,
                    gcs_arcs_[ait2->second].center,
                    gcs_arcs_[ait1->second].rad,
                    gcs_arcs_[ait2->second].rad,
                    /*internal=*/sc.value > 0.5);
            }
        }
    }

    // Arc rules: keep each arc's start/end points on its circumcircle
    // at the stored angles (FreeCAD's ArcRules model).  This is what
    // makes arcs follow shared line endpoints through the solve.
    for (auto& garc : gcs_arcs_) {
        system_->addConstraintArcRules(garc);
    }

    // Midpoint and point-line anchors: constrain the anchored point
    // to lie on the host line.  The parametric position (t=0.5 for
    // midpoints, t=t for point-line) is enforced deterministically by
    // enforce_midpoint_anchors / enforce_point_line_anchors after the
    // solve — NOT by a solver mapping.  A WeightedLinearCombination
    // mapping was tried (SK2) and reverted: its pole gradients let
    // DogLeg satisfy the anchor by translating the under-constrained
    // host line (null-space drift) whenever the anchored point starts
    // off-target, and the host-length pin it needed conflicted with
    // driving dimensions on the host (dofs=-6, drive rejected).
    // PointOnLine only constrains the perpendicular distance, which is
    // zero in every refresh where the host was just edited, so the
    // solver leaves the anchor alone and the deterministic pass moves
    // the anchored point — never the host.
    for (const auto& anchor : params.midpoint_anchors) {
        auto pit = point_id_to_index_.find(anchor.vertex_id);
        auto lit = line_id_to_index_.find(anchor.line_id);
        if (pit != point_id_to_index_.end() &&
            lit != line_id_to_index_.end()) {
            system_->addConstraintPointOnLine(
                gcs_points_[pit->second],
                gcs_lines_[lit->second]);
        }
    }

    for (const auto& anchor : params.point_line_anchors) {
        auto pit = point_id_to_index_.find(anchor.vertex_id);
        auto lit = line_id_to_index_.find(anchor.line_id);
        if (pit != point_id_to_index_.end() &&
            lit != line_id_to_index_.end()) {
            system_->addConstraintPointOnLine(
                gcs_points_[pit->second],
                gcs_lines_[lit->second]);
        }
    }
