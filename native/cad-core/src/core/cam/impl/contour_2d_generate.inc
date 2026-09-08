// ── 2D Contour generator ───────────────────────────────────────────
//
// Follows one closed wire at a fixed depth, offset by the tool radius
// (+ radial stock allowance) on the inside, outside, or on the line
// itself.  The input is a picked planar face (its largest |signed
// area| wire — inner holes are NOT contoured in v1) or, failing that,
// the first selected sketch profile (laser-cut-style capture).  The
// face wins when both are present.
//
// Direction rule (the pocket precedent — climb keeps the material on
// the toolpath's left, and the wire bounds solid material):
//   outside/climb       walk CCW, d = +r_eff
//   outside/conventional walk reversed (CW), d = −r_eff
//   inside/climb        walk reversed (CW), d = +r_eff
//   inside/conventional walk CCW, d = −r_eff
// compact: reverse = (side == "inside") XOR conventional;
//          d = conventional ? −r_eff : +r_eff.
// on_line emits the base loop directly (d = 0 miter edge cases are
// avoided entirely); a positive allowance is warned and ignored.
//
// Exact arcs: line/circle wires emit true FeedArcCW/CCW (G2/G3);
// ellipse/spline edges fall back to a chord-tolerance polyline.  A
// left-handed sketch frame mirrors the contour into world XY, which
// flips every arc sweep.
//
// Shape: Rapid(start, retract) → plunge FeedLinear → closed loop at
// feedrate → Rapid back to retract.  Single pass — no stepdown.

