  // Phase: splines.
  const auto spline_it = std::find_if(
      params.splines.begin(), params.splines.end(),
      [&](const SketchSpline& s) { return s.id == entity_id; });

  if (spline_it != params.splines.end()) {
    const auto intersections = find_all_intersections(*spline_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      trim_remove_entity_attachments(params, entity_id,
                                     spline_it->pole_vertex_ids);
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
      trim_remove_entity_attachments(params, entity_id,
                                     spline_it->pole_vertex_ids);
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
    const std::vector<std::string> old_pole_ids = spline_it->pole_vertex_ids;

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

    // Re-fit into TEMPORARIES and commit only when every span
    // succeeds. The document layer pushed the undo state before this
    // engine call; the old code mutated the spline in place and could
    // throw from the second span's re-fit, leaving the sketch
    // half-cut behind an undo entry that restored the wrong baseline.
    SketchSpline left;
    SketchSpline right;
    bool have_right = false;
    bool ok = true;
    if (clicked_index == 0) {
      // Deleted the head piece — keep [first boundary, 1].
      ok = rebuild_span(left, segments[1].param_start, 1.0);
    } else if (clicked_index == last) {
      // Deleted the tail piece — keep [0, last boundary].
      ok = rebuild_span(left, 0.0, segments[clicked_index - 1].param_end);
    } else {
      // Middle piece deleted — split into two splines. Their cut ends
      // are two DIFFERENT points; each resolves its own vertex.
      const double left_end = segments[clicked_index - 1].param_end;
      const double right_start = segments[clicked_index + 1].param_start;
      ok = rebuild_span(left, 0.0, left_end);
      have_right = true;
      if (ok) {
        std::vector<std::string> spline_ids;
        spline_ids.reserve(params.splines.size());
        for (const auto& s : params.splines) spline_ids.push_back(s.id);
        const int next_idx = next_trim_entity_index(spline_ids);
        right.id = "spline-" + std::to_string(next_idx);
        ok = rebuild_span(right, right_start, 1.0);
      }
    }
    if (!ok) {
      throw std::runtime_error("Spline trim: segment re-fit failed");
    }
    left.id = spline_it->id;
    left.is_construction = spline_it->is_construction;
    left.generated_by = spline_it->generated_by;
    *spline_it = std::move(left);
    if (have_right) {
      right.is_construction = spline_it->is_construction;
      right.generated_by = spline_it->generated_by;
      params.splines.push_back(std::move(right));
    }

    // D1 capture BEFORE the cleanup erases the coincident constraints
    // on the spline's poles.
    const auto transfer_plan =
        capture_trim_constraint_transfer(params, entity_id, old_pole_ids);

    // Clean up every attachment through the shared pipeline (the old
    // code erased constraints + primary-side dimensions only).
    trim_remove_entity_attachments(params, entity_id, old_pole_ids);

    // D1 transfer: coincident survival on the kept span(s) only
    // (splines carry no H/V, relations or dimensions).
    {
      std::vector<std::string> survivors{entity_id};
      if (have_right && !right.id.empty()) survivors.push_back(right.id);
      apply_trim_constraint_transfer(params, transfer_plan, survivors, {});
    }

    return;
  }
