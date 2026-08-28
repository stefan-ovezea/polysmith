  // ── CAM toolpaths from operations ────────────────────────────
  // Toolpaths are memory-only: generated on demand, cached in
  // cam_runtime keyed by document revision.  The viewport draws the
  // full path when the op is "generated", falling back to the cached
  // wireframe preview otherwise.  Arcs are chord-linearized for the
  // polyline renderer (0.05 mm sagitta).
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
      ToolpathMove previous = toolpath->moves.front();
      for (const auto& move : toolpath->moves) {
        if (move.kind == ToolpathMoveKind::FeedArcCW ||
            move.kind == ToolpathMoveKind::FeedArcCCW) {
          // Chord points first, endpoint last (linearize_arc_move's
          // final point is the move endpoint).
          std::vector<std::array<double, 3>> arcPoints;
          linearize_arc_move(previous, move, /*chord_tolerance_mm=*/0.05,
                             arcPoints);
          for (const auto& point : arcPoints) {
            primitive.points.push_back({point[0], point[1], point[2], false});
          }
        } else {
          primitive.points.push_back(
              {move.x, move.y, move.z,
               move.kind == ToolpathMoveKind::Rapid});
        }
        previous = move;
      }
      toolpaths.push_back(std::move(primitive));
    }
  }
