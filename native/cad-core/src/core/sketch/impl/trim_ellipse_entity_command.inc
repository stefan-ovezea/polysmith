  // Phase: ellipses.
  const auto ellipse_it = std::find_if(
      params.ellipses.begin(), params.ellipses.end(),
      [&](const SketchEllipse& e) { return e.id == entity_id; });

  if (ellipse_it != params.ellipses.end()) {
    const auto intersections = find_all_intersections(*ellipse_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      trim_remove_entity_attachments(
          params, entity_id,
          {ellipse_it->center_vertex_id, ellipse_it->axis_a_vertex_id,
           ellipse_it->axis_b_vertex_id, ellipse_it->start_vertex_id,
           ellipse_it->end_vertex_id});
      params.ellipses.erase(ellipse_it);
      return;
    }

    const auto segments =
        split_ellipse_at_intersections(*ellipse_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = resolve_clicked_index(
        segments,
        select_clicked_segment(segments, *ellipse_it, click_x, click_y));

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " +
          entity_id);
    }

    // A single surviving segment means every intersection sits on the
    // ellipse's own endpoints — trimming the only real piece is a full
    // delete for BOTH full and partial ellipses (the old code let the
    // partial case fall through to the arc-style branch below and
    // read `segments[1]` on a size-1 vector — undefined behaviour
    // that silently corrupted the ellipse instead of deleting it).
    if (segments.size() == 1) {
      trim_remove_entity_attachments(
          params, entity_id,
          {ellipse_it->center_vertex_id, ellipse_it->axis_a_vertex_id,
           ellipse_it->axis_b_vertex_id, ellipse_it->start_vertex_id,
           ellipse_it->end_vertex_id});
      params.ellipses.erase(ellipse_it);
      return;
    }

    const std::string old_sp_id = ellipse_it->start_vertex_id;
    const std::string old_ep_id = ellipse_it->end_vertex_id;
    const std::string old_center_id = ellipse_it->center_vertex_id;
    const std::string old_axis_a_id = ellipse_it->axis_a_vertex_id;
    const std::string old_axis_b_id = ellipse_it->axis_b_vertex_id;
    // Filled by the middle-split branch with the new ellipse's id.
    std::string new_ellipse_id;

    // D1 capture BEFORE the cleanup erases the coincident constraints
    // on the ellipse's points.
    const auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id,
        {old_sp_id, old_ep_id, old_center_id, old_axis_a_id, old_axis_b_id});

    const std::unordered_set<std::string> excluded_entities{entity_id};
    auto shared_pt = [&](double x, double y) {
      const auto shared = resolve_shared_point(params, x, y, excluded_entities);
      return shared.has_value() ? std::get<0>(shared.value())
                                : mint_vertex_id();
    };
    auto wrap_angle = [](double a) {
      const double k2Pi = 2.0 * M_PI;
      while (a < 0.0) a += k2Pi;
      while (a >= k2Pi) a -= k2Pi;
      return a;
    };

    const int last = static_cast<int>(segments.size()) - 1;

    if (!ellipse_it->has_sweep) {
      // Full ellipse → one complementary elliptical arc, circle-style:
      // from the segment after the clicked one CCW the long way back
      // to the clicked segment's start.
      const int N = static_cast<int>(segments.size());
      const int next = (clicked_index + 1) % N;
      double a_start = segments[next].param_start;
      double a_end = segments[clicked_index].param_start;
      if (a_end <= a_start) a_end += 2.0 * M_PI;
      ellipse_it->has_sweep = true;
      ellipse_it->ccw = true;
      ellipse_it->sweep_start_angle = wrap_angle(a_start);
      ellipse_it->sweep_end_angle = wrap_angle(a_end);
      ellipse_it->start_vertex_id =
          shared_pt(segments[next].start_x, segments[next].start_y);
      ellipse_it->end_vertex_id =
          shared_pt(segments[clicked_index].start_x,
                    segments[clicked_index].start_y);
    } else if (clicked_index == 0) {
      ellipse_it->sweep_start_angle =
          wrap_angle(segments[1].param_start);
      ellipse_it->start_vertex_id =
          shared_pt(segments[1].start_x, segments[1].start_y);
    } else if (clicked_index == last) {
      ellipse_it->sweep_end_angle =
          wrap_angle(segments[clicked_index - 1].param_end);
      ellipse_it->end_vertex_id = shared_pt(segments[clicked_index - 1].end_x,
                                            segments[clicked_index - 1].end_y);
    } else {
      // Middle segment deleted — the elliptical arc splits into two.
      ellipse_it->sweep_end_angle =
          wrap_angle(segments[clicked_index - 1].param_end);
      ellipse_it->end_vertex_id = shared_pt(segments[clicked_index - 1].end_x,
                                            segments[clicked_index - 1].end_y);

      std::vector<std::string> ellipse_ids;
      ellipse_ids.reserve(params.ellipses.size());
      for (const auto& e : params.ellipses) ellipse_ids.push_back(e.id);
      const int next_idx = next_trim_entity_index(ellipse_ids);
      new_ellipse_id = "ellipse-" + std::to_string(next_idx);
      const auto& right = segments[clicked_index + 1];
      const auto& last_seg = segments[last];
      params.ellipses.push_back(SketchEllipse{
          .id = "ellipse-" + std::to_string(next_idx),
          .center_vertex_id = ellipse_it->center_vertex_id,
          .axis_a_vertex_id = ellipse_it->axis_a_vertex_id,
          .axis_b_vertex_id = ellipse_it->axis_b_vertex_id,
          .center_x = ellipse_it->center_x,
          .center_y = ellipse_it->center_y,
          .a = ellipse_it->a,
          .b = ellipse_it->b,
          .rotation = ellipse_it->rotation,
          .is_construction = ellipse_it->is_construction,
          .has_sweep = true,
          .sweep_start_angle = wrap_angle(right.param_start),
          .sweep_end_angle = wrap_angle(last_seg.param_end),
          .ccw = ellipse_it->ccw,
          .start_vertex_id = shared_pt(right.start_x, right.start_y),
          .end_vertex_id = shared_pt(last_seg.end_x, last_seg.end_y),
      });
    }

    // Clean up every attachment of the ellipse through the shared
    // pipeline (the old code erased constraints + primary-side
    // dimensions only — relations, anchors, fillets and chamfers
    // leaked).
    trim_remove_entity_attachments(
        params, entity_id,
        {old_sp_id, old_ep_id, old_center_id, old_axis_a_id,
         old_axis_b_id});

    // Safety net: delete coincident constraints whose merged point
    // died or whose listed lines no longer reference it.
    sweep_orphan_coincident_constraints(params);

    // D1 transfer: point-on-object anchors at the cuts and coincident
    // survival on the kept piece(s) (ellipses carry no dimensions or
    // line relations). The cut coordinates derive from the sweep
    // angles the branches above just wrote (the ellipse stores no
    // endpoint coords).
    {
      std::vector<std::string> survivors{entity_id};
      std::vector<TrimTransferCut> cuts;
      const auto add_cut = [&](const std::string& pid, double t, double param) {
        const std::string* cutter = cutter_for_param(intersections, param);
        const auto [px, py] = transfer_ellipse_point(*ellipse_it, t);
        cuts.push_back(TrimTransferCut{
            px, py, pid, cutter != nullptr ? *cutter : ""});
      };
      if (!ellipse_it->has_sweep) {
        // Full ellipse → complementary arc: BOTH endpoints move — the
        // new start at the segment after the clicked one, the new end
        // at the clicked segment's start.
        const int N = static_cast<int>(segments.size());
        add_cut(ellipse_it->start_vertex_id, ellipse_it->sweep_start_angle,
                segments[(clicked_index + 1) % N].param_start);
        add_cut(ellipse_it->end_vertex_id, ellipse_it->sweep_end_angle,
                segments[clicked_index].param_start);
      } else if (clicked_index == 0) {
        add_cut(ellipse_it->start_vertex_id, ellipse_it->sweep_start_angle,
                segments[1].param_start);
      } else if (clicked_index == last) {
        add_cut(ellipse_it->end_vertex_id, ellipse_it->sweep_end_angle,
                segments[clicked_index - 1].param_end);
      } else {
        add_cut(ellipse_it->end_vertex_id, ellipse_it->sweep_end_angle,
                segments[clicked_index - 1].param_end);
        if (!new_ellipse_id.empty()) {
          const auto right_ellipse = std::find_if(
              params.ellipses.begin(), params.ellipses.end(),
              [&](const SketchEllipse& e) { return e.id == new_ellipse_id; });
          if (right_ellipse != params.ellipses.end()) {
            survivors.push_back(new_ellipse_id);
            const auto [px, py] = transfer_ellipse_point(
                *right_ellipse, right_ellipse->sweep_start_angle);
            const std::string* cutter =
                cutter_for_param(intersections,
                                 segments[clicked_index + 1].param_start);
            cuts.push_back(TrimTransferCut{
                px, py, right_ellipse->start_vertex_id,
                cutter != nullptr ? *cutter : ""});
          }
        }
      }
      apply_trim_constraint_transfer(params, transfer_plan, survivors, cuts);
    }

    return;
  }
