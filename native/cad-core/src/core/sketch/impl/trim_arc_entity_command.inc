  // Phase 3: arcs — not yet supported.
  const auto arc_it = std::find_if(
      params.arcs.begin(),
      params.arcs.end(),
      [&](const SketchArc& a) { return a.id == entity_id; });

  if (arc_it != params.arcs.end()) {
    const auto intersections = find_all_intersections(*arc_it, params);

    if (intersections.empty()) {
      trim_remove_entity_attachments(
          params, entity_id,
          {arc_it->start_vertex_id, arc_it->end_vertex_id,
           arc_it->center_vertex_id});
      params.arcs.erase(arc_it);
      return;
    }

    const auto segments = split_arc_at_intersections(*arc_it, intersections);
    if (segments.empty()) return;

    // A single surviving segment means every intersection sits on the
    // arc's own endpoints (e.g. two lines crossing the arc exactly at
    // its ends): trimming the only real piece is a full delete — the
    // line and spline paths carry the same guard. The old code fell
    // into `segments[1]` on a size-1 vector here — undefined
    // behaviour that silently corrupted the arc instead of deleting
    // it ("the trim action silently goes away").
    if (segments.size() == 1) {
      trim_remove_entity_attachments(
          params, entity_id,
          {arc_it->start_vertex_id, arc_it->end_vertex_id,
           arc_it->center_vertex_id});
      params.arcs.erase(arc_it);
      return;
    }

    // Capture the old point ids before the middle-segment push_back
    // can invalidate the arc iterator.
    const std::string old_sp_id = arc_it->start_vertex_id;
    const std::string old_ep_id = arc_it->end_vertex_id;
    const std::string old_center_id = arc_it->center_vertex_id;
    // Filled by the middle-split branch with the new arc's id.
    std::string new_arc_id;

    // D1 capture BEFORE the cleanup erases the whitelisted
    // attachments (arc dims, coincident).
    const auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id, {old_sp_id, old_ep_id, old_center_id});

    const int clicked_index = resolve_clicked_index(
        segments, select_clicked_segment(segments, *arc_it, click_x, click_y));

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Delete the clicked segment — keep from arc start to segment
    // boundary, or from boundary to arc end.
    const int last = static_cast<int>(segments.size()) - 1;
    const std::unordered_set<std::string> excluded_entities{entity_id};
    auto arc_shared_pt = [&](double x, double y) {
      const auto shared = resolve_shared_point(params, x, y, excluded_entities);
      return shared.has_value() ? std::get<0>(shared.value()) : mint_vertex_id();
    };

    if (clicked_index == 0) {
      arc_it->start_x = segments[1].start_x;
      arc_it->start_y = segments[1].start_y;
      arc_it->start_vertex_id = arc_shared_pt(arc_it->start_x, arc_it->start_y);
    } else if (clicked_index == last) {
      arc_it->end_x = segments[clicked_index - 1].end_x;
      arc_it->end_y = segments[clicked_index - 1].end_y;
      arc_it->end_vertex_id = arc_shared_pt(arc_it->end_x, arc_it->end_y);
    } else {
      // Middle segment deleted — arc splits into two.
      arc_it->end_x = segments[clicked_index - 1].end_x;
      arc_it->end_y = segments[clicked_index - 1].end_y;
      arc_it->end_vertex_id = arc_shared_pt(arc_it->end_x, arc_it->end_y);

      std::vector<std::string> arc_ids;
      arc_ids.reserve(params.arcs.size());
      for (const auto& arc : params.arcs) arc_ids.push_back(arc.id);
      const int next_idx = next_trim_entity_index(arc_ids);
      new_arc_id = "arc-" + std::to_string(next_idx);
      const auto& right = segments[clicked_index + 1];
      const auto& last_seg = segments[last];
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(next_idx),
          .start_vertex_id = arc_shared_pt(right.start_x, right.start_y),
          .end_vertex_id   = arc_shared_pt(last_seg.end_x, last_seg.end_y),
          // The split keeps the source arc's center — share its center
          // vertex id instead of minting a second one at the same spot.
          .center_vertex_id = arc_it->center_vertex_id,
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

    // Clean up every attachment of the arc — constraints, dimensions
    // (including secondary-side references), relations, anchors,
    // fillets and chamfers — through the one shared pipeline.
    trim_remove_entity_attachments(
        params, entity_id, {old_sp_id, old_ep_id, old_center_id});

    // Safety net: delete coincident constraints whose merged point
    // died or whose listed lines no longer reference it.
    sweep_orphan_coincident_constraints(params);

    // D1 transfer: driven dimension re-derivation, point-on-object
    // anchors at the cuts, and coincident survival on the kept
    // piece(s).
    {
      std::vector<std::string> survivors{entity_id};
      std::vector<TrimTransferCut> cuts;
      const std::string* cutter = nullptr;
      if (clicked_index == 0) {
        cutter = cutter_for_param(intersections, segments[1].param_start);
        cuts.push_back(TrimTransferCut{
            arc_it->start_x, arc_it->start_y, arc_it->start_vertex_id,
            cutter != nullptr ? *cutter : ""});
      } else if (clicked_index == last) {
        cutter =
            cutter_for_param(intersections,
                             segments[clicked_index - 1].param_end);
        cuts.push_back(TrimTransferCut{
            arc_it->end_x, arc_it->end_y, arc_it->end_vertex_id,
            cutter != nullptr ? *cutter : ""});
      } else {
        cutter =
            cutter_for_param(intersections,
                             segments[clicked_index - 1].param_end);
        cuts.push_back(TrimTransferCut{
            arc_it->end_x, arc_it->end_y, arc_it->end_vertex_id,
            cutter != nullptr ? *cutter : ""});
        if (!new_arc_id.empty()) {
          const auto right_arc = std::find_if(
              params.arcs.begin(), params.arcs.end(),
              [&](const SketchArc& a) { return a.id == new_arc_id; });
          if (right_arc != params.arcs.end()) {
            survivors.push_back(new_arc_id);
            cutter =
                cutter_for_param(intersections,
                                 segments[clicked_index + 1].param_start);
            cuts.push_back(TrimTransferCut{
                right_arc->start_x, right_arc->start_y,
                right_arc->start_vertex_id,
                cutter != nullptr ? *cutter : ""});
          }
        }
      }
      apply_trim_constraint_transfer(params, transfer_plan, survivors, cuts);
    }

    return;
  }
