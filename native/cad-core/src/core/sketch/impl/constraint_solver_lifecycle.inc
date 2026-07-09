ConstraintSolver::SolveResult ConstraintSolver::solve(GCS::Algorithm alg) {
    SolveResult result;
    if (!system_) {
        result.status = GCS::Failed;
        return result;
    }

    system_->initSolution(alg);
    int status = system_->solve(/*isFine=*/true, alg);
    result.status = static_cast<GCS::SolveStatus>(status);

    // DogLeg first (fast, converges reliably on under-constrained
    // sketches), Levenberg-Marquardt as safety net (conservative,
    // stays near the initial guess — catches the rare case where
    // DogLeg leaps to a distant solution).  This matches FreeCAD's
    // strategy.
    //
    // IMPORTANT: Do NOT call initSolution again before the retry — that
    // would call setReference(), snapshoting the (possibly corrupted)
    // post-DogLeg parameter values. Instead, call solve() directly; its
    // internal resetToReference() restores the original parameters from
    // the first initSolution call above.
    if (!result.ok() && alg == GCS::DogLeg) {
        status = system_->solve(/*isFine=*/true, GCS::LevenbergMarquardt);
        result.status = static_cast<GCS::SolveStatus>(status);
    }

    // Run diagnose to get DOF and conflicting/redundant info.
    system_->diagnose(alg);
    result.dofs = system_->dofsNumber();

    GCS::VEC_I conflicting, redundant, partially_redundant;
    system_->getConflicting(conflicting);
    system_->getRedundant(redundant);
    system_->getPartiallyRedundant(partially_redundant);
    result.conflicting.assign(conflicting.begin(), conflicting.end());
    result.redundant.assign(redundant.begin(), redundant.end());
    result.partially_redundant.assign(
        partially_redundant.begin(), partially_redundant.end());

    if (result.ok()) {
        system_->applySolution();
    }

    last_result_ = result;
    return result;
}

void ConstraintSolver::apply(SketchFeatureParameters& params) const {
    if (!last_result_.ok()) return;

    // ── Phase 5: write solved coords to canonical vertices first,
    // then sync back to entity cached fields ─────────────────────

    // Update line endpoint vertices, then sync cached coords.
    for (auto& sl : params.lines) {
        auto sit = point_id_to_index_.find(sl.start_vertex_id);
        auto eit = point_id_to_index_.find(sl.end_vertex_id);
        if (sit != point_id_to_index_.end()) {
            params.vertices[sit->second].x = param_storage_[sit->second * 2];
            params.vertices[sit->second].y = param_storage_[sit->second * 2 + 1];
            sl.start_x = params.vertices[sit->second].x;
            sl.start_y = params.vertices[sit->second].y;
        }
        if (eit != point_id_to_index_.end()) {
            params.vertices[eit->second].x = param_storage_[eit->second * 2];
            params.vertices[eit->second].y = param_storage_[eit->second * 2 + 1];
            sl.end_x = params.vertices[eit->second].x;
            sl.end_y = params.vertices[eit->second].y;
        }
    }

    // Update circle center vertices, then sync cached coords.
    size_t radius_base = params.vertices.size() * 2;
    for (size_t i = 0; i < params.circles.size(); ++i) {
        auto& sc = params.circles[i];
        std::string center_point_id = circle_center_vertex_id(sc);
        auto cit = point_id_to_index_.find(center_point_id);
        if (cit != point_id_to_index_.end()) {
            params.vertices[cit->second].x = param_storage_[cit->second * 2];
            params.vertices[cit->second].y = param_storage_[cit->second * 2 + 1];
            sc.center_x = params.vertices[cit->second].x;
            sc.center_y = params.vertices[cit->second].y;
        }
        sc.radius = param_storage_[radius_base + i];
    }
}

int ConstraintSolver::dofs() const {
    return last_result_.dofs;
}
