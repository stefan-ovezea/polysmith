    // --- Phase 1: count and allocate parameter storage ---
    // Pre-count to avoid reallocation (which would invalidate double*
    // pointers stored in GCS::Point objects).
    // We read geometry from lines/circles (canonical storage), not from
    // params.vertices[] (which is a deduplicated view rebuilt later by
    // rebuild_sketch_vertices). This ensures the solver sees the current
    // geometry even when called between rebuilds (e.g. during drag).
    const size_t num_points = params.vertices.size();
    const size_t num_circles = params.circles.size();
    // Only user-drawn arcs that a solver constraint actually references
    // participate (see arc_participates_in_solver); derived arcs
    // (fillets, projections, generated) are re-derived after the solve.
    size_t num_arcs = 0;
    for (const auto& arc : params.arcs) {
        if (!is_derived_arc(params, arc) && arc_participates_in_solver(params, arc)) {
            ++num_arcs;
        }
    }
    // Count driving dimensions for parameter reservation.
    // Manual dims always drive; auto dims drive only with expression.
    size_t driving_dim_count = 0;
    for (const auto& dim : params.dimensions) {
        if (dim.driven) continue;
        if (dim.is_auto && dim.expression.empty()) continue;
        ++driving_dim_count;
    }
    const size_t num_params = num_points * 2 + num_circles * 3 +
                              num_arcs * 3 + driving_dim_count;

    param_storage_.clear();
    param_storage_.reserve(num_params);

    gcs_points_.clear();
    gcs_points_.reserve(num_points);
    point_id_to_index_.clear();
