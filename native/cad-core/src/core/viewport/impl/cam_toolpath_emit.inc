  // ── CAM toolpaths from operations ────────────────────────────
  // Toolpaths are memory-only: generated on demand, cached in
  // cam_runtime keyed by document revision.  The viewport draws the
  // full path when the op is "generated", falling back to the cached
  // wireframe preview otherwise.  Arcs are chord-linearized for the
  // polyline renderer: a pinned arc_segments_per_circle count when
  // set (the preview then shows the faceting the machine will
  // actually cut), else a 0.005 mm sagitta — far below a screen
  // pixel at typical zoom so true-arc exports still PREVIEW round
  // (0.05 mm left ~8° facets on a 20 mm circle).
  constexpr double kPreviewChordToleranceMm = 0.005;
  std::vector<ViewportToolpathPrimitive> toolpaths;
  if (document.has_value()) {
    for (const auto& op : document->cam.operations) {
      if (!op.enabled) {
        continue;
      }
      const Toolpath* toolpath =
          cam_runtime::cached_toolpath(*document, op.op_id);
      if (toolpath == nullptr) {
        toolpath = cam_runtime::cached_preview(*document, op.op_id);
      }
      if (toolpath == nullptr || toolpath->moves.empty()) {
        continue;
      }

      ViewportToolpathPrimitive primitive;
      primitive.id = "tp:" + op.op_id;
      primitive.label = op.name;
      // The post's first G0 rides the implicit start at machine zero
      // — no move in the toolpath carries it, so without this
      // segment the drilling preview shows only the plunge verticals
      // and the travel from the origin to the first hole is
      // invisible.  Draw it: a rapid from the resolved WCS origin
      // (machine zero in world coords) to the first hole.
      if (op.type == "drilling" &&
          toolpath->moves.front().kind == ToolpathMoveKind::Rapid) {
        const CamSetup* setup = setup_for(*document, op);
        std::array<double, 3> origin = {0.0, 0.0, 0.0};
        if (setup != nullptr) {
          if (setup->wcs_origin.position.has_value()) {
            origin = *setup->wcs_origin.position;
          } else if (setup->stock.origin.has_value()) {
            origin = *setup->stock.origin;
          }
        }
        primitive.points.push_back(
            {origin[0], origin[1], origin[2], true, false});
      }
      ToolpathMove previous = toolpath->moves.front();
      for (const auto& move : toolpath->moves) {
        if (move.kind == ToolpathMoveKind::FeedArcCW ||
            move.kind == ToolpathMoveKind::FeedArcCCW) {
          // Chord points first, endpoint last (linearize_arc_move's
          // final point is the move endpoint).
          std::vector<std::array<double, 3>> arcPoints;
          if (toolpath->arc_segments_per_circle > 0) {
            const int steps = arc_steps_for(
                previous, move, toolpath->arc_segments_per_circle);
            linearize_arc_move_steps(previous, move, steps, arcPoints);
          } else {
            linearize_arc_move(previous, move, kPreviewChordToleranceMm,
                               arcPoints);
          }
          for (const auto& point : arcPoints) {
            primitive.points.push_back(
                {point[0], point[1], point[2], false, false});
          }
        } else if (move.kind == ToolpathMoveKind::DrillCycle) {
          // One IR move covers the whole retract → bottom → retract
          // trip — synthesize the visual points (the IR's own Rapid
          // carries only the XY positioning, so the first synthesized
          // point duplicates it as a zero-length segment).
          primitive.points.push_back(
              {move.x, move.y, move.r_plane_z, true, false});
          primitive.points.push_back({move.x, move.y, move.z, false, false});
          primitive.points.push_back(
              {move.x, move.y, move.r_plane_z, true, false});
        } else {
          primitive.points.push_back(
              {move.x, move.y, move.z,
               move.kind == ToolpathMoveKind::Rapid,
               move.laser_on && move.dwell_seconds > 0.0});
        }
        previous = move;
      }
      toolpaths.push_back(std::move(primitive));
    }
  }
