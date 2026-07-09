  // Phase 3: arcs — not yet supported.
  const auto arc_it = std::find_if(
      params.arcs.begin(),
      params.arcs.end(),
      [&](const SketchArc& a) { return a.id == entity_id; });

  if (arc_it != params.arcs.end()) {
    const auto intersections = find_all_intersections(*arc_it, params);

    if (intersections.empty()) {
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           return std::find(c.target_ids.begin(), c.target_ids.end(),
                                            entity_id) != c.target_ids.end();
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.arcs.erase(arc_it);
      return;
    }

    const auto segments = split_arc_at_intersections(*arc_it, intersections);
    if (segments.empty()) return;

    const std::string old_sp_id = arc_it->start_vertex_id;
    const std::string old_ep_id = arc_it->end_vertex_id;

    const int clicked_index = select_clicked_segment(
        segments, *arc_it, click_x, click_y);

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Delete the clicked segment — keep from arc start to segment
    // boundary, or from boundary to arc end.
    const int last = static_cast<int>(segments.size()) - 1;
    auto arc_shared_pt = [&](double x, double y, const std::string& fallback) {
      const auto shared = find_coincident_endpoint(params, entity_id, x, y);
      return shared.has_value() ? std::get<0>(shared.value()) : fallback;
    };

    if (clicked_index == 0) {
      arc_it->start_x = segments[1].start_x;
      arc_it->start_y = segments[1].start_y;
      arc_it->start_vertex_id = arc_shared_pt(
          arc_it->start_x, arc_it->start_y,
          "vertex-" + std::to_string(params.next_vertex_index++));
    } else if (clicked_index == last) {
      arc_it->end_x = segments[clicked_index - 1].end_x;
      arc_it->end_y = segments[clicked_index - 1].end_y;
      arc_it->end_vertex_id = arc_shared_pt(
          arc_it->end_x, arc_it->end_y,
          "vertex-" + std::to_string(params.next_vertex_index++));
    } else {
      // Middle segment deleted — arc splits into two.
      arc_it->end_x = segments[clicked_index - 1].end_x;
      arc_it->end_y = segments[clicked_index - 1].end_y;
      arc_it->end_vertex_id = arc_shared_pt(
          arc_it->end_x, arc_it->end_y,
          "vertex-" + std::to_string(params.next_vertex_index++));

      const int next_idx = static_cast<int>(params.arcs.size()) + 1;
      const auto& right = segments[clicked_index + 1];
      const auto& last_seg = segments[last];
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(next_idx),
          .start_vertex_id = arc_shared_pt(
              right.start_x, right.start_y,
              "vertex-" + std::to_string(params.next_vertex_index++)),
          .end_vertex_id   = arc_shared_pt(
              last_seg.end_x, last_seg.end_y,
              "vertex-" + std::to_string(params.next_vertex_index++)),
          .center_x = arc_it->center_x,
          .center_y = arc_it->center_y,
          .radius   = arc_it->radius,
          .start_x  = right.start_x,
          .start_y  = right.start_y,
          .end_x    = last_seg.end_x,
          .end_y    = last_seg.end_y,
          .ccw      = arc_it->ccw,
          .is_construction = arc_it->is_construction,
      });
    }

    // Clean up constraints/dimensions/relations/fillets on the arc.
    params.constraints.erase(
        std::remove_if(params.constraints.begin(), params.constraints.end(),
                       [&](const SketchConstraint& c) {
                         for (const auto& tid : c.target_ids)
                           if (tid == entity_id || tid == old_sp_id || tid == old_ep_id)
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
    params.line_relations.erase(
        std::remove_if(params.line_relations.begin(), params.line_relations.end(),
                       [&](const SketchLineRelation& r) {
                         return r.first_line_id == entity_id ||
                                r.second_line_id == entity_id;
                       }),
        params.line_relations.end());
    params.fillets.erase(
        std::remove_if(params.fillets.begin(), params.fillets.end(),
                       [&](const SketchFillet& f) {
                         return f.line_a_id == entity_id ||
                                f.line_b_id == entity_id ||
                                f.arc_id == entity_id ||
                                f.corner_point_id == old_sp_id ||
                                f.corner_point_id == old_ep_id ||
                                f.trim_a_point_id == old_sp_id ||
                                f.trim_a_point_id == old_ep_id ||
                                f.trim_b_point_id == old_sp_id ||
                                 f.trim_b_point_id == old_ep_id;
                       }),
        params.fillets.end());

    // Safety net: delete coincident constraints referencing orphaned point IDs.
    {
      std::unordered_set<std::string> live_pt;
      for (const auto& l : params.lines) {
        live_pt.insert(l.start_vertex_id); live_pt.insert(l.end_vertex_id);
      }
      for (const auto& c : params.circles) {
        live_pt.insert(circle_center_vertex_id(c));
        // Quadrants still use legacy format (Phase 4.1 will update)
        live_pt.insert("vertex-" + c.id + "-quadrant-0");
        live_pt.insert("vertex-" + c.id + "-quadrant-1");
        live_pt.insert("vertex-" + c.id + "-quadrant-2");
        live_pt.insert("vertex-" + c.id + "-quadrant-3");
      }
      for (const auto& a : params.arcs) {
        live_pt.insert(a.start_vertex_id); live_pt.insert(a.end_vertex_id);
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
