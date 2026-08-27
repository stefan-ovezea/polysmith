  // Phase 1: lines.
  const auto line_it = std::find_if(
      params.lines.begin(),
      params.lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });

  if (line_it != params.lines.end()) {
    const auto intersections = find_all_intersections(*line_it, params);

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
                           return d.entity_id == entity_id ||
                                  d.secondary_entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.lines.erase(line_it);
      return;
    }

    // Capture old point IDs *before* vector mutations (push_back
    // may invalidate the line iterator, making later reads garbage).
    const std::string old_sp_id = line_it->start_vertex_id;
    const std::string old_ep_id = line_it->end_vertex_id;

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

      if (points_match(new_line.start_x, new_line.start_y,
                       new_line.end_x, new_line.end_y)) {
        log_warn("trim", "trim split produced zero-length line '" +
                             new_line.id + "' — dropping");
        params.lines.pop_back();
      }
      if (points_match(line_it->start_x, line_it->start_y,
                       line_it->end_x, line_it->end_y)) {
        log_warn("trim", "trim split collapsed left portion '" +
                             line_it->id + "' — deleting");
        params.lines.erase(line_it);
        return;
      }
    }

    #include "core/sketch/impl/trim_line_relation_cleanup.inc"

    #include "core/sketch/impl/trim_line_point_rebind.inc"

    // Clear any H/V constraint the line may have had — trim may have
    // changed the direction enough to invalidate it.
    line_it->constraint = std::nullopt;

    #include "core/sketch/impl/trim_line_orphan_constraint_cleanup.inc"

    return;
  }
