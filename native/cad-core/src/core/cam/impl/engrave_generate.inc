// ── Mill Engrave generator ────────────────────────────────────────
//
// Traces sketch profile geometry ON-LINE (no offset, no leads) at a
// fixed depth below the sketch plane — the milling twin of laser
// engrave.  EVERY selected profile is traced in region order (one
// rapid-plunge-feed-rapid per loop); inner loops (holes) are traced
// too, so glyph counters ("O", "8", …) render hollow.  Standalone
// circles and circle holes emit one exact full-circle arc;
// ellipse/spline edges fall back to a chord-tolerance polyline.
//
// Direction: outer loops walk CCW (material left — climb), holes CW;
// "conventional" flips both; "mixed"/unknown warn and behave as
// climb.  A left-handed sketch frame mirrors the geometry into world
// XY, which flips every arc sweep.
//
// Shape: per loop — Rapid(start, retract) → plunge FeedLinear at the
// cut plane → trace at feedrate → Rapid back to retract.  Single
// pass — no stepdown.
//
// v1 exclusions: no fill/hatch, no lead-in/out, no depth passes,
// sketch-profile input only.

namespace {

// One planned trace loop, bound to its owning sketch's plane frame
// (mixed sketches may share one operation).
struct EngraveLoop {
  std::vector<BaseSegment> base;
  bool isHole = false;
  SketchFeatureParameters::SketchPlaneFrame frame;
  bool flippedFrame = false;
  double cutZ = 0.0;
};

CamGenerateResult generate_engrave_toolpath(
    const CamGenerateContext& context) {
  CamGenerateResult result;
  const auto& op = context.operation;

  // Tool-axis guard (5-axis scaffolding): reserved rotary modes must
  // degrade with a clear error, never silently emit 3-axis moves.
  std::string axisError;
  if (!milling::check_tool_axis_supported(op, axisError)) {
    result.ok = false;
    result.error_message = axisError;
    return result;
  }

  // An absent engrave block falls back to the struct defaults (depth
  // 0.5) so old documents still generate.
  const EngraveParameters engrave = op.parameters.engrave.has_value()
                                        ? op.parameters.engrave.value()
                                        : EngraveParameters{};
  if (engrave.depth_mm <= 0.0) {
    result.ok = false;
    result.error_message = "The engrave depth must be positive.";
    return result;
  }

  // Sketch-profile input ONLY.  Reject mixed attestations BEFORE
  // touching geometry.profiles/sketches — those two are parallel
  // arrays filled together by the driver, while faces/edges/points
  // land in other vectors.
  if (!context.geometry.faces.empty() || !context.geometry.edges.empty() ||
      !context.geometry.points.empty()) {
    result.ok = false;
    result.error_message =
        "Engrave traces sketch profiles only — a selected region is "
        "not a sketch profile.";
    return result;
  }
  if (context.geometry.profiles.empty()) {
    result.ok = false;
    result.error_message =
        "Engrave requires at least one selected sketch profile "
        "(geometry changed — re-select it).";
    return result;
  }

  // cutting_direction: "conventional" flips the walks; "mixed" and
  // unknown values warn and behave as climb.
  bool conventional = false;
  if (op.parameters.cutting_direction == "conventional") {
    conventional = true;
  } else if (op.parameters.cutting_direction == "mixed") {
    result.warnings.push_back(
        "Cutting direction 'mixed' is treated as climb for engrave.");
  } else if (op.parameters.cutting_direction != "climb") {
    result.warnings.push_back(
        "Unknown cutting direction '" + op.parameters.cutting_direction +
        "' — treated as climb.");
  }

  // ── Plan loops: every profile in region order ───────────────────
  std::vector<EngraveLoop> loops;
  double maxCutZ = -1e9;
  double minCutZ = 1e9;
  for (size_t r = 0; r < context.geometry.profiles.size(); ++r) {
    const SketchFeatureParameters* owningSketch =
        context.geometry.sketches[r];  // parallel array — driver guarantee
    if (owningSketch == nullptr) {
      result.ok = false;
      result.error_message = "The sketch plane could not be resolved.";
      return result;
    }
    const auto frame = cam_planning::resolve_sketch_frame(*owningSketch);
    if (!frame.has_value()) {
      result.ok = false;
      result.error_message = "The sketch plane could not be resolved.";
      return result;
    }
    // A tilted or vertical sketch plane cannot be engraved by a
    // 3-axis machine in v1 — reject it before generating garbage.
    if (std::abs(frame->normal_z) < cam_planning::kMaxUpwardFaceTilt) {
      result.ok = false;
      result.error_message =
          "Engrave requires a horizontal sketch plane; this sketch "
          "plane is not horizontal.";
      return result;
    }
    // Left-handed in-plane frames mirror the geometry into world XY,
    // which reverses every arc sweep.
    const bool flippedFrame =
        frame->x_axis_x * frame->y_axis_y -
            frame->x_axis_y * frame->y_axis_x <
        0.0;
    const double cutZ = cam_planning::world_z(frame.value(), XY{0.0, 0.0},
                                              -engrave.depth_mm);

    const auto& region = *context.geometry.profiles[r].region;
    const auto push_loop = [&](std::vector<BaseSegment> base,
                               bool isHole) {
      if (base.size() < 2 && !(base.size() == 1 && base[0].is_arc)) {
        result.warnings.push_back(
            isHole ? "A profile hole contour was skipped."
                   : "A profile contour was skipped.");
        return;
      }
      if (base.size() >= 2 && base_segments_signed_area(base) < 0) {
        reverse_segments(base);  // normalize: material on the left (CCW)
      }
      if (isHole) {
        reverse_segments(base);  // holes walk CW (hole interior right)
      }
      if (conventional) {
        reverse_segments(base);
      }
      EngraveLoop loop;
      loop.base = std::move(base);
      loop.isHole = isHole;
      loop.frame = frame.value();
      loop.flippedFrame = flippedFrame;
      loop.cutZ = cutZ;
      loops.push_back(std::move(loop));
      maxCutZ = std::max(maxCutZ, cutZ);
      minCutZ = std::min(minCutZ, cutZ);
    };

    // Outer loop (the laser order: outer first, then holes).
    std::vector<BaseSegment> base;
    if (!cam_planning::build_base_segments_from_edges(region, base)) {
      result.warnings.push_back(
          "Profile contour is tessellated (no exact arcs).");
      cam_planning::build_base_segments_from_points(region.points, base);
    }
    // Standalone circle regions carry exact center/radius but no
    // boundary edges — synthesize a full-circle base segment.
    if (base.empty() &&
        (region.kind == "circle" || region.source_circle_id.has_value())) {
      BaseSegment full;
      full.is_arc = true;
      full.center = {region.center_x, region.center_y};
      full.radius = region.radius;
      full.start = {region.center_x + region.radius, region.center_y};
      full.end = full.start;
      full.ccw = true;
      base.push_back(full);
    }
    push_loop(std::move(base), /*isHole=*/false);

    // Hole loops — glyph counters must render hollow.
    for (size_t holeIndex = 0; holeIndex < region.inner_loops.size();
         ++holeIndex) {
      const auto& holePoints = region.inner_loops[holeIndex];
      std::vector<BaseSegment> holeBase;
      // Circle holes carry an exact center/radius descriptor — trace
      // them as one full-circle arc (the sampled points would read as
      // a visibly polygonal path beside the smooth outline).  Stored
      // CCW like every other base; push_loop's hole flip turns the
      // walk CW (hole interior on the right).
      const auto circleHole = std::find_if(
          region.circle_holes.begin(), region.circle_holes.end(),
          [&](const auto& entry) {
            return entry.loop_index == static_cast<int>(holeIndex);
          });
      if (circleHole != region.circle_holes.end()) {
        BaseSegment full;
        full.is_arc = true;
        full.center = {circleHole->center_x, circleHole->center_y};
        full.radius = circleHole->radius;
        full.start = {full.center.x + full.radius, full.center.y};
        full.end = full.start;
        full.ccw = true;
        holeBase.push_back(full);
      } else {
        cam_planning::build_base_segments_from_points(holePoints, holeBase);
      }
      push_loop(std::move(holeBase), /*isHole=*/true);
    }
  }

  if (loops.empty()) {
    result.ok = false;
    result.error_message = "No engrave moves were produced.";
    return result;
  }

  // ── Guards ─────────────────────────────────────────────────────
  // Retract must clear the highest cut plane and the stock top; the
  // deepest cut plane must stay inside the stock (the contour
  // retract-guard contract).  The retract plane is WCS-relative
  // (origin Z + retract height).
  const double retractZ =
      cam_planning::setup_retract_plane_z(context.setup);
  if (retractZ < maxCutZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the cut plane (" << maxCutZ
            << " mm) — rapids travel through the material. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  const auto extents = cam_stock::stock_box_extents(context.document,
                                                    context.setup.stock);
  if (extents.valid) {
    const double stockTopZ = extents.center[2] + extents.size[2] / 2.0;
    const double stockBottomZ = extents.center[2] - extents.size[2] / 2.0;
    if (retractZ < stockTopZ) {
      std::ostringstream message;
      message << "Retract height (" << context.setup.retract_height
              << " mm above the setup origin) reaches " << retractZ
              << " mm — below the stock top (" << stockTopZ
              << " mm) — rapids travel through unmilled stock. Raise "
                 "it in the Setup.";
      result.warnings.push_back(message.str());
    }
    if (minCutZ < stockBottomZ - 1e-6) {
      std::ostringstream message;
      message << "The cut plane (" << minCutZ
              << " mm) is below the stock bottom (" << stockBottomZ
              << " mm) — the tool cuts through the stock. Check the "
                 "depth.";
      result.warnings.push_back(message.str());
    }
  }

  // ── Emit moves ─────────────────────────────────────────────────
  const double feedrate = op.parameters.feedrate_mm_per_min > 0.0
                              ? op.parameters.feedrate_mm_per_min
                              : context.tool.default_feedrate_mm_per_min;
  const double plungeFeed =
      op.parameters.plunge_feedrate_mm_per_min > 0.0
          ? op.parameters.plunge_feedrate_mm_per_min
          : context.tool.default_plunge_feedrate_mm_per_min;

  Toolpath toolpath;
  toolpath.op_id = op.op_id;

  // Emitters bound to ONE loop's frame — mixed-sketch ops map each
  // loop through its own sketch plane.
  const auto emit_loop = [&](const EngraveLoop& loop) {
    const auto toWorld = [&](const XY& p) {
      return cam_planning::world_point(loop.frame, p);
    };
    const auto toZ = [&](const XY& p) {
      return cam_planning::world_z(loop.frame, p, -engrave.depth_mm);
    };
    const auto append_rapid_xy = [&](const XY& p, double z) {
      const XY w = toWorld(p);
      ToolpathMove move;
      move.kind = ToolpathMoveKind::Rapid;
      move.x = w.x;
      move.y = w.y;
      move.z = z;
      move.laser_on = false;
      toolpath.moves.push_back(move);
    };
    const auto append_feed_xy = [&](const XY& p, double z, double feed) {
      const XY w = toWorld(p);
      ToolpathMove move;
      move.kind = ToolpathMoveKind::FeedLinear;
      move.x = w.x;
      move.y = w.y;
      move.z = z;
      move.feedrate_mm_per_min = feed;
      move.laser_on = false;
      toolpath.moves.push_back(move);
    };
    // G2/G3 live in machine XY: map center and start to world and
    // emit the world-space center offset.  A left-handed sketch frame
    // mirrors the geometry and reverses the arc sweep.
    const auto append_arc = [&](bool cw, const XY& start, const XY& end,
                                const XY& center, double z, double feed) {
      const XY wStart = toWorld(start);
      const XY wEnd = toWorld(end);
      const XY wCenter = toWorld(center);
      ToolpathMove move;
      move.kind = (cw != loop.flippedFrame) ? ToolpathMoveKind::FeedArcCW
                                            : ToolpathMoveKind::FeedArcCCW;
      move.x = wEnd.x;
      move.y = wEnd.y;
      move.z = z;
      move.i = wCenter.x - wStart.x;
      move.j = wCenter.y - wStart.y;
      move.feedrate_mm_per_min = feed;
      move.laser_on = false;
      toolpath.moves.push_back(move);
    };

    // Rapid in → plunge → trace → retract (single pass).
    append_rapid_xy(loop.base.front().start, retractZ);
    append_feed_xy(loop.base.front().start, toZ(loop.base.front().start),
                   plungeFeed);
    for (const auto& segment : loop.base) {
      if (segment.is_arc) {
        append_arc(!segment.ccw, segment.start, segment.end,
                   segment.center, toZ(segment.end), feedrate);
      } else {
        append_feed_xy(segment.end, toZ(segment.end), feedrate);
      }
    }
    append_rapid_xy(loop.base.back().end, retractZ);
  };
  for (const auto& loop : loops) {
    emit_loop(loop);
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No engrave moves were produced.";
    return result;
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace
