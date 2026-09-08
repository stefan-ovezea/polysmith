// ── Face milling generator ────────────────────────────────────────
//
// Resolves the referenced 3D face through the witness machinery (the
// TNP stress path), requires a near-horizontal upward face, insets the
// boundary by the tool radius, and fills it with clipped zigzag rows.
// Rapids at the retract height, plunges at the plunge feed, rows at
// the cutting feed.

namespace {

CamGenerateResult generate_face_milling_toolpath(
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

  if (context.geometry.faces.empty()) {
    result.ok = false;
    result.error_message =
        "Face milling requires a selected face (geometry changed — "
        "re-select it).";
    return result;
  }
  const auto& resolved = context.geometry.faces[0];

  // Resolve the live TopoDS_Face from the compiled body's face map.
  TopoDS_Face face;
  std::string faceError;
  if (!cam_planning::map_face_index(resolved.body->shape,
                                    resolved.faceIndex, face, faceError)) {
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
  // Orientation-correct the normal: the raw surface parameterization
  // gives +Z for BOTH box caps — only the face orientation tells the
  // upward side apart from the downward one.
  if (face.Orientation() == TopAbs_REVERSED) {
    normal.Reverse();
  }
  if (normal.Z() < cam_planning::kMaxUpwardFaceTilt) {  // within ~5° of +Z
    result.ok = false;
    result.error_message =
        "Face milling requires a horizontal upward-facing face.";
    return result;
  }
  const double faceZ = center.Z();

  // Single-wire faces only in v1 (holes in the face would need island
  // avoidance).
  int wireCount = 0;
  for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
       wireExp.Next()) {
    ++wireCount;
  }
  if (wireCount != 1) {
    result.ok = false;
    result.error_message =
        "Face milling supports faces with a single outer boundary in v1.";
    return result;
  }

  // Boundary polygon — shared chord-tolerance wire sampling with
  // reversed-piece chaining (cam_planning::sample_planar_wire).
  std::vector<XY> boundary;
  for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
       wireExp.Next()) {
    if (!cam_planning::sample_planar_wire(
            TopoDS::Wire(wireExp.Current()), /*chord_tolerance=*/0.05,
            boundary)) {
      result.ok = false;
      result.error_message = "The face boundary could not be sampled.";
      return result;
    }
  }
  // Dedupe consecutive near-identical points.
  std::vector<XY> deduped;
  for (const auto& point : boundary) {
    if (deduped.empty() ||
        xy_length(point.x - deduped.back().x,
                  point.y - deduped.back().y) > 0.01) {
      deduped.push_back(point);
    }
  }
  if (deduped.size() < 3) {
    result.ok = false;
    result.error_message = "The face boundary could not be sampled.";
    return result;
  }
  if (xy_length(deduped.front().x - deduped.back().x,
                deduped.front().y - deduped.back().y) < 1e-9) {
    deduped.pop_back();  // sampled loops may repeat the first point
  }

