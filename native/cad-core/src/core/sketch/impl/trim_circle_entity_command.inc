  // Phase 2: circles → arcs.
  const auto circle_it = std::find_if(
      params.circles.begin(),
      params.circles.end(),
      [&](const SketchCircle& c) { return c.id == entity_id; });

  if (circle_it != params.circles.end()) {
    const auto intersections = find_all_intersections(*circle_it, params);

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
      params.circles.erase(circle_it);
      return;
    }

    const auto segments = split_circle_at_intersections(*circle_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = select_clicked_segment(
        segments, *circle_it, click_x, click_y);

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Delete the clicked arc segment — keep the complementary arc.
    // For 2 segments this is the opposite; for 3+ we build the arc
    // that spans all non-clicked segments going CCW.
    const int N = static_cast<int>(segments.size());
    const int arc_index = params.arcs.size() + 1;
    const int start_pt_idx = arc_index * 2;
    const int end_pt_idx = arc_index * 2 + 1;
    const double cx = circle_it->center_x, cy = circle_it->center_y;
    const double r = circle_it->radius;
    auto pt_at_angle = [&](double a) -> std::pair<double, double> {
      return {cx + r * std::cos(a), cy + r * std::sin(a)};
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
      const auto shared_start = find_coincident_endpoint(params, "", sx, sy);
      const auto shared_end   = find_coincident_endpoint(params, "", ex, ey);
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(arc_index),
          .start_point_id = shared_start.has_value()
              ? std::get<0>(shared_start.value())
              : "point-trim-" + std::to_string(start_pt_idx) + "-start",
          .end_point_id   = shared_end.has_value()
              ? std::get<0>(shared_end.value())
              : "point-trim-" + std::to_string(end_pt_idx) + "-end",
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
      const auto shared_start = find_coincident_endpoint(params, "", sx, sy);
      const auto shared_end   = find_coincident_endpoint(params, "", ex, ey);
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(arc_index),
          .start_point_id = shared_start.has_value()
              ? std::get<0>(shared_start.value())
              : "point-trim-" + std::to_string(start_pt_idx) + "-start",
          .end_point_id   = shared_end.has_value()
              ? std::get<0>(shared_end.value())
              : "point-trim-" + std::to_string(end_pt_idx) + "-end",
          .center_x = cx, .center_y = cy, .radius = r,
          .start_x = sx, .start_y = sy,
          .end_x = ex, .end_y = ey,
          .ccw = true,
          .is_construction = circle_it->is_construction,
      });
    }

    // Delete the circle.
    params.circles.erase(circle_it);

    // Mark all constraints referencing the circle as dependency_broken.
    // (The constraint and dimension structs don't have a dependency_broken
    // flag yet, so for v1 we just delete circle-related constraints.)
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
                         return d.entity_id == entity_id ||
                                d.secondary_entity_id == entity_id;
                       }),
        params.dimensions.end());

    // Safety net: delete coincident constraints referencing orphaned point IDs.
    {
      std::unordered_set<std::string> live_pt;
      for (const auto& l : params.lines) {
        live_pt.insert(l.start_point_id); live_pt.insert(l.end_point_id);
      }
      for (const auto& c : params.circles) {
        live_pt.insert("point-circle-" + c.id + "-center");
        live_pt.insert("point-circle-" + c.id + "-quadrant-0");
        live_pt.insert("point-circle-" + c.id + "-quadrant-1");
        live_pt.insert("point-circle-" + c.id + "-quadrant-2");
        live_pt.insert("point-circle-" + c.id + "-quadrant-3");
      }
      for (const auto& a : params.arcs) {
        live_pt.insert(a.start_point_id); live_pt.insert(a.end_point_id);
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
