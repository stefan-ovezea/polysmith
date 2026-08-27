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
    // One length constant per collinear / tangent_line_line relation
    // (pins the driven line's own length during the solve), one per
    // midpoint constraint and two per curve-curve tangent constraint
    // (pins the radii).  Midpoint/point-line ANCHORS are deliberately
    // NOT solver-mapped with length pins — the pin conflicts with a
    // driving dimension on the host line, and the deterministic
    // enforce_midpoint_anchors / enforce_point_line_anchors passes
    // position the anchored points after every solve.
    size_t num_length_params = 0;
    for (const auto& rel : params.line_relations) {
        if (rel.kind == "collinear" || rel.kind == "tangent_line_line") {
            ++num_length_params;
        }
    }
    for (const auto& constraint : params.constraints) {
        if (constraint.kind == "midpoint") ++num_length_params;
        if (constraint.kind == "tangent_circle_circle" ||
            constraint.kind == "tangent_arc_arc") {
            // Two radius pins per curve-curve tangent constraint.
            num_length_params += 2;
        }
    }
    const size_t num_params = num_points * 2 + num_circles * 3 +
                              num_arcs * 3 + driving_dim_count +
                              num_length_params;

    param_storage_.clear();
    param_storage_.reserve(num_params);

    gcs_points_.clear();
    gcs_points_.reserve(num_points);
    point_id_to_index_.clear();
