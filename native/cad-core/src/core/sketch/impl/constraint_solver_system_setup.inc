    // --- Phase 2: build the GCS::System ---
    system_ = std::make_unique<GCS::System>();
    system_->clear();

    // Declare parameters. Fixed points are NOT added to unknowns.
    // Circle radii are unknowns so the solver can adjust them to
    // satisfy radius constraints.
    GCS::VEC_pD unknown_params;
    for (size_t i = 0; i < num_points; ++i) {
        if (!params.points[i].is_fixed) {
            unknown_params.push_back(&param_storage_[i * 2]);     // x
            unknown_params.push_back(&param_storage_[i * 2 + 1]); // y
        }
    }
    // Circle radius parameters start after the point params.
    size_t radius_base = num_points * 2;
    for (size_t i = 0; i < num_circles; ++i) {
        unknown_params.push_back(&param_storage_[radius_base + i]);
    }
    system_->declareUnknowns(unknown_params);