namespace {

CamGenerateResult generate_contour_2d_toolpath(
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

  // An absent contour block falls back to the struct defaults
  // (outside / depth 1.0 / no allowance) so old documents still
  // generate.
  const ContourParameters contour = op.parameters.contour.has_value()
                                        ? op.parameters.contour.value()
                                        : ContourParameters{};

  // Side validation (generator-side — the house style for op blocks).
  if (contour.side != "outside" && contour.side != "inside" &&
      contour.side != "on_line") {
    result.ok = false;
    result.error_message =
        "Unknown contour side '" + contour.side +
        "' (expected outside, inside, or on_line).";
    return result;
  }
  if (contour.depth_mm <= 0.0) {
    result.ok = false;
    result.error_message = "The contour depth must be positive.";
    return result;
  }
  if (contour.stock_allowance_mm < 0.0) {
    result.ok = false;
    result.error_message =
        "The stock allowance must not be negative.";
    return result;
  }

  // cutting_direction: "conventional" flips the walk; "mixed" and
  // unknown values warn and behave as climb.
  bool conventional = false;
  if (op.parameters.cutting_direction == "conventional") {
    conventional = true;
  } else if (op.parameters.cutting_direction == "mixed") {
    result.warnings.push_back(
        "Cutting direction 'mixed' is treated as climb for a "
        "single-pass contour.");
  } else if (op.parameters.cutting_direction != "climb") {
    result.warnings.push_back(
        "Unknown cutting direction '" + op.parameters.cutting_direction +
        "' — treated as climb.");
  }

  // ── Input: the face wins over sketch profiles ──────────────────
  const bool haveFace = !context.geometry.faces.empty();
  const bool haveProfile = !context.geometry.profiles.empty();
  if (!haveFace && !haveProfile) {
    result.ok = false;
    result.error_message =
        "2D Contour requires a selected face or a sketch profile.";
    return result;
  }
  if (haveFace && haveProfile) {
    result.warnings.push_back(
        "Both a face and sketch profiles are selected — 2D Contour "
        "uses the face.");
  }
  if (!haveFace && context.geometry.profiles.size() > 1) {
    result.warnings.push_back(
        "2D Contour uses only the first sketch profile.");
  }

  // ── Base loop (face wire or sketch profile) ────────────────────
  std::vector<BaseSegment> base;
  bool profileInput = false;  // profile loops map through the sketch frame
  bool flippedFrame = false;  // left-handed sketch frame mirrors arcs
  double cutZ = 0.0;          // world Z of the cut plane
  std::optional<SketchFeatureParameters::SketchPlaneFrame> frame;

  if (haveFace) {
    const auto& resolved = context.geometry.faces[0];

    // Resolve the live TopoDS_Face from the compiled body's face map.
    TopoDS_Face face;
    std::string faceError;
    if (!cam_planning::map_face_index(resolved.body->shape,
                                      resolved.faceIndex, face,
                                      faceError)) {
      result.ok = false;
      result.error_message = faceError;
      return result;
    }

    // Near-horizontal upward face only (v1: 2.5D).
    gp_Pnt center;
    gp_Vec normal;
    if (!cam_planning::face_cut_plane(face, center, normal)) {
      result.ok = false;
      result.error_message = "The referenced face is degenerate.";
      return result;
    }
    normal.Normalize();
    // Orientation-correct the normal (see face milling: the raw
    // surface parameterization gives +Z for both box caps).
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
    }
    if (normal.Z() < cam_planning::kMaxUpwardFaceTilt) {
      result.ok = false;
      result.error_message =
          "2D Contour requires a horizontal upward-facing face.";
      return result;
    }
    cutZ = center.Z() - contour.depth_mm;

    // The largest |signed area| wire is the contour boundary (the
    // pocket outer-wire selection); exact segments where possible.
    TopoDS_Wire outerWire;
    double largestArea = -1.0;
    for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
         wireExp.Next()) {
      const TopoDS_Wire wire = TopoDS::Wire(wireExp.Current());
      std::vector<XY> loop;
      if (!cam_planning::sample_planar_wire(
              wire, /*chord_tolerance=*/0.05, loop)) {
        continue;
      }
      const double area = std::fabs(xy_signed_area(loop));
      if (area > largestArea) {
        largestArea = area;
        outerWire = wire;
      }
    }
    if (outerWire.IsNull()) {
      result.ok = false;
      result.error_message = "The face boundary could not be sampled.";
      return result;
    }
    if (!cam_planning::build_base_segments_from_wire(outerWire, base)) {
      // Non-exact edge (ellipse/spline): chord-tolerance polyline
      // fallback — all linear feeds, no arcs.
      std::vector<XY> loop;
      if (!cam_planning::sample_planar_wire(
              outerWire, /*chord_tolerance=*/0.05, loop)) {
        result.ok = false;
        result.error_message = "The face boundary could not be sampled.";
        return result;
      }
      for (size_t i = 0; i < loop.size(); ++i) {
        BaseSegment segment;
        segment.start = loop[i];
        segment.end = loop[(i + 1) % loop.size()];
        base.push_back(segment);
      }
    }
  } else {
    // ── Sketch profile path (laser-cut-style capture) ────────────
    profileInput = true;
    const SketchFeatureParameters* owningSketch =
        context.geometry.sketches.empty() ? nullptr
                                          : context.geometry.sketches[0];
    if (owningSketch == nullptr) {
      result.ok = false;
      result.error_message = "The sketch plane could not be resolved.";
      return result;
    }
    frame = cam_planning::resolve_sketch_frame(*owningSketch);
    if (!frame.has_value()) {
      result.ok = false;
      result.error_message = "The sketch plane could not be resolved.";
      return result;
    }
    // A tilted or vertical sketch plane cannot be contoured by a
    // 3-axis machine in v1 — reject it before generating garbage.
    if (std::abs(frame->normal_z) < cam_planning::kMaxUpwardFaceTilt) {
      result.ok = false;
      result.error_message =
          "2D Contour requires a horizontal sketch plane; this sketch "
          "plane is not horizontal.";
      return result;
    }
    // Left-handed in-plane frames mirror the contour into world XY,
    // which reverses every arc sweep.
    flippedFrame =
        frame->x_axis_x * frame->y_axis_y -
            frame->x_axis_y * frame->y_axis_x <
        0.0;
    cutZ = cam_planning::world_z(frame.value(), XY{0.0, 0.0},
                                 -contour.depth_mm);

    const auto& region = *context.geometry.profiles[0].region;
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
  }

  if (base.size() < 2 && !(base.size() == 1 && base[0].is_arc)) {
    result.ok = false;
    result.error_message = "The contour wire has no usable boundary.";
    return result;
  }
  // Normalize CCW (material left — the wire bounds the solid).  A
  // single full-circle segment is stored CCW already.
  if (base.size() >= 2 && base_segments_signed_area(base) < 0) {
    reverse_segments(base);
  }

  // ── Offset + direction (pocket precedent) ──────────────────────
  // r_eff rides the tool center: tool radius + radial allowance.
  // Compact rule: reverse = inside XOR conventional;
  //               d = conventional ? −r_eff : +r_eff.
  // Climb external = CCW, climb internal = CW — the same walk
  // convention the pocket finishing contours pin.
  const bool inside = contour.side == "inside";
  const bool onLine = contour.side == "on_line";
  if (onLine && contour.stock_allowance_mm > 0.0) {
    result.warnings.push_back(
        "Stock allowance is ignored for an on-line contour.");
  }
  const double rEff =
      context.tool.diameter_mm / 2.0 + contour.stock_allowance_mm;
  const bool reverse = inside != conventional;
  if (reverse) {
    reverse_segments(base);
  }
  const double d = conventional ? -rEff : rEff;

  std::vector<OffsetSegment> offsetLoop;
  if (!onLine) {
    // Miter corners for line offsets; any arc in the base forces
    // round joins internally (cam2d) — fine for both.
    if (!offset_closed_loop(base, d, offsetLoop,
                            /*round_joins=*/false)) {
      result.ok = false;
      result.error_message =
          inside ? "The contour is too small for the tool — no "
                   "machinable path."
                 : "The contour could not be offset.";
      return result;
    }
    const auto samples =
        sample_offset_loop(offsetLoop, /*tolerance=*/0.05);
    if (offset_loop_self_intersects(samples)) {
      result.ok = false;
      result.error_message =
          "The offset contour self-intersects — the feature is "
          "narrower than the tool.";
      return result;
    }
    for (const auto& segment : offsetLoop) {
      if (segment.is_arc && segment.radius <= kOffsetEps) {
        result.ok = false;
        result.error_message =
            "The tool does not fit inside the contour (a corner "
            "collapses).";
        return result;
      }
    }
  }

  // ── Guards ─────────────────────────────────────────────────────
  // Retract must clear the cut plane and the stock top; the cut plane
  // must stay inside the stock (the pocket retract-guard contract).
  const double retractZ = context.setup.retract_height;
  if (retractZ < cutZ) {
    std::ostringstream message;
    message << "Retract height (" << retractZ
            << " mm) is below the cut plane (" << cutZ
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
      message << "Retract height (" << retractZ
              << " mm) is below the stock top (" << stockTopZ
              << " mm) — rapids travel through unmilled stock. Raise "
                 "it in the Setup.";
      result.warnings.push_back(message.str());
    }
    if (cutZ < stockBottomZ - 1e-6) {
      std::ostringstream message;
      message << "The cut plane (" << cutZ
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

  // Sketch loops map through the sketch plane frame; face loops are
  // already world XY at a fixed cut-plane height.
  const auto toWorld = [&](const XY& p) {
    return profileInput ? cam_planning::world_point(frame.value(), p)
                        : XY{p.x, p.y};
  };
  const auto toZ = [&](const XY& p) {
    return profileInput
               ? cam_planning::world_z(frame.value(), p, -contour.depth_mm)
               : cutZ;
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
  // G2/G3 live in machine XY: map center and start to world and emit
  // the world-space center offset.  A left-handed sketch frame
  // mirrors the contour and reverses the arc sweep.
  const auto append_arc = [&](bool cw, const XY& start, const XY& end,
                              const XY& center, double z, double feed) {
    const XY wStart = toWorld(start);
    const XY wEnd = toWorld(end);
    const XY wCenter = toWorld(center);
    ToolpathMove move;
    move.kind = (cw != flippedFrame) ? ToolpathMoveKind::FeedArcCW
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

  // Rapid in → plunge → closed loop → retract (single pass).
  const auto emit_base_loop = [&]() {
    append_rapid_xy(base.front().start, retractZ);
    append_feed_xy(base.front().start, toZ(base.front().start),
                   plungeFeed);
    for (const auto& segment : base) {
      if (segment.is_arc) {
        append_arc(!segment.ccw, segment.start, segment.end,
                   segment.center, toZ(segment.end), feedrate);
      } else {
        append_feed_xy(segment.end, toZ(segment.end), feedrate);
      }
    }
    append_rapid_xy(base.back().end, retractZ);
  };
  const auto emit_offset_loop = [&]() {
    append_rapid_xy(offsetLoop.front().start, retractZ);
    append_feed_xy(offsetLoop.front().start, toZ(offsetLoop.front().start),
                   plungeFeed);
    for (const auto& segment : offsetLoop) {
      if (segment.is_arc) {
        append_arc(segment.cw, segment.start, segment.end, segment.center,
                   toZ(segment.end), feedrate);
      } else {
        append_feed_xy(segment.end, toZ(segment.end), feedrate);
      }
    }
    append_rapid_xy(offsetLoop.back().end, retractZ);
  };

  if (onLine) {
    emit_base_loop();
  } else {
    emit_offset_loop();
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No contour moves were produced.";
    return result;
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace
