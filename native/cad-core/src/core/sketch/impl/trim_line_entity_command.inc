  // Phase 1: lines.
  // Non-const: the middle-segment split re-locates this iterator after
  // push_back (which may invalidate it).
  auto line_it = std::find_if(
      params.lines.begin(),
      params.lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });

  if (line_it != params.lines.end()) {
    const auto intersections = find_all_intersections(*line_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      trim_remove_entity_attachments(
          params, entity_id,
          {line_it->start_vertex_id, line_it->end_vertex_id});
      params.lines.erase(line_it);
      return;
    }

    const auto segments = split_line_at_intersections(*line_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = resolve_clicked_index(
        segments, select_clicked_segment(segments, *line_it, click_x, click_y));

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // The splitters drop pieces shorter than kTrimCoincidentTolerance,
    // so a single surviving segment means every intersection sits on
    // the line's own endpoints: trimming the only real piece is a full
    // delete. The coordinate mutation below assumes two neighbouring
    // segments exist and would index out of bounds here.
    if (segments.size() == 1) {
      trim_remove_entity_attachments(
          params, entity_id,
          {line_it->start_vertex_id, line_it->end_vertex_id});
      params.lines.erase(line_it);
      return;
    }

    // Capture old point IDs *before* vector mutations — the cleanup
    // helper below runs after push_back and must not read line_it.
    const std::string old_sp_id = line_it->start_vertex_id;
    const std::string old_ep_id = line_it->end_vertex_id;
    // Filled by the middle-split branch with the right piece's id
    // (empty otherwise) — used by the constraint transfer below.
    std::string new_line_id;

    // Delete the clicked segment.
    const int last = static_cast<int>(segments.size()) - 1;

    if (clicked_index == 0) {
      line_it->start_x = segments[1].start_x;
      line_it->start_y = segments[1].start_y;
    } else if (clicked_index == last) {
      line_it->end_x = segments[clicked_index - 1].end_x;
      line_it->end_y = segments[clicked_index - 1].end_y;
    }
    if (points_match(line_it->start_x, line_it->start_y,
                     line_it->end_x, line_it->end_y)) {
      log_warn("trim", "trim produced zero-length line '" + line_it->id +
                           "' — deleting");
      params.lines.erase(line_it);
      return;
    }
    if (clicked_index > 0 && clicked_index < last) {
      // Middle segment deleted — line splits into two.
      // Left portion: original line shortened to intersection before deleted segment.
      line_it->end_x = segments[clicked_index - 1].end_x;
      line_it->end_y = segments[clicked_index - 1].end_y;

      // Right portion: new line from intersection after deleted segment to end.
      std::vector<std::string> line_ids;
      line_ids.reserve(params.lines.size());
      for (const auto& line : params.lines) line_ids.push_back(line.id);
      const int next_index = next_trim_entity_index(line_ids);
      const double rsx = segments[clicked_index + 1].start_x;
      const double rsy = segments[clicked_index + 1].start_y;
      const double rex = segments[last].end_x;
      const double rey = segments[last].end_y;

      // The split point is an intersection another entity may already
      // own: adopt its vertex id so the wire walk welds the two curves
      // through a shared id. Exclude the entity under construction and
      // its sibling so neither can match its own (not yet final)
      // endpoint — the "works only by iteration-order luck" bug.
      const std::unordered_set<std::string> excluded{entity_id};
      const auto shared_split =
          resolve_shared_point(params, rsx, rsy, excluded);
      const std::string split_id =
          shared_split.has_value() ? std::get<0>(shared_split.value())
                                   : mint_vertex_id();
      const auto shared_far =
          resolve_shared_point(params, rex, rey, excluded);
      const std::string far_id =
          shared_far.has_value() ? std::get<0>(shared_far.value())
                                 : mint_vertex_id();

      params.lines.push_back(SketchLine{
          .id = "line-" + std::to_string(next_index),
          .start_vertex_id = split_id,
          .end_vertex_id   = far_id,
          .start_x = rsx,
          .start_y = rsy,
          .end_x = rex,
          .end_y = rey,
          .constraint = std::nullopt,
          .is_construction = line_it->is_construction,
      });
      auto& new_line = params.lines.back();
      new_line_id = new_line.id;

      if (points_match(new_line.start_x, new_line.start_y,
                       new_line.end_x, new_line.end_y)) {
        log_warn("trim", "trim split produced zero-length line '" +
                             new_line.id + "' — dropping");
        params.lines.pop_back();
      }

      // push_back may have reallocated the line vector — re-locate the
      // iterator before ANY further read (the old code dereferenced the
      // invalidated iterator in the collapsed-left check below).
      line_it = std::find_if(
          params.lines.begin(), params.lines.end(),
          [&](const SketchLine& line) { return line.id == entity_id; });

      if (points_match(line_it->start_x, line_it->start_y,
                       line_it->end_x, line_it->end_y)) {
        log_warn("trim", "trim split collapsed left portion '" +
                             line_it->id + "' — deleting");
        params.lines.erase(line_it);
        return;
      }
    }

    // D1 capture BEFORE the cleanup erases the whitelisted attachments.
    const auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id, {old_sp_id, old_ep_id});

    trim_remove_entity_attachments(params, entity_id, {old_sp_id, old_ep_id});

    #include "core/sketch/impl/trim_line_point_rebind.inc"

    #include "core/sketch/impl/trim_line_orphan_constraint_cleanup.inc"

    // D1/D4 transfer: H/V, whitelisted relations, point-on-object
    // anchors at the cuts, and driven dimension re-derivation on the
    // surviving pieces (replaces the old unconditional H/V clear).
    // NOTE: the cut point ids are read AFTER the rebind above — it
    // re-assigns the trimmed line's own endpoint ids (adopted/minted),
    // so the pre-rebind ids (old_sp_id/old_ep_id) are dead here.
    {
      std::vector<std::string> survivors{entity_id};
      std::vector<TrimTransferCut> cuts;
      const std::string* cutter = nullptr;
      if (clicked_index == 0) {
        cutter = cutter_for_param(intersections, segments[1].param_start);
        cuts.push_back(TrimTransferCut{
            line_it->start_x, line_it->start_y, line_it->start_vertex_id,
            cutter != nullptr ? *cutter : ""});
      } else if (clicked_index == last) {
        cutter =
            cutter_for_param(intersections,
                             segments[clicked_index - 1].param_end);
        cuts.push_back(TrimTransferCut{
            line_it->end_x, line_it->end_y, line_it->end_vertex_id,
            cutter != nullptr ? *cutter : ""});
      } else {
        cutter =
            cutter_for_param(intersections,
                             segments[clicked_index - 1].param_end);
        cuts.push_back(TrimTransferCut{
            line_it->end_x, line_it->end_y, line_it->end_vertex_id,
            cutter != nullptr ? *cutter : ""});
        if (!new_line_id.empty()) {
          const auto right_it = std::find_if(
              params.lines.begin(), params.lines.end(),
              [&](const SketchLine& l) { return l.id == new_line_id; });
          if (right_it != params.lines.end()) {
            survivors.push_back(new_line_id);
            cutter =
                cutter_for_param(intersections,
                                 segments[clicked_index + 1].param_start);
            cuts.push_back(TrimTransferCut{
                right_it->start_x, right_it->start_y,
                right_it->start_vertex_id,
                cutter != nullptr ? *cutter : ""});
          }
        }
      }
      apply_trim_constraint_transfer(params, transfer_plan, survivors, cuts);
    }

    return;
  }
