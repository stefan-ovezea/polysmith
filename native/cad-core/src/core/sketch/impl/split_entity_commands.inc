// ================================================================
// Split (R4) — divides the clicked entity at ALL its intersections
// without deleting anything.
//
// Open kinds (line / arc / partial ellipse / spline): split at every
// intersection with other sketch entities; every resulting piece is
// kept. Closed kinds (circle / full ellipse) follow the SolidWorks
// rule: TWO user click points divide the entity into two pieces.
//
// Constraint policy (D1): the same transfer pipeline as trim — the
// whitelist re-anchors onto every surviving piece and dimensions
// re-derive as driven. equal_length is deliberately NOT transferred
// (N+1 collinear pieces equal to one reference overconstrains the
// sketch); parallel/perpendicular/H-V stay valid on every piece.
//
// Refresh is NOT called here — the caller (document layer) handles
// that; minted_vertex_ids collects the fresh split point ids for the
// freeze pass.
// ================================================================

void split_sketch_entity(FeatureEntry& feature,
                         const std::string& entity_id,
                         double click_x,
                         double click_y,
                         double split2_x,
                         double split2_y,
                         std::vector<std::string>* minted_vertex_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can split entities");
  }

  auto& params = *feature.sketch_parameters;

  // Every vertex id the split creates goes through this helper so the
  // document layer can collect them and freeze the fresh split points
  // against the planegcs pass (the same contract as trim).
  auto mint_vertex_id = [&]() -> std::string {
    const std::string id =
        "vertex-" + std::to_string(params.next_vertex_index++);
    if (minted_vertex_ids != nullptr) minted_vertex_ids->push_back(id);
    return id;
  };

  ensure_user_editable_entity(params, entity_id);

  const std::unordered_set<std::string> excluded{entity_id};
  auto split_shared_pt = [&](double x, double y) {
    const auto shared = resolve_shared_point(params, x, y, excluded);
    return shared.has_value() ? std::get<0>(shared.value())
                              : mint_vertex_id();
  };
  auto wrap_angle = [](double a) {
    const double k2Pi = 2.0 * M_PI;
    while (a < 0.0) a += k2Pi;
    while (a >= k2Pi) a -= k2Pi;
    return a;
  };

  // ---- Line ------------------------------------------------------
  auto line_it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const SketchLine& l) { return l.id == entity_id; });

  if (line_it != params.lines.end()) {
    const auto intersections = find_all_intersections(*line_it, params);
    if (intersections.empty()) {
      throw std::runtime_error(
          "Sketch split: the line has no intersections to split at");
    }
    const auto segments = split_line_at_intersections(*line_it, intersections);
    if (segments.size() <= 1) {
      throw std::runtime_error(
          "Sketch split: every intersection sits on the line's endpoints");
    }

    const std::string old_sp = line_it->start_vertex_id;
    const std::string old_ep = line_it->end_vertex_id;

    // D1 capture BEFORE the cleanup erases the whitelisted attachments.
    auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id, {old_sp, old_ep});
    // equal_length on N+1 pieces would force every piece (and the
    // reference) to one length while their sum is the original — a
    // guaranteed solver contradiction. Drop it; the other whitelisted
    // relations survive (parallel/perpendicular hold for collinear
    // pieces).
    transfer_plan.line_relations.erase(
        std::remove_if(transfer_plan.line_relations.begin(),
                       transfer_plan.line_relations.end(),
                       [](const auto& rel) {
                         return rel.first == "equal_length";
                       }),
        transfer_plan.line_relations.end());

    const int last = static_cast<int>(segments.size()) - 1;

    // Resolve every interior boundary id FIRST (no piece exists yet,
    // so no self-matching is possible), then commit.
    std::vector<std::string> boundary_ids(last);  // boundary k = end of piece k-1
    for (int k = 1; k <= last; ++k) {
      boundary_ids[k - 1] =
          split_shared_pt(segments[k].start_x, segments[k].start_y);
    }

    // Piece 0 keeps the original id and start.
    line_it->end_x = segments[0].end_x;
    line_it->end_y = segments[0].end_y;
    line_it->end_vertex_id = boundary_ids[0];

    std::vector<std::string> survivor_ids{entity_id};
    std::vector<TrimTransferCut> cuts;
    std::vector<std::string> line_ids;
    line_ids.reserve(params.lines.size() + segments.size());
    for (const auto& l : params.lines) line_ids.push_back(l.id);

    for (int k = 1; k <= last; ++k) {
      const auto& seg = segments[k];
      const std::string start_id = boundary_ids[k - 1];
      const std::string end_id =
          k == last ? old_ep : boundary_ids[k];
      const int next_index = next_trim_entity_index(line_ids);
      const std::string new_id = "line-" + std::to_string(next_index);
      line_ids.push_back(new_id);
      params.lines.push_back(SketchLine{
          .id = new_id,
          .start_vertex_id = start_id,
          .end_vertex_id   = end_id,
          .start_x = seg.start_x,
          .start_y = seg.start_y,
          .end_x = seg.end_x,
          .end_y = seg.end_y,
          .constraint = std::nullopt,  // H/V re-applied by the transfer
          .is_construction = line_it->is_construction,
      });
      survivor_ids.push_back(new_id);
      const std::string* cutter =
          cutter_for_param(intersections, seg.param_start);
      cuts.push_back(TrimTransferCut{
          seg.start_x, seg.start_y, start_id,
          cutter != nullptr ? *cutter : ""});
    }

    trim_remove_entity_attachments(params, entity_id, {old_sp, old_ep});

    // Orphan coincident sweep (same safety net as trim).
    {
      sweep_orphan_coincident_constraints(params);
    }

    apply_trim_constraint_transfer(params, transfer_plan, survivor_ids, cuts);
    return;
  }

  // ---- Circle (two points -> two CCW arcs) ------------------------
  const auto circle_it = std::find_if(
      params.circles.begin(), params.circles.end(),
      [&](const SketchCircle& c) { return c.id == entity_id; });

  if (circle_it != params.circles.end()) {
    const double cx = circle_it->center_x, cy = circle_it->center_y;
    const double r = circle_it->radius;
    double a1 = wrap_angle(std::atan2(click_y - cy, click_x - cx));
    double a2 = wrap_angle(std::atan2(split2_y - cy, split2_x - cx));
    double gap = std::abs(a1 - a2);
    gap = std::min(gap, 2.0 * M_PI - gap);
    if (gap < 1e-6) {
      throw std::runtime_error(
          "Sketch split: the two split points are too close together");
    }
    if (a2 <= a1) a2 += 2.0 * M_PI;  // arc 1 sweeps CCW from a1 to a2
    auto pt_at_angle = [&](double a) -> std::pair<double, double> {
      return {cx + r * std::cos(a), cy + r * std::sin(a)};
    };
    auto [p1x, p1y] = pt_at_angle(a1);
    auto [p2x, p2y] = pt_at_angle(a2);

    std::vector<std::string> arc_ids;
    arc_ids.reserve(params.arcs.size());
    for (const auto& arc : params.arcs) arc_ids.push_back(arc.id);
    const int arc_index = next_trim_entity_index(arc_ids);
    const std::string arc1_id = "arc-" + std::to_string(arc_index);
    const std::string arc2_id = "arc-" + std::to_string(arc_index + 1);

    // Resolve each split point ONCE — the two arcs share both
    // endpoint vertices so the wire walk welds them together.
    const std::string p1_id = split_shared_pt(p1x, p1y);
    const std::string p2_id = split_shared_pt(p2x, p2y);

    // D1/D4 capture BEFORE the cleanup erases the whitelisted
    // attachments (concentric pairs, radius dims, coincident).
    const std::string circle_center_id = circle_it->center_vertex_id;
    const auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id, {circle_center_id});

    params.arcs.push_back(SketchArc{
        .id = arc1_id,
        .start_vertex_id = p1_id,
        .end_vertex_id   = p2_id,
        .center_vertex_id = circle_center_id,
        .center_x = cx, .center_y = cy, .radius = r,
        .start_x = p1x, .start_y = p1y,
        .end_x = p2x, .end_y = p2y,
        .ccw = true,
        .is_construction = circle_it->is_construction,
    });
    params.arcs.push_back(SketchArc{
        .id = arc2_id,
        .start_vertex_id = p2_id,
        .end_vertex_id   = p1_id,
        .center_vertex_id = circle_center_id,
        .center_x = cx, .center_y = cy, .radius = r,
        .start_x = p2x, .start_y = p2y,
        .end_x = p1x, .end_y = p1y,
        .ccw = true,
        .is_construction = circle_it->is_construction,
    });

    params.circles.erase(circle_it);

    trim_remove_entity_attachments(params, entity_id, {circle_center_id});

    // Orphan coincident sweep.
    {
      sweep_orphan_coincident_constraints(params);
    }

    // D4 transfer: both arcs keep the circle's concentric relations
    // and re-derive radius dimensions as driven. The two split points
    // are user-picked (no cutter), so no point-on-object anchors.
    apply_trim_constraint_transfer(params, transfer_plan,
                                   {arc1_id, arc2_id}, {});
    return;
  }

  // ---- Arc --------------------------------------------------------
  const auto arc_it = std::find_if(
      params.arcs.begin(), params.arcs.end(),
      [&](const SketchArc& a) { return a.id == entity_id; });

  if (arc_it != params.arcs.end()) {
    const auto intersections = find_all_intersections(*arc_it, params);
    if (intersections.empty()) {
      throw std::runtime_error(
          "Sketch split: the arc has no intersections to split at");
    }
    const auto segments = split_arc_at_intersections(*arc_it, intersections);
    if (segments.size() <= 1) {
      throw std::runtime_error(
          "Sketch split: every intersection sits on the arc's endpoints");
    }

    const std::string old_sp = arc_it->start_vertex_id;
    const std::string old_ep = arc_it->end_vertex_id;
    const std::string old_center = arc_it->center_vertex_id;

    auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id, {old_sp, old_ep, old_center});

    const int last = static_cast<int>(segments.size()) - 1;

    std::vector<std::string> boundary_ids(last);
    for (int k = 1; k <= last; ++k) {
      boundary_ids[k - 1] =
          split_shared_pt(segments[k].start_x, segments[k].start_y);
    }

    // Piece 0 keeps the original id and start.
    arc_it->end_x = segments[0].end_x;
    arc_it->end_y = segments[0].end_y;
    arc_it->end_vertex_id = boundary_ids[0];

    std::vector<std::string> survivor_ids{entity_id};
    std::vector<TrimTransferCut> cuts;
    std::vector<std::string> arc_ids;
    arc_ids.reserve(params.arcs.size() + segments.size());
    for (const auto& a : params.arcs) arc_ids.push_back(a.id);

    for (int k = 1; k <= last; ++k) {
      const auto& seg = segments[k];
      const std::string start_id = boundary_ids[k - 1];
      const std::string end_id =
          k == last ? old_ep : boundary_ids[k];
      const int next_index = next_trim_entity_index(arc_ids);
      const std::string new_id = "arc-" + std::to_string(next_index);
      arc_ids.push_back(new_id);
      params.arcs.push_back(SketchArc{
          .id = new_id,
          .start_vertex_id = start_id,
          .end_vertex_id   = end_id,
          // All pieces keep the source arc's center — share its
          // center vertex id instead of minting duplicates.
          .center_vertex_id = arc_it->center_vertex_id,
          .center_x = arc_it->center_x,
          .center_y = arc_it->center_y,
          .radius   = arc_it->radius,
          .start_x  = seg.start_x,
          .start_y  = seg.start_y,
          .end_x    = seg.end_x,
          .end_y    = seg.end_y,
          .ccw      = arc_it->ccw,
          .is_construction = arc_it->is_construction,
      });
      survivor_ids.push_back(new_id);
      const std::string* cutter =
          cutter_for_param(intersections, seg.param_start);
      cuts.push_back(TrimTransferCut{
          seg.start_x, seg.start_y, start_id,
          cutter != nullptr ? *cutter : ""});
    }

    trim_remove_entity_attachments(
        params, entity_id, {old_sp, old_ep, old_center});

    {
      sweep_orphan_coincident_constraints(params);
    }

    apply_trim_constraint_transfer(params, transfer_plan, survivor_ids, cuts);
    return;
  }

  // ---- Ellipse ----------------------------------------------------
  const auto ellipse_it = std::find_if(
      params.ellipses.begin(), params.ellipses.end(),
      [&](const SketchEllipse& e) { return e.id == entity_id; });

  if (ellipse_it != params.ellipses.end()) {
    const std::string old_sp = ellipse_it->start_vertex_id;
    const std::string old_ep = ellipse_it->end_vertex_id;
    const std::string old_center = ellipse_it->center_vertex_id;
    const std::string old_axis_a = ellipse_it->axis_a_vertex_id;
    const std::string old_axis_b = ellipse_it->axis_b_vertex_id;

    auto transfer_plan = capture_trim_constraint_transfer(
        params, entity_id,
        {old_sp, old_ep, old_center, old_axis_a, old_axis_b});

    if (!ellipse_it->has_sweep) {
      // Full ellipse -> two partial elliptical pieces (two clicks).
      const double cu = std::cos(ellipse_it->rotation);
      const double su = std::sin(ellipse_it->rotation);
      // Convert each click into the ellipse-frame parametric angle
      // (the inverse of transfer_ellipse_point).
      auto click_param = [&](double px, double py) -> double {
        const double dx = px - ellipse_it->center_x;
        const double dy = py - ellipse_it->center_y;
        const double xf = dx * cu + dy * su;
        const double yf = -dx * su + dy * cu;
        return std::atan2(yf / ellipse_it->b, xf / ellipse_it->a);
      };
      double a1 = click_param(click_x, click_y);
      double a2 = click_param(split2_x, split2_y);
      double gap = std::fmod(std::abs(a1 - a2), 2.0 * M_PI);
      gap = std::min(gap, 2.0 * M_PI - gap);
      if (gap < 1e-6) {
        throw std::runtime_error(
            "Sketch split: the two split points are too close together");
      }
      if (a2 <= a1) a2 += 2.0 * M_PI;  // piece 1 sweeps CCW from a1 to a2
      const auto [p1x, p1y] = transfer_ellipse_point(*ellipse_it, a1);
      const auto [p2x, p2y] = transfer_ellipse_point(*ellipse_it, a2);

      std::vector<std::string> ellipse_ids;
      ellipse_ids.reserve(params.ellipses.size() + 1);
      for (const auto& e : params.ellipses) ellipse_ids.push_back(e.id);
      const int next_idx = next_trim_entity_index(ellipse_ids);
      const std::string piece2_id = "ellipse-" + std::to_string(next_idx);

      const std::string p1_id = split_shared_pt(p1x, p1y);
      const std::string p2_id = split_shared_pt(p2x, p2y);

      // Piece 1 keeps the original id (circle-style in-place
      // conversion, like the trim full-ellipse branch).
      ellipse_it->has_sweep = true;
      ellipse_it->ccw = true;
      ellipse_it->sweep_start_angle = wrap_angle(a1);
      ellipse_it->sweep_end_angle = wrap_angle(a2);
      ellipse_it->start_vertex_id = p1_id;
      ellipse_it->end_vertex_id = p2_id;

      params.ellipses.push_back(SketchEllipse{
          .id = piece2_id,
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
          .sweep_start_angle = wrap_angle(a2),
          .sweep_end_angle = wrap_angle(a1 + 2.0 * M_PI),
          .ccw = true,
          .start_vertex_id = p2_id,
          .end_vertex_id = p1_id,
      });

      trim_remove_entity_attachments(
          params, entity_id,
          {old_sp, old_ep, old_center, old_axis_a, old_axis_b});

      {
        sweep_orphan_coincident_constraints(params);
      }

      // No cutters: the split points are user-picked.
      apply_trim_constraint_transfer(
          params, transfer_plan, {entity_id, piece2_id}, {});
      return;
    }

    // Partial ellipse -> split at every intersection.
    const auto intersections = find_all_intersections(*ellipse_it, params);
    if (intersections.empty()) {
      throw std::runtime_error(
          "Sketch split: the ellipse has no intersections to split at");
    }
    const auto segments =
        split_ellipse_at_intersections(*ellipse_it, intersections);
    if (segments.size() <= 1) {
      throw std::runtime_error(
          "Sketch split: every intersection sits on the ellipse's endpoints");
    }

    const int last = static_cast<int>(segments.size()) - 1;

    std::vector<std::string> boundary_ids(last);
    for (int k = 1; k <= last; ++k) {
      boundary_ids[k - 1] =
          split_shared_pt(segments[k].start_x, segments[k].start_y);
    }

    // Piece 0 keeps the original id and start.
    ellipse_it->sweep_end_angle = wrap_angle(segments[0].param_end);
    ellipse_it->end_vertex_id = boundary_ids[0];

    std::vector<std::string> survivor_ids{entity_id};
    std::vector<TrimTransferCut> cuts;
    std::vector<std::string> ellipse_ids;
    ellipse_ids.reserve(params.ellipses.size() + segments.size());
    for (const auto& e : params.ellipses) ellipse_ids.push_back(e.id);

    for (int k = 1; k <= last; ++k) {
      const auto& seg = segments[k];
      const std::string start_id = boundary_ids[k - 1];
      const std::string end_id =
          k == last ? old_ep : boundary_ids[k];
      const int next_index = next_trim_entity_index(ellipse_ids);
      const std::string new_id = "ellipse-" + std::to_string(next_index);
      ellipse_ids.push_back(new_id);
      params.ellipses.push_back(SketchEllipse{
          .id = new_id,
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
          .sweep_start_angle = wrap_angle(seg.param_start),
          .sweep_end_angle = wrap_angle(seg.param_end),
          .ccw = ellipse_it->ccw,
          .start_vertex_id = start_id,
          .end_vertex_id = end_id,
      });
      survivor_ids.push_back(new_id);
      const std::string* cutter =
          cutter_for_param(intersections, seg.param_start);
      const auto [px, py] =
          transfer_ellipse_point(*ellipse_it, seg.param_start);
      cuts.push_back(TrimTransferCut{
          px, py, start_id, cutter != nullptr ? *cutter : ""});
    }

    trim_remove_entity_attachments(
        params, entity_id,
        {old_sp, old_ep, old_center, old_axis_a, old_axis_b});

    {
      sweep_orphan_coincident_constraints(params);
    }

    apply_trim_constraint_transfer(params, transfer_plan, survivor_ids, cuts);
    return;
  }

  // ---- Spline -----------------------------------------------------
  const auto spline_it = std::find_if(
      params.splines.begin(), params.splines.end(),
      [&](const SketchSpline& s) { return s.id == entity_id; });

  if (spline_it != params.splines.end()) {
    const auto intersections = find_all_intersections(*spline_it, params);
    if (intersections.empty()) {
      throw std::runtime_error(
          "Sketch split: the spline has no intersections to split at");
    }
    const auto segments =
        split_spline_at_intersections(*spline_it, intersections);
    if (segments.size() <= 1) {
      throw std::runtime_error(
          "Sketch split: every intersection sits on the spline's endpoints");
    }

    const std::vector<double> orig_px = spline_it->pole_xs;
    const std::vector<double> orig_py = spline_it->pole_ys;
    const int orig_degree = spline_it->degree;
    const std::vector<std::string> old_pole_ids = spline_it->pole_vertex_ids;

    // D1 capture BEFORE the cleanup erases the coincident constraints
    // on the spline's poles.
    const auto transfer_plan =
        capture_trim_constraint_transfer(params, entity_id, old_pole_ids);

    // Re-fit every span into temporaries FIRST and commit only when
    // every span succeeds (the trim spline path's undo-safety rule).
    const int last = static_cast<int>(segments.size()) - 1;
    std::vector<std::string> spline_ids;
    spline_ids.reserve(params.splines.size() + last);
    for (const auto& s : params.splines) spline_ids.push_back(s.id);
    const int base_index = next_trim_entity_index(spline_ids);

    std::vector<SketchSpline> pieces(last + 1);
    bool ok = true;
    for (int k = 0; k <= last && ok; ++k) {
      auto& out = pieces[k];
      std::vector<double> px, py;
      int deg = 3;
      if (!spline_segment_poles(orig_px, orig_py, orig_degree,
                                segments[k].param_start,
                                segments[k].param_end, px, py, deg)) {
        ok = false;
        break;
      }
      out.degree = deg;
      out.pole_xs = std::move(px);
      out.pole_ys = std::move(py);
      out.pole_vertex_ids.clear();
      for (size_t i = 0; i < out.pole_xs.size(); ++i) {
        if (i == 0 || i + 1 == out.pole_xs.size()) {
          out.pole_vertex_ids.push_back(
              split_shared_pt(out.pole_xs[i], out.pole_ys[i]));
        } else {
          out.pole_vertex_ids.push_back(mint_vertex_id());
        }
      }
      out.id = k == 0 ? spline_it->id
                      : "spline-" + std::to_string(base_index + k - 1);
      out.is_construction = spline_it->is_construction;
      out.generated_by = spline_it->generated_by;
    }
    if (!ok) {
      throw std::runtime_error("Sketch split: spline segment re-fit failed");
    }

    // Adjacent pieces share their boundary point — force a single
    // vertex id on both sides so the wire walk welds the pieces.
    // (split_shared_pt above resolved each side separately and could
    // not see the sibling's minted id.)
    for (int k = 1; k <= last; ++k) {
      const std::string b_id =
          split_shared_pt(pieces[k].pole_xs.front(),
                          pieces[k].pole_ys.front());
      pieces[k - 1].pole_vertex_ids.back() = b_id;
      pieces[k].pole_vertex_ids.front() = b_id;
    }

    *spline_it = std::move(pieces[0]);
    for (int k = 1; k <= last; ++k) {
      params.splines.push_back(std::move(pieces[k]));
    }

    trim_remove_entity_attachments(params, entity_id, old_pole_ids);

    {
      sweep_orphan_coincident_constraints(params);
    }

    // Splines carry no H/V, relations or dimensions — coincident
    // survival only, no cut anchors.
    std::vector<std::string> survivor_ids;
    survivor_ids.reserve(last + 1);
    survivor_ids.push_back(entity_id);
    for (int k = 1; k <= last; ++k) survivor_ids.push_back(pieces[k].id);
    apply_trim_constraint_transfer(params, transfer_plan, survivor_ids, {});
    return;
  }

  throw std::runtime_error("Sketch entity not found: " + entity_id);
}
