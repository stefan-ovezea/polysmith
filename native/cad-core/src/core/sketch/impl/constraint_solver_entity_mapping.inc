    // Fill parameter storage. For each sketch point, read the current
    // coordinate from the owning line/circle if possible; fall back to
    // the point's stored value.
    for (size_t i = 0; i < num_points; ++i) {
        const auto& sp = params.vertices[i];
        double px = sp.x;
        double py = sp.y;

        // If this point is a line endpoint or circle center, read the
        // current coordinate from that entity — the point vector may
        // be stale between rebuild_sketch_vertices calls.
        if (sp.kind == "endpoint") {
            for (const auto& line : params.lines) {
                if (line.start_point_id == sp.id) {
                    px = line.start_x; py = line.start_y; break;
                }
                if (line.end_point_id == sp.id) {
                    px = line.end_x; py = line.end_y; break;
                }
            }
        } else if (sp.kind == "center") {
            // Phase 4.2: use geometry_owner_ids to find owning entity
            if (const auto* circle = find_circle_for_point(params, sp.id)) {
                px = circle->center_x; py = circle->center_y;
            }
        }

        param_storage_.push_back(px);
        param_storage_.push_back(py);

        GCS::Point gp;
        gp.x = &param_storage_[i * 2];
        gp.y = &param_storage_[i * 2 + 1];
        gcs_points_.push_back(gp);
        point_id_to_index_[sp.id] = i;
    }

    // Map line entities.
    gcs_lines_.clear();
    gcs_lines_.reserve(params.lines.size());
    line_id_to_index_.clear();
    for (size_t i = 0; i < params.lines.size(); ++i) {
        const auto& sl = params.lines[i];
        auto start_it = point_id_to_index_.find(sl.start_point_id);
        auto end_it = point_id_to_index_.find(sl.end_point_id);
        if (start_it == point_id_to_index_.end() ||
            end_it == point_id_to_index_.end()) {
            // Missing point reference — skip this line.
            continue;
        }
        GCS::Line gl;
        gl.p1 = gcs_points_[start_it->second];
        gl.p2 = gcs_points_[end_it->second];
        gcs_lines_.push_back(gl);
        line_id_to_index_[sl.id] = gcs_lines_.size() - 1;
    }

    // Map circle entities.
    gcs_circles_.clear();
    gcs_circles_.reserve(params.circles.size());
    circle_id_to_index_.clear();
    for (size_t i = 0; i < params.circles.size(); ++i) {
        const auto& sc = params.circles[i];
        // Compute the radius from the stored value.
        // The circle's center is a sketch point referenced by the circle.
        // For now, use a dedicated parameter for the radius.
        param_storage_.push_back(sc.radius);

        GCS::Circle gc;
        // Find the center point via the shared vertex-lookup helper.
        std::string center_point_id = circle_center_point_id(sc);
        auto center_it = point_id_to_index_.find(center_point_id);
        if (center_it != point_id_to_index_.end()) {
            gc.center = gcs_points_[center_it->second];
        } else {
            // Fallback: use the circle's stored center coordinates
            // by creating an anonymous point in param_storage_.
            // This shouldn't normally happen.
            param_storage_.push_back(sc.center_x);
            param_storage_.push_back(sc.center_y);
            GCS::Point anon_center;
            anon_center.x = &param_storage_[param_storage_.size() - 2];
            anon_center.y = &param_storage_[param_storage_.size() - 1];
            gc.center = anon_center;
        }
        gc.rad = &param_storage_[param_storage_.size() - 3]; // radius param
        gcs_circles_.push_back(gc);
        circle_id_to_index_[sc.id] = gcs_circles_.size() - 1;
    }
