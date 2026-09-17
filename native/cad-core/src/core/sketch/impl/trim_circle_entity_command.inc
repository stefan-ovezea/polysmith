  // Phase 2: circles → arcs.
  const auto circle_it = std::find_if(
      params.circles.begin(),
      params.circles.end(),
      [&](const SketchCircle& c) { return c.id == entity_id; });

  if (circle_it != params.circles.end()) {
    const auto intersections = find_all_intersections(*circle_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      trim_remove_entity_attachments(
          params, entity_id, {circle_it->center_vertex_id});
      params.circles.erase(circle_it);
      return;
    }

    const auto segments = split_circle_at_intersections(*circle_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = resolve_clicked_index(
        segments, select_clicked_segment(segments, *circle_it, click_x, click_y));

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Delete the clicked arc segment — keep the complementary arc.
    // For 2 segments this is the opposite; for 3+ we build the arc
    // that spans all non-clicked segments going CCW.
    const int N = static_cast<int>(segments.size());
    std::vector<std::string> arc_ids;
    arc_ids.reserve(params.arcs.size());
    for (const auto& arc : params.arcs) arc_ids.push_back(arc.id);
    const int arc_index = next_trim_entity_index(arc_ids);
    const double cx = circle_it->center_x, cy = circle_it->center_y;
    const double r = circle_it->radius;
    auto pt_at_angle = [&](double a) -> std::pair<double, double> {
      return {cx + r * std::cos(a), cy + r * std::sin(a)};
    };

    // The circle being converted must not resolve against its own
    // centre or quadrant vertices — exclude it from the shared-point
    // scan (the old code passed "" as the ignored id and could match
    // the circle itself).
    const std::unordered_set<std::string> excluded_entities{entity_id};
    auto arc_endpoint_id = [&](double px, double py) -> std::string {
      const auto shared = resolve_shared_point(params, px, py, excluded_entities);
      return shared.has_value() ? std::get<0>(shared.value())
                                : mint_vertex_id();
    };

    if (N == 2) {
      const auto& kept = segments[1 - clicked_index];
      auto [sx, sy] = pt_at_angle(kept.param_start);
      auto [ex, ey] = pt_at_angle(kept.param_end < 2.0 * M_PI
                                      ? kept.param_end
                                      : kept.param_end - 2.0 * M_PI);
      // Reuse existing endpoint ids when arc endpoints land on a point
      // another entity already owns — mirrors add_sketch_arc so the
      // profile loop detector can chain arcs with lines.
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(arc_index),
          .start_vertex_id = arc_endpoint_id(sx, sy),
          .end_vertex_id   = arc_endpoint_id(ex, ey),
          // The arc keeps the circle's center — share its center vertex
          // id instead of minting a second one at the same spot.
          .center_vertex_id = circle_it->center_vertex_id,
          .center_x = cx, .center_y = cy, .radius = r,
          .start_x = sx, .start_y = sy,
          .end_x = ex, .end_y = ey,
          .ccw = true,
          .is_construction = circle_it->is_construction,
      });
    } else {
      // Complementary arc: from segment after clicked to clicked's start,
      // going CCW the long way around the circle.
      const int next = (clicked_index + 1) % N;
      const double a_start = segments[next].param_start;
      double a_end = segments[clicked_index].param_start;
      if (a_end <= a_start) a_end += 2.0 * M_PI;
      auto [sx, sy] = pt_at_angle(a_start);
      auto [ex, ey] = pt_at_angle(segments[clicked_index].param_start);
      // Reuse existing endpoint ids for profile loop connectivity.
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(arc_index),
          .start_vertex_id = arc_endpoint_id(sx, sy),
          .end_vertex_id   = arc_endpoint_id(ex, ey),
          // The arc keeps the circle's center — share its center vertex
          // id instead of minting a second one at the same spot.
          .center_vertex_id = circle_it->center_vertex_id,
          .center_x = cx, .center_y = cy, .radius = r,
          .start_x = sx, .start_y = sy,
          .end_x = ex, .end_y = ey,
          .ccw = true,
          .is_construction = circle_it->is_construction,
      });
    }

    // Delete the circle (capture the center id first — the helper
    // below needs it after the erase).
    const std::string circle_center_id = circle_it->center_vertex_id;

    // D1/D4 capture BEFORE the cleanup erases the whitelisted
    // attachments (concentric pairs, radius dims, coincident).
    const auto transfer_plan =
        capture_trim_constraint_transfer(params, entity_id, {circle_center_id});

    params.circles.erase(circle_it);

    #include "core/sketch/impl/trim_circle_split_crossing_lines.inc"

    // The old code erased only circle-related constraints here — line
    // relations (tangent_line_circle), anchors and fillets leaked. The
    // shared helper removes every attachment of the circle.
    trim_remove_entity_attachments(params, entity_id, {circle_center_id});

    // Safety net: delete coincident constraints whose merged point
    // died or whose listed lines no longer reference it.
    sweep_orphan_coincident_constraints(params);

    // D4: the surviving arc keeps the circle's concentric relations
    // and re-derives radius dimensions as driven. Its endpoints sit
    // on the cutting curves — point-on-object anchors for line
    // cutters.
    {
      const std::string arc_id = "arc-" + std::to_string(arc_index);
      const auto arc_it2 = std::find_if(
          params.arcs.begin(), params.arcs.end(),
          [&](const SketchArc& a) { return a.id == arc_id; });
      if (arc_it2 != params.arcs.end()) {
        std::vector<TrimTransferCut> cuts;
        const double sa = std::atan2(arc_it2->start_y - cy,
                                     arc_it2->start_x - cx);
        const double ea = std::atan2(arc_it2->end_y - cy,
                                     arc_it2->end_x - cx);
        const std::string* cutter_start = cutter_for_param(intersections, sa);
        const std::string* cutter_end = cutter_for_param(intersections, ea);
        cuts.push_back(TrimTransferCut{
            arc_it2->start_x, arc_it2->start_y, arc_it2->start_vertex_id,
            cutter_start != nullptr ? *cutter_start : ""});
        cuts.push_back(TrimTransferCut{
            arc_it2->end_x, arc_it2->end_y, arc_it2->end_vertex_id,
            cutter_end != nullptr ? *cutter_end : ""});
        apply_trim_constraint_transfer(params, transfer_plan, {arc_id}, cuts);
      }
    }
    return;
  }
