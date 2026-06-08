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
            // Angle between two lines.
            auto lit1 = line_id_to_index_.find(dim.entity_id);
            auto lit2 = line_id_to_index_.find(dim.secondary_entity_id);
            if (lit1 != line_id_to_index_.end() &&
                lit2 != line_id_to_index_.end()) {
                system_->addConstraintL2LAngle(
                    gcs_lines_[lit1->second],
                    gcs_lines_[lit2->second],
                    val_ptr);
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