  // Normalize to CCW (material left) and build base segments.
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < deduped.size(); ++i) {
    const auto& a = deduped[i];
    const auto& b = deduped[(i + 1) % deduped.size()];
    BaseSegment segment;
    segment.start = a;
    segment.end = b;
    base.push_back(segment);
  }
  if (base_segments_signed_area(base) < 0) {
    reverse_segments(base);
  }
  // offset_closed_loop offsets to the RIGHT of the walk (the laser's
  // scrap side).  Reversing puts the material on the right, so the
  // same +d offset becomes the tool-radius INSET into the material.
  reverse_segments(base);

  const double inset = context.tool.diameter_mm / 2.0;
  std::vector<OffsetSegment> insetLoop;
  // Miter corners: the tool path must never round into the material.
  if (!offset_closed_loop(base, inset, insetLoop,
                          /*round_joins=*/false)) {
    result.ok = false;
    result.error_message =
        "The face is smaller than the tool — no machinable area.";
    return result;
  }
  auto insetPoints = sample_offset_loop(insetLoop, /*tolerance=*/0.05);
  // A collapsed inset (face smaller than the tool) can produce a
  // plausible-looking mitered polygon — validate physically: every
  // inset point must stay at distance >= d from EVERY boundary
  // segment (it lies on its own edge's offset line at exactly d, so a
  // smaller distance means the tool would overlap the opposite wall).
  bool insetValid = insetPoints.size() >= 3;
  for (const auto& p : insetPoints) {
    double minDist = 1e9;
    for (size_t i = 0; i < deduped.size(); ++i) {
      const auto& a = deduped[i];
      const auto& b = deduped[(i + 1) % deduped.size()];
      minDist = std::min(minDist, cam2d::xy_point_segment_distance(p, a, b));
    }
    if (minDist < inset - 1e-3) {
      insetValid = false;
      break;
    }
  }
  if (!insetValid || offset_loop_self_intersects(insetPoints)) {
    result.ok = false;
    result.error_message =
        "The face is smaller than the tool — no machinable area.";
    return result;
  }
  // The clip polygon must be CCW (inside = left of each edge); the
  // inset loop walks CW (material right), so flip it for clipping.
  if (xy_signed_area(insetPoints) < 0) {
    std::reverse(insetPoints.begin(), insetPoints.end());
  }

  // ── Zigzag fill ────────────────────────────────────────────────
  const double stepover =
      op.parameters.stepover_percent.value_or(
          context.tool.default_stepover_percent) /
      100.0;
  const double spacing =
      std::max(context.tool.diameter_mm * stepover, 0.1);
  const double angleRad =
      op.parameters.zigzag_angle_deg.value_or(0.0) * 3.14159265358979323846 /
      180.0;
  const XY rowDir{std::cos(angleRad), std::sin(angleRad)};
  const XY rowNormal{-rowDir.y, rowDir.x};
  const bool zigzag =
      !op.parameters.strategy.has_value() ||
      op.parameters.strategy.value() == "zigzag";

  // Row sweep extent along the normal.
  double minN = 1e9;
  double maxN = -1e9;
  for (const auto& point : insetPoints) {
    const double n = point.x * rowNormal.x + point.y * rowNormal.y;
    minN = std::min(minN, n);
    maxN = std::max(maxN, n);
  }
  const int rowCount =
      std::max(1, static_cast<int>(std::ceil((maxN - minN) / spacing)) + 1);

  const double retractZ = context.setup.retract_height;
  // Rapid-safety guard: when the retract height sits BELOW the face,
  // every rapid travels through the material and every entry move
  // rises up out of it — on the machine that crashes the tool.  v1
  // warns instead of failing; the operator raises it in the Setup.
  if (retractZ < faceZ) {
    std::ostringstream message;
    message << "Retract height (" << retractZ
            << " mm) is below the face height (" << faceZ
            << " mm) — rapids travel through the material. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  const double feedrate = op.parameters.feedrate_mm_per_min > 0.0
                              ? op.parameters.feedrate_mm_per_min
                              : context.tool.default_feedrate_mm_per_min;
  const double plungeFeed =
      op.parameters.plunge_feedrate_mm_per_min > 0.0
          ? op.parameters.plunge_feedrate_mm_per_min
          : context.tool.default_plunge_feedrate_mm_per_min;

  // Multi-pass levels: with a stepdown, cut from the STOCK TOP down to
  // the face (first level at stockTop − stepdown, last pinned to the
  // face — stock 23 / face 20 / stepdown 2 → 21, 20).  No stepdown, a
  // non-positive stepdown, or no resolvable stock = the legacy single
  // pass at the face.  The shared cam_planning planner keeps face
  // milling and pocketing level plans in lockstep.
  std::vector<double> levels;
  double stockTopZ = 0.0;
  bool haveStockTop = false;
  if (op.parameters.stepdown_mm.has_value() &&
      op.parameters.stepdown_mm.value() > 1e-9) {
    haveStockTop = cam_stock::stock_top_z(context.document,
                                          context.setup.stock, stockTopZ);
  }
  cam_planning::plan_stepdown_levels(
      stockTopZ, faceZ, op.parameters.stepdown_mm.value_or(0.0),
      /*extra_levels=*/{}, levels, result.warnings);

  // Guard 2 (multi-pass): rapids must clear the unmilled stock above
  // the first level, not just the face.  Guard 1 above already covers
  // retract < face; this covers the band between face and stock top.
  if (levels.size() > 1 && haveStockTop && retractZ < stockTopZ) {
    std::ostringstream message;
    message << "Retract height (" << retractZ
            << " mm) is below the stock top (" << stockTopZ
            << " mm) — rapids travel through unmilled stock. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }

  Toolpath toolpath;
  toolpath.op_id = op.op_id;

  const auto append_rapid_xy = [&](const XY& p, double z) {
    ToolpathMove move;
    move.kind = ToolpathMoveKind::Rapid;
    move.x = p.x;
    move.y = p.y;
    move.z = z;
    move.laser_on = false;
    toolpath.moves.push_back(move);
  };
  const auto append_feed_xy = [&](const XY& p, double z, double feed) {
    ToolpathMove move;
    move.kind = ToolpathMoveKind::FeedLinear;
    move.x = p.x;
    move.y = p.y;
    move.z = z;
    move.feedrate_mm_per_min = feed;
    move.laser_on = false;
    toolpath.moves.push_back(move);
  };

  // The GLOBAL row index keeps the zigzag alternation going across
  // levels, so consecutive cutting rows always swap direction.
  int globalRow = 0;
  for (const double levelZ : levels) {
    for (int row = 0; row < rowCount; ++row, ++globalRow) {
      // Pin the final row to maxN: rowCount rounds UP, so the last step
      // would otherwise land past the polygon and the clip would
      // silently drop the far edge's coverage band.
      const double n = (row == rowCount - 1)
                           ? maxN
                           : minN + spacing * static_cast<double>(row);
      const XY anchor{rowNormal.x * n, rowNormal.y * n};
      XY a{anchor.x - rowDir.x * 10000.0, anchor.y - rowDir.y * 10000.0};
      XY b{anchor.x + rowDir.x * 10000.0, anchor.y + rowDir.y * 10000.0};
      const auto clipped = cam2d::clip_segment_to_polygon(a, b, insetPoints);
      if (clipped.size() < 2) {
        continue;
      }
      XY rowStart = clipped[0];
      XY rowEnd = clipped.back();
      if (zigzag && globalRow % 2 == 1) {
        std::swap(rowStart, rowEnd);
      }

      append_rapid_xy(rowStart, retractZ);
      append_feed_xy(rowStart, levelZ, plungeFeed);
      append_feed_xy(rowEnd, levelZ, feedrate);
      append_rapid_xy(rowEnd, retractZ);
    }
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No machinable rows were produced.";
    return result;
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace
