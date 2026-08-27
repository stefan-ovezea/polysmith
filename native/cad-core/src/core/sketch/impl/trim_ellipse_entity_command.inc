  // Phase: ellipses.
  const auto ellipse_it = std::find_if(
      params.ellipses.begin(), params.ellipses.end(),
      [&](const SketchEllipse& e) { return e.id == entity_id; });

  if (ellipse_it != params.ellipses.end()) {
    const auto intersections = find_all_intersections(*ellipse_it, params);

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

    // A single surviving segment on a FULL ellipse means every
    // intersection sits at the same point (tangent cluster): trimming
    // the only real piece is a full delete. Partial ellipses with one
    // piece keep the arc-style path below (they can shrink to it).
    if (!ellipse_it->has_sweep && segments.size() == 1) {
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
      params.ellipses.erase(ellipse_it);
      return;
    }

    const std::string old_sp_id = ellipse_it->start_vertex_id;
    const std::string old_ep_id = ellipse_it->end_vertex_id;

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

    // Clean up constraints/dimensions on the ellipse (and its old
    // partial-arc endpoint ids) — same conservative policy as arcs.
    params.constraints.erase(
        std::remove_if(params.constraints.begin(), params.constraints.end(),
                       [&](const SketchConstraint& c) {
                         for (const auto& tid : c.target_ids)
                           if (tid == entity_id || tid == old_sp_id ||
                               tid == old_ep_id)
                             return true;
                         return false;
                       }),
        params.constraints.end());
    params.dimensions.erase(
        std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                       [&](const SketchDimension& d) {
                         return d.entity_id == entity_id;
                       }),
        params.dimensions.end());

    // Safety net: delete coincident constraints referencing orphaned
    // point ids (mirrors the arc phase).
    {
      std::unordered_set<std::string> live_pt;
      for (const auto& l : params.lines) {
        live_pt.insert(l.start_vertex_id);
        live_pt.insert(l.end_vertex_id);
      }
      for (const auto& c : params.circles) {
        live_pt.insert(circle_center_vertex_id(c));
        live_pt.insert("vertex-" + c.id + "-quadrant-0");
        live_pt.insert("vertex-" + c.id + "-quadrant-1");
        live_pt.insert("vertex-" + c.id + "-quadrant-2");
        live_pt.insert("vertex-" + c.id + "-quadrant-3");
      }
      for (const auto& a : params.arcs) {
        live_pt.insert(a.start_vertex_id);
        live_pt.insert(a.end_vertex_id);
      }
      for (const auto& e : params.ellipses) {
        live_pt.insert(e.center_vertex_id);
        live_pt.insert(e.axis_a_vertex_id);
        live_pt.insert(e.axis_b_vertex_id);
        live_pt.insert(e.start_vertex_id);
        live_pt.insert(e.end_vertex_id);
      }
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           if (c.kind != "coincident") return false;
                           for (const auto& tid : c.target_ids)
                             if (!live_pt.count(tid)) return true;
                           return false;
                         }),
          params.constraints.end());
    }

    return;
  }
