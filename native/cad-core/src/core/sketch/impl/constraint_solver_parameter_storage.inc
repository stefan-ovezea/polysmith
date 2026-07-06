    // --- Phase 1: count and allocate parameter storage ---
    // Pre-count to avoid reallocation (which would invalidate double*
    // pointers stored in GCS::Point objects).
    // We read geometry from lines/circles (canonical storage), not from
    // params.points[] (which is a deduplicated view rebuilt later by
    // rebuild_sketch_vertices). This ensures the solver sees the current
    // geometry even when called between rebuilds (e.g. during drag).
    const size_t num_points = params.points.size();
    const size_t num_circles = params.circles.size();
    // Count driving dimensions for parameter reservation.
    // Manual dims always drive; auto dims drive only with expression.
    size_t driving_dim_count = 0;
    for (const auto& dim : params.dimensions) {
        if (dim.driven) continue;
        if (dim.is_auto && dim.expression.empty()) continue;
        ++driving_dim_count;
    }
    const size_t num_params =
        num_points * 2 + num_circles * 3 + driving_dim_count;

    param_storage_.clear();
    param_storage_.reserve(num_params);

    gcs_points_.clear();
    gcs_points_.reserve(num_points);
    point_id_to_index_.clear();
