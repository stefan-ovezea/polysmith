    // --- Phase 2: build the GCS::System ---
    system_ = std::make_unique<GCS::System>();
    system_->clear();

    // Give the solver more iterations than the planegcs default (100).
    // LM in particular benefits from extra headroom on under-constrained
    // sketches where the error surface is flat and progress is slow.
    system_->maxIter = 200;

    // Declare parameters. Fixed points are NOT added to unknowns.
    // Points owned by arcs that are NOT registered in this solve are
    // pinned too: an unregistered arc has no constraints tying its
    // vertices down, so declaring them as unknowns would let the
    // solver wander their null space on every solve (the arc's
    // geometry is maintained by the rescue + enforcement passes
    // instead).  Circle radii are unknowns so the solver can adjust
    // them to satisfy radius constraints.
    std::set<std::string> pinned_arc_vertex_ids;
    for (const auto& arc : params.arcs) {
        if (arc_id_to_index_.count(arc.id) != 0) continue;
        pinned_arc_vertex_ids.insert(arc.start_vertex_id);
        pinned_arc_vertex_ids.insert(arc.end_vertex_id);
        pinned_arc_vertex_ids.insert(arc.center_vertex_id);
    }
    GCS::VEC_pD unknown_params;
    for (size_t i = 0; i < num_points; ++i) {
        if (!params.vertices[i].is_fixed &&
            pinned_arc_vertex_ids.count(params.vertices[i].id) == 0) {
            unknown_params.push_back(&param_storage_[i * 2]);     // x
            unknown_params.push_back(&param_storage_[i * 2 + 1]); // y
        }
    }
    // Circle radius parameters start after the point params.
    size_t radius_base = num_points * 2;
    for (size_t i = 0; i < num_circles; ++i) {
        unknown_params.push_back(&param_storage_[radius_base + i]);
    }
    // Arc parameters (rad, startAngle, endAngle per mapped arc) follow
    // the circle radius params.
    for (const auto& mapping : arc_param_idx_) {
        unknown_params.push_back(&param_storage_[mapping.rad_idx]);
        unknown_params.push_back(&param_storage_[mapping.start_angle_idx]);
        unknown_params.push_back(&param_storage_[mapping.end_angle_idx]);
    }
    system_->declareUnknowns(unknown_params);
