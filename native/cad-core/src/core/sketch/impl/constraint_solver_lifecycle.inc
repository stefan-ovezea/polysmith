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

    // Write solved coordinates back to the canonical storage (lines
    // and circles). Do NOT write to params.points[] — that array is
    // rebuilt from lines/circles by rebuild_sketch_points() later in
    // refresh_sketch_derived_state.

    // Update line endpoints from their points.
    for (auto& sl : params.lines) {
        auto sit = point_id_to_index_.find(sl.start_point_id);
        auto eit = point_id_to_index_.find(sl.end_point_id);
        if (sit != point_id_to_index_.end()) {
            sl.start_x = param_storage_[sit->second * 2];
            sl.start_y = param_storage_[sit->second * 2 + 1];
        }
        if (eit != point_id_to_index_.end()) {
            sl.end_x = param_storage_[eit->second * 2];
            sl.end_y = param_storage_[eit->second * 2 + 1];
        }
    }

    // Update circle center positions and radii.
    size_t radius_base = params.points.size() * 2;
    for (size_t i = 0; i < params.circles.size(); ++i) {
        auto& sc = params.circles[i];
        std::string center_point_id =
            "point-circle-" + sc.id + "-center";
        auto cit = point_id_to_index_.find(center_point_id);
        if (cit != point_id_to_index_.end()) {
            sc.center_x = param_storage_[cit->second * 2];
            sc.center_y = param_storage_[cit->second * 2 + 1];
        }
        sc.radius = param_storage_[radius_base + i];
    }
}

int ConstraintSolver::dofs() const {
    return last_result_.dofs;
}
