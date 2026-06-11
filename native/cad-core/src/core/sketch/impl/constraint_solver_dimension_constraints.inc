    // --- Phase 4: dimensional constraints (from dimensions) ---
    // Manual dimensions (is_auto=false) are always driving constraints
    // at their current measured value. Auto-dimensions (is_auto=true)
    // become driving constraints only when the user types a value
    // (non-empty expression).
    for (const auto& dim : params.dimensions) {
        if (dim.driven) continue;  // reference-only, not a constraint
        // Auto-dimensions without a user-set expression are display-only.
        if (dim.is_auto && dim.expression.empty()) continue;

        // Allocate a parameter for the constraint target value.
        size_t val_idx = param_storage_.size();
        param_storage_.push_back(dim.value);
        double* val_ptr = &param_storage_[val_idx];

        if (dim.kind == "line_length") {
            auto lit = line_id_to_index_.find(dim.entity_id);
            if (lit != line_id_to_index_.end()) {
                auto& gl = gcs_lines_[lit->second];
                system_->addConstraintP2PDistance(gl.p1, gl.p2, val_ptr);
            }
        } else if (dim.kind == "line_angle") {
            auto lit = line_id_to_index_.find(dim.entity_id);
            if (lit != line_id_to_index_.end()) {
                auto& gl = gcs_lines_[lit->second];
                // P2PAngle constrains the angle of the vector from p1 to p2.
                system_->addConstraintP2PAngle(gl.p1, gl.p2, val_ptr, 0.0);
            }
        } else if (dim.kind == "circle_radius") {
            auto cit = circle_id_to_index_.find(dim.entity_id);
            if (cit != circle_id_to_index_.end()) {
                system_->addConstraintCircleRadius(
                    gcs_circles_[cit->second], val_ptr);
            }
        } else if (dim.kind == "angle" && !dim.secondary_entity_id.empty()) {
            // Angle between two lines.  Ensure both line directions
            // (p1→p2) point away from the shared vertex so the solver
            // enforces the interior angle, not its supplement.
            auto lit1 = line_id_to_index_.find(dim.entity_id);
            auto lit2 = line_id_to_index_.find(dim.secondary_entity_id);
            if (lit1 != line_id_to_index_.end() &&
                lit2 != line_id_to_index_.end()) {
                auto gl1 = gcs_lines_[lit1->second];
                auto gl2 = gcs_lines_[lit2->second];
                // Detect shared endpoint by comparing coordinate values.
                auto eq = [](const GCS::Point& a, const GCS::Point& b) {
                    return std::abs(*a.x - *b.x) < 1e-6 &&
                           std::abs(*a.y - *b.y) < 1e-6;
                };
                int best_i = -1, best_j = -1;
                if (eq(gl1.p1, gl2.p1))      { best_i = 0; best_j = 0; }
                else if (eq(gl1.p1, gl2.p2)) { best_i = 0; best_j = 1; }
                else if (eq(gl1.p2, gl2.p1)) { best_i = 1; best_j = 0; }
                else if (eq(gl1.p2, gl2.p2)) { best_i = 1; best_j = 1; }
                // If shared pivot is at p2, swap so it's at p1
                // (direction points away from pivot).
                if (best_i == 1) std::swap(gl1.p1, gl1.p2);
                if (best_j == 1) std::swap(gl2.p1, gl2.p2);
                system_->addConstraintL2LAngle(gl1, gl2, val_ptr);
            }
        } else if (dim.kind == "point_distance" &&
                   !dim.secondary_entity_id.empty()) {
            auto pit1 = point_id_to_index_.find(dim.entity_id);
            auto pit2 = point_id_to_index_.find(dim.secondary_entity_id);
            if (pit1 != point_id_to_index_.end() &&
                pit2 != point_id_to_index_.end()) {
                system_->addConstraintP2PDistance(
                    gcs_points_[pit1->second],
                    gcs_points_[pit2->second],
                    val_ptr);
            }
        } else if (dim.kind == "circle_center_distance" &&
                   !dim.secondary_entity_id.empty()) {
            // Distance between two circle centers (P2P).
            std::string cid1 = "point-circle-" + dim.entity_id + "-center";
            std::string cid2 = "point-circle-" + dim.secondary_entity_id + "-center";
            auto pit1 = point_id_to_index_.find(cid1);
            auto pit2 = point_id_to_index_.find(cid2);
            if (pit1 != point_id_to_index_.end() &&
                pit2 != point_id_to_index_.end()) {
                system_->addConstraintP2PDistance(
                    gcs_points_[pit1->second],
                    gcs_points_[pit2->second],
                    val_ptr);
            }
        } else if (dim.kind == "circle_line_distance" &&
                   !dim.secondary_entity_id.empty()) {
            // Distance from circle center to a line.
            // entity_id is the circle, secondary_entity_id is the line.
            auto cit = circle_id_to_index_.find(dim.entity_id);
            auto lit = line_id_to_index_.find(dim.secondary_entity_id);
            if (cit != circle_id_to_index_.end() &&
                lit != line_id_to_index_.end()) {
                system_->addConstraintC2LDistance(
                    gcs_circles_[cit->second],
                    gcs_lines_[lit->second],
                    val_ptr, 0);
            }
        }
    }
