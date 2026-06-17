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

                // Virtual pivot: stored on dimension for non-intersecting
                // lines.  Orient both p1→p2 directions away from the pivot
                // so the signed angle matches what was stored at creation.
                if (dim.pivot_x.has_value() && dim.pivot_y.has_value()) {
                    double px = *dim.pivot_x;
                    double py = *dim.pivot_y;
                    auto dist = [](double x1, double y1, double x2, double y2) {
                        double dx = x1 - x2, dy = y1 - y2;
                        return std::sqrt(dx * dx + dy * dy);
                    };
                    // Orient gl1: p1→p2 away from pivot
                    double d1a = dist(*gl1.p1.x, *gl1.p1.y, px, py);
                    double d1b = dist(*gl1.p2.x, *gl1.p2.y, px, py);
                    if (d1a > d1b) std::swap(gl1.p1, gl1.p2);
                    // Orient gl2: p1→p2 away from pivot
                    double d2a = dist(*gl2.p1.x, *gl2.p1.y, px, py);
                    double d2b = dist(*gl2.p2.x, *gl2.p2.y, px, py);
                    if (d2a > d2b) std::swap(gl2.p1, gl2.p2);
                    system_->addConstraintL2LAngle(gl1, gl2, val_ptr);
                    continue;
                }
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

                // Endpoint-on-segment case (T-connection): one line's
                // endpoint lies on the other line's segment rather than
                // at a shared vertex.  Orient both direction vectors
                // away from that pivot so the L2LAngle signed-angle
                // convention matches what add_sketch_angle_dimension
                // stored (dir_away from the pivot).
                if (best_i == -1 && best_j == -1) {
                    constexpr double kLocalPi = 3.14159265358979323846;
                    constexpr double kLocalTwoPi = 2.0 * kLocalPi;
                    double requested = std::fmod(std::abs(*val_ptr), kLocalTwoPi);
                    if (requested < 0.0) requested += kLocalTwoPi;
                    const double target_small =
                        requested > kLocalPi ? kLocalTwoPi - requested : requested;
                    auto ray_score = [&](const GCS::Point& ray_start,
                                         const GCS::Point& ray_end,
                                         double other_dx,
                                         double other_dy,
                                         bool host_is_first_line) {
                        double dx = *ray_end.x - *ray_start.x;
                        double dy = *ray_end.y - *ray_start.y;
                        double len = std::sqrt(dx * dx + dy * dy);
                        double other_len =
                            std::sqrt(other_dx * other_dx + other_dy * other_dy);
                        if (len < 1e-12 || other_len < 1e-12) {
                            return 1e99;
                        }
                        dx /= len;
                        dy /= len;
                        other_dx /= other_len;
                        other_dy /= other_len;
                        const double cross = host_is_first_line
                            ? dx * other_dy - dy * other_dx
                            : other_dx * dy - other_dy * dx;
                        const double dot = dx * other_dx + dy * other_dy;
                        return std::abs(std::abs(std::atan2(cross, dot)) -
                                        target_small);
                    };
                    auto pt_on_seg = [](double px, double py,
                                       double ax, double ay,
                                       double bx, double by,
                                       double tol) -> bool {
                        double dx = bx - ax, dy = by - ay;
                        double len2 = dx * dx + dy * dy;
                        if (len2 < 1e-12) return false;
                        double t = ((px - ax) * dx + (py - ay) * dy) / len2;
                        if (t < 0.0 || t > 1.0) return false;
                        double proj_x = ax + t * dx, proj_y = ay + t * dy;
                        return std::sqrt((px - proj_x) * (px - proj_x) +
                                         (py - proj_y) * (py - proj_y)) < tol;
                    };
                    bool found = false;
                    // Check each endpoint of gl1 against gl2's segment.
                    for (int ei = 0; ei < 2 && !found; ++ei) {
                        const GCS::Point& p =
                            (ei == 0) ? gl1.p1 : gl1.p2;
                        if (pt_on_seg(*p.x, *p.y,
                                      *gl2.p1.x, *gl2.p1.y,
                                      *gl2.p2.x, *gl2.p2.y, 0.10)) {
                            // gl1's endpoint lies on gl2 — pivot is p.
                            if (ei == 1) std::swap(gl1.p1, gl1.p2);
                            const double other_dx = *gl1.p2.x - *gl1.p1.x;
                            const double other_dy = *gl1.p2.y - *gl1.p1.y;
                            const double keep_score =
                                ray_score(gl2.p1, gl2.p2, other_dx, other_dy, false);
                            const double swap_score =
                                ray_score(gl2.p2, gl2.p1, other_dx, other_dy, false);
                            if (swap_score < keep_score) std::swap(gl2.p1, gl2.p2);
                            found = true;
                        }
                    }
                    // Check each endpoint of gl2 against gl1's segment.
                    for (int ej = 0; ej < 2 && !found; ++ej) {
                        const GCS::Point& p =
                            (ej == 0) ? gl2.p1 : gl2.p2;
                        if (pt_on_seg(*p.x, *p.y,
                                      *gl1.p1.x, *gl1.p1.y,
                                      *gl1.p2.x, *gl1.p2.y, 0.10)) {
                            // gl2's endpoint lies on gl1 — pivot is p.
                            if (ej == 1) std::swap(gl2.p1, gl2.p2);
                            const double other_dx = *gl2.p2.x - *gl2.p1.x;
                            const double other_dy = *gl2.p2.y - *gl2.p1.y;
                            const double keep_score =
                                ray_score(gl1.p1, gl1.p2, other_dx, other_dy, true);
                            const double swap_score =
                                ray_score(gl1.p2, gl1.p1, other_dx, other_dy, true);
                            if (swap_score < keep_score) std::swap(gl1.p1, gl1.p2);
                            found = true;
                        }
                    }
                }
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
