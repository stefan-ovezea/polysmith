  // Phase: splines.
  const auto spline_it = std::find_if(
      params.splines.begin(), params.splines.end(),
      [&](const SketchSpline& s) { return s.id == entity_id; });

  if (spline_it != params.splines.end()) {
    const auto intersections = find_all_intersections(*spline_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           return std::find(c.target_ids.begin(),
                                            c.target_ids.end(),
                                            entity_id) != c.target_ids.end();
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.splines.erase(spline_it);
      return;
    }

    const auto segments =
        split_spline_at_intersections(*spline_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = resolve_clicked_index(
        segments,
        select_clicked_segment(segments, *spline_it, click_x, click_y));

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " +
          entity_id);
    }

    if (segments.size() == 1) {
      // Only one real piece — trimming it is a full delete (same
      // guard as the line command).
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           for (const auto& tid : c.target_ids)
                             if (tid == entity_id) return true;
                           return false;
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.splines.erase(spline_it);
      return;
    }

    const std::unordered_set<std::string> excluded_entities{entity_id};
    auto shared_pt = [&](double x, double y) {
      const auto shared = resolve_shared_point(params, x, y, excluded_entities);
      return shared.has_value() ? std::get<0>(shared.value())
                                : mint_vertex_id();
    };

    // Snapshot the ORIGINAL poles: the first rebuild mutates the
    // spline in place, so the second span must re-fit against the
    // pre-trim curve, never the already-trimmed half.
    const std::vector<double> orig_px = spline_it->pole_xs;
    const std::vector<double> orig_py = spline_it->pole_ys;
    const int orig_degree = spline_it->degree;

    // Re-fit a kept parameter span into poles in the project's
    // convention.  The span's ENDPOINT poles resolve through the
    // shared-point resolver (adopting a crossing entity's vertex or
    // minting); interior poles get fresh vertices. The two cut ends of
    // a middle-segment deletion are DIFFERENT points and must never
    // share a vertex id — the vertex-table sync would drag one piece's
    // end pole onto the other's coordinates.
    auto rebuild_span = [&](SketchSpline& out, double u0, double u1) -> bool {
      std::vector<double> px, py;
      int deg = 3;
      if (!spline_segment_poles(orig_px, orig_py, orig_degree, u0, u1, px,
                                py, deg)) {
        return false;
      }
      out.degree = deg;
      out.pole_xs = std::move(px);
      out.pole_ys = std::move(py);
      out.pole_vertex_ids.clear();
      for (size_t i = 0; i < out.pole_xs.size(); ++i) {
        if (i == 0 || i + 1 == out.pole_xs.size()) {
          out.pole_vertex_ids.push_back(
              shared_pt(out.pole_xs[i], out.pole_ys[i]));
        } else {
          out.pole_vertex_ids.push_back(mint_vertex_id());
        }
      }
      return true;
    };

    const int last = static_cast<int>(segments.size()) - 1;

    if (clicked_index == 0) {
      // Deleted the head piece — keep [first boundary, 1].
      if (!rebuild_span(*spline_it, segments[1].param_start, 1.0)) {
        throw std::runtime_error("Spline trim: segment re-fit failed");
      }
    } else if (clicked_index == last) {
      // Deleted the tail piece — keep [0, last boundary].
      if (!rebuild_span(*spline_it, 0.0,
                        segments[clicked_index - 1].param_end)) {
        throw std::runtime_error("Spline trim: segment re-fit failed");
      }
    } else {
      // Middle piece deleted — split into two splines. Their cut ends
      // are two DIFFERENT points; each resolves its own vertex.
      const double left_end = segments[clicked_index - 1].param_end;
      const double right_start = segments[clicked_index + 1].param_start;
      if (!rebuild_span(*spline_it, 0.0, left_end)) {
        throw std::runtime_error("Spline trim: segment re-fit failed");
      }

      std::vector<std::string> spline_ids;
      spline_ids.reserve(params.splines.size());
      for (const auto& s : params.splines) spline_ids.push_back(s.id);
      const int next_idx = next_trim_entity_index(spline_ids);
      SketchSpline right;
      right.id = "spline-" + std::to_string(next_idx);
      right.is_construction = spline_it->is_construction;
      if (!rebuild_span(right, right_start, 1.0)) {
        throw std::runtime_error("Spline trim: segment re-fit failed");
      }
      params.splines.push_back(std::move(right));
    }

    // Splines carry no constraints today; erase anything referencing
    // the id as a safety net (mirrors the line phase).
    params.constraints.erase(
        std::remove_if(params.constraints.begin(), params.constraints.end(),
                       [&](const SketchConstraint& c) {
                         for (const auto& tid : c.target_ids)
                           if (tid == entity_id) return true;
                         return false;
                       }),
        params.constraints.end());
    params.dimensions.erase(
        std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                       [&](const SketchDimension& d) {
                         return d.entity_id == entity_id;
                       }),
        params.dimensions.end());

    return;
  }
