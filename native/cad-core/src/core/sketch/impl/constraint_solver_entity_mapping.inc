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
                if (line.start_vertex_id == sp.id) {
                    px = line.start_x; py = line.start_y; break;
                }
                if (line.end_vertex_id == sp.id) {
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
        auto start_it = point_id_to_index_.find(sl.start_vertex_id);
        auto end_it = point_id_to_index_.find(sl.end_vertex_id);
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
        std::string center_point_id = circle_center_vertex_id(sc);
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

    // Map user arc entities. Each arc owns three parameters — radius,
    // start angle, end angle — and shares its center/start/end points
    // with the existing point set (vertex unification), so a line
    // sharing an endpoint with an arc drags the arc along through the
    // shared GCS::Point.  addConstraintArcRules keeps the endpoints on
    // the circumcircle at their angles.
    //
    // Angles are stored so that endAngle = startAngle + signed_sweep
    // (positive for ccw, negative for cw), avoiding the ±π wrap problem
    // when a sweep crosses the atan2 boundary.
    gcs_arcs_.clear();
    gcs_arcs_.reserve(num_arcs);
    arc_param_idx_.clear();
    arc_param_idx_.reserve(num_arcs);
    arc_id_to_index_.clear();
    for (const auto& arc : params.arcs) {
        if (is_derived_arc(params, arc) ||
            !arc_participates_in_solver(params, arc)) {
            continue;
        }
        auto center_it = point_id_to_index_.find(arc.center_vertex_id);
        auto start_it = point_id_to_index_.find(arc.start_vertex_id);
        auto end_it = point_id_to_index_.find(arc.end_vertex_id);
        if (center_it == point_id_to_index_.end() ||
            start_it == point_id_to_index_.end() ||
            end_it == point_id_to_index_.end()) {
            // Missing point reference — skip this arc.
            continue;
        }

        constexpr double kPi = 3.14159265358979323846;
        const double start_angle = std::atan2(arc.start_y - arc.center_y,
                                              arc.start_x - arc.center_x);
        const double raw_sweep = std::atan2(arc.end_y - arc.center_y,
                                            arc.end_x - arc.center_x) -
                                 start_angle;
        double signed_sweep = raw_sweep;
        if (arc.ccw && signed_sweep < 0.0) signed_sweep += 2.0 * kPi;
        if (!arc.ccw && signed_sweep > 0.0) signed_sweep -= 2.0 * kPi;
        const double end_angle = start_angle + signed_sweep;

        const size_t rad_idx = param_storage_.size();
        param_storage_.push_back(arc.radius);
        param_storage_.push_back(start_angle);
        param_storage_.push_back(end_angle);

        GCS::Arc ga;
        ga.center = gcs_points_[center_it->second];
        ga.start = gcs_points_[start_it->second];
        ga.end = gcs_points_[end_it->second];
        ga.rad = &param_storage_[rad_idx];
        ga.startAngle = &param_storage_[rad_idx + 1];
        ga.endAngle = &param_storage_[rad_idx + 2];
        gcs_arcs_.push_back(ga);
        arc_param_idx_.push_back({rad_idx, rad_idx + 1, rad_idx + 2});
        arc_id_to_index_[arc.id] = gcs_arcs_.size() - 1;
    }
