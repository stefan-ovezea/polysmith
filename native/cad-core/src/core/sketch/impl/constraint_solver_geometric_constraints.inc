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

    // SketchLineRelation constraints (parallel, perpendicular, equal).
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
        } else if (rel.kind == "tangent_line_circle") {
            // second_line_id is the circle id for tangent_line_circle.
            auto cit = circle_id_to_index_.find(rel.second_line_id);
            if (cit != circle_id_to_index_.end()) {
                system_->addConstraintTangent(
                    gl1, gcs_circles_[cit->second]);
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
            // center points (naming convention: point-circle-{id}-center)
            // and add a P2PCoincident between them.
            std::string cid1 = "point-circle-" + sc.target_ids[0] + "-center";
            std::string cid2 = "point-circle-" + sc.target_ids[1] + "-center";
            auto pit1 = point_id_to_index_.find(cid1);
            auto pit2 = point_id_to_index_.find(cid2);
            if (pit1 != point_id_to_index_.end() &&
                pit2 != point_id_to_index_.end()) {
                system_->addConstraintP2PCoincident(
                    gcs_points_[pit1->second],
                    gcs_points_[pit2->second]);
            }
        }
    }

    // Midpoint and point-line anchors: constrain the anchored point
    // to lie on the host line. Full parametric position enforcement
    // (t=0.5 for midpoints, t=t for point-line) is a follow-up.
    for (const auto& anchor : params.midpoint_anchors) {
        auto pit = point_id_to_index_.find(anchor.point_id);
        auto lit = line_id_to_index_.find(anchor.line_id);
        if (pit != point_id_to_index_.end() &&
            lit != line_id_to_index_.end()) {
            system_->addConstraintPointOnLine(
                gcs_points_[pit->second],
                gcs_lines_[lit->second]);
        }
    }

    for (const auto& anchor : params.point_line_anchors) {
        auto pit = point_id_to_index_.find(anchor.point_id);
        auto lit = line_id_to_index_.find(anchor.line_id);
        if (pit != point_id_to_index_.end() &&
            lit != line_id_to_index_.end()) {
            system_->addConstraintPointOnLine(
                gcs_points_[pit->second],
                gcs_lines_[lit->second]);
        }
    }
