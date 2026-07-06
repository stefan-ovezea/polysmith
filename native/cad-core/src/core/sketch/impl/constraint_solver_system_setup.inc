    // --- Phase 2: build the GCS::System ---
    system_ = std::make_unique<GCS::System>();
    system_->clear();

    // Give the solver more iterations than the planegcs default (100).
    // LM in particular benefits from extra headroom on under-constrained
    // sketches where the error surface is flat and progress is slow.
    system_->maxIter = 200;

    // Declare parameters. Fixed points are NOT added to unknowns.
    // Circle radii are unknowns so the solver can adjust them to
    // satisfy radius constraints.
    GCS::VEC_pD unknown_params;
    for (size_t i = 0; i < num_points; ++i) {
        if (!params.vertices[i].is_fixed) {
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
