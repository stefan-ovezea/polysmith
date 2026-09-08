// ── 2D Pocket generator ────────────────────────────────────────────
//
// Resolves the referenced 3D face (the pocket floor) plus island faces
// (avoidance regions) through the witness machinery (the TNP stress
// path), requires a near-horizontal upward floor, insets the floor
// boundary by the tool radius, grows holes and island footprints by the
// tool radius, and fills the result with clipped zigzag rows.  A broken
// island reference FAILS generation — skipping its subtraction would
// mill straight into a boss.
//
// Region model:
//   * outer loop  = the face wire with the largest |signed area|
//     (multi-wire faces ARE allowed), inset by tool radius with miter
//     corners.
//   * inner wires = classified by the walls around them (adjacent-face
//     centre-of-mass probe against the floor plane):
//       - walls ABOVE the floor  → a boss rising from the pocket —
//         always avoided, grown outward by the tool radius (round
//         joins — growth is the opposite of the miter inset);
//       - walls below/coplanar   → an open hole (or a seam between
//         floor patches) — NOT avoided: the rows cross it, clearing
//         the stock plug that sits in the finished part's hole;
//       - nothing probeable      → conservatively avoided.
//   * islands     = avoidance faces; footprint grown by the tool
//     radius, subtracted per level only while the island is solid
//     there: islandTopZ > levelZ.  At islandTopZ ≈ levelZ the
//     footprint is INCLUDED — the pass cuts the stock above the island
//     flush with its top.  Island tops strictly between the floor and
//     the stock top join the level plan (extra_levels), so the stock
//     above a short boss is cleared down to its top first.
//   * fill        = the face-milling row planner (minN/maxN sweep,
//     spacing = max(diam × stepover%, 0.1), last row pinned to maxN,
//     GLOBAL row index for zigzag parity).  Each row clips against the
//     inset polygon, then against every ACTIVE avoidance polygon; the
//     clip may split a row into several pieces (bosses, non-convex
//     boundaries) — every piece is emitted, each with its own
//     rapid-plunge-feed-rapid cycle.
//   * finishing   = after the fill at every level, one closed CLIMB
//     contour per active avoidance loop (CCW, riding the grown loop
//     itself) and one around the outer inset (CW, reversed) — a round
//     boss gets a circular path, and the zigzag scallops against the
//     walls are cleaned up.

namespace {

// Dedupes consecutive near-identical points and drops a repeated
// closing point.  Returns false when fewer than 3 points remain.
bool clean_sampled_loop(std::vector<XY>& loop) {
  std::vector<XY> deduped;
  for (const auto& point : loop) {
    if (deduped.empty() ||
        xy_length(point.x - deduped.back().x,
                  point.y - deduped.back().y) > 0.01) {
      deduped.push_back(point);
    }
  }
  if (deduped.size() >= 2 &&
      xy_length(deduped.front().x - deduped.back().x,
                deduped.front().y - deduped.back().y) < 1e-9) {
    deduped.pop_back();  // sampled loops may repeat the first point
  }
  loop = std::move(deduped);
  return loop.size() >= 3;
}

// One sampled floor-face wire plus its live 3D wire (kept for the
// hole/boss wall probe).
struct FaceLoop {
  std::vector<XY> points;
  TopoDS_Wire wire;
};

// Classifies a floor-face inner wire by the walls around it.  Every
// face adjacent to the wire's edges (excluding the floor itself) is
// probed with its centre of mass against the floor plane:
//   * any wall ABOVE the floor (COM z > faceZ + eps) → the wire
//     borders a raised boss — the tool must avoid it;
//   * all walls below or coplanar → the wire borders an open hole (or
//     a seam between floor patches) — the rows cross it and clear the
//     stock plug that sits in the finished part's hole.
// A wire with no probeable wall stays avoided (conservative).
bool classify_inner_wire(const TopoDS_Shape& body, const TopoDS_Face& floorFace,
                         const TopoDS_Wire& wire, double faceZ,
                         bool& isBoss) {
  // OCCT 8: use the NCollection map directly — the TopTools alias is
  // deprecated and not shipped in the install include dir.
  NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                             TopTools_ShapeMapHasher>
      edgeFaces;
  TopExp::MapShapesAndAncestors(body, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
  isBoss = true;  // conservative default: unknown walls stay avoided
  bool probed = false;
  for (TopExp_Explorer edgeExp(wire, TopAbs_EDGE); edgeExp.More();
       edgeExp.Next()) {
    const TopoDS_Shape& edge = edgeExp.Current();
    if (!edgeFaces.Contains(edge)) {
      continue;
    }
    for (const auto& ancestor : edgeFaces.FindFromKey(edge)) {
      if (ancestor.IsSame(floorFace)) {
        continue;
      }
      GProp_GProps props;
      try {
        BRepGProp::SurfaceProperties(TopoDS::Face(ancestor), props);
      } catch (const Standard_Failure&) {
        continue;
      }
      probed = true;
      // A boss wall (its side face) or a fillet/chamfer ringing the
      // boss sits above the floor; a hole wall or a coplanar floor
      // seam does not.  One wall above decides — mixed wires (a boss
      // with a hole through it) are avoided.
      if (props.CentreOfMass().Z() > faceZ + 0.1) {
        return true;
      }
    }
  }
  if (!probed) {
    return true;  // nothing to probe — conservatively avoided
  }
  isBoss = false;
  return true;
}

// Grows one closed loop OUTWARD by d (round joins — the inverse of the
// miter inset) and validates the growth.  Orientation does not matter
// for the caller: subtraction clips to the EXTERIOR
// (clip_segment_outside_polygon), never to a walk side.  `label` names
// the region in errors ("Island 1" / "A hole in the pocket").
//
// offset_closed_loop offsets to the RIGHT of the walk; with the loop
// normalized CCW (material left), right is the outward growth.
bool grow_avoidance_loop(const std::string& label,
                         const std::vector<XY>& base, double d,
                         std::vector<XY>& out_loop, std::string& error) {
  std::vector<BaseSegment> segments;
  for (size_t i = 0; i < base.size(); ++i) {
    BaseSegment segment;
    segment.start = base[i];
    segment.end = base[(i + 1) % base.size()];
    segments.push_back(segment);
  }
  if (base_segments_signed_area(segments) < 0) {
    reverse_segments(segments);
  }
  std::vector<OffsetSegment> grownSegments;
  if (!offset_closed_loop(segments, d, grownSegments,
                          /*round_joins=*/true)) {
    error = label + " could not be offset.";
    return false;
  }
  auto grown = sample_offset_loop(grownSegments, /*tolerance=*/0.05);
  if (grown.size() < 3) {
    error = label + " could not be offset.";
    return false;
  }

  // Narrower-than-tool validation, two checks:
  //   (1) the grown loop must not cross itself — the two sides of a
  //       neck narrower than the tool pass through each other;
  //   (2) every grown point must stay >= d from every base VERTEX.
  //       Vertices are the tightest probe: grown straight edges and
  //       join arcs sit exactly at distance d from the boundary, while
  //       base SEGMENTS would falsely under-measure curved bases by
  //       the chord sagitta (chords of a sampled circle lie inside the
  //       true boundary).
  if (offset_loop_self_intersects(grown)) {
    error = label +
            " is narrower than the tool — the pocket cannot clear "
            "around it.";
    return false;
  }
  for (const auto& p : grown) {
    double minDist = 1e9;
    for (const auto& vertex : base) {
      minDist =
          std::min(minDist, xy_length(p.x - vertex.x, p.y - vertex.y));
    }
    if (minDist < d - 1e-3) {
      error = label +
              " is narrower than the tool — the pocket cannot clear "
              "around it.";
      return false;
    }
  }

  out_loop = std::move(grown);
  return true;
}

// One subtractable region: a grown clip loop (any orientation —
// subtraction clips to the exterior).  Islands also carry their top Z
// for the per-level filter and level insertion.
struct AvoidanceRegion {
  std::vector<XY> clipLoop;  // grown polygon (subtracted per level)
  double topZ = 0.0;         // islands only — face wires are always active
  bool alwaysActive = false; // a boss wire of the pocket floor
  std::string label;         // "Island N" or "A boss in the pocket"
};

CamGenerateResult generate_pocket_2d_toolpath(
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
        "2D Pocket requires a selected face (geometry changed — "
        "re-select it).";
    return result;
  }
  const auto& resolved = context.geometry.faces[0];

  // ── Outer loop ────────────────────────────────────────────────
  // Resolve the live TopoDS_Face from the compiled body's face map.
  TopoDS_Face face;
  std::string faceError;
  if (!cam_planning::map_face_index(resolved.body->shape,
                                    resolved.faceIndex, face, faceError)) {
    result.ok = false;
    result.error_message = faceError;
    return result;
  }

  // Near-horizontal upward floor only (v1: 2.5D).
  gp_Pnt center;
  gp_Vec normal;
  if (!cam_planning::face_cut_plane(face, center, normal)) {
    result.ok = false;
    result.error_message = "The referenced face is degenerate.";
    return result;
  }
  normal.Normalize();
  // Orientation-correct the normal (see face milling: the raw surface
  // parameterization gives +Z for both box caps).
  if (face.Orientation() == TopAbs_REVERSED) {
    normal.Reverse();
  }
  if (normal.Z() < cam_planning::kMaxUpwardFaceTilt) {  // within ~5° of +Z
    result.ok = false;
    result.error_message =
        "2D Pocket requires a horizontal upward-facing face.";
    return result;
  }
  const double faceZ = center.Z();

  // Sample EVERY wire of the floor face.  The largest |signed area| is
  // the pocket boundary; the rest are probed for boss-vs-hole walls.
  std::vector<FaceLoop> loops;
  for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
       wireExp.Next()) {
    FaceLoop loop;
    loop.wire = TopoDS::Wire(wireExp.Current());
    if (!cam_planning::sample_planar_wire(
            loop.wire, /*chord_tolerance=*/0.05, loop.points)) {
      result.ok = false;
      result.error_message = "The face boundary could not be sampled.";
      return result;
    }
    if (!clean_sampled_loop(loop.points)) {
      result.ok = false;
      result.error_message = "The face boundary could not be sampled.";
      return result;
    }
    loops.push_back(std::move(loop));
  }
  if (loops.empty()) {
    result.ok = false;
    result.error_message = "The face boundary could not be sampled.";
    return result;
  }
  size_t outerIndex = 0;
  double largestArea = -1.0;
  for (size_t i = 0; i < loops.size(); ++i) {
    const double area = std::fabs(xy_signed_area(loops[i].points));
    if (area > largestArea) {
      largestArea = area;
      outerIndex = i;
    }
  }
  std::vector<XY> outer = loops[outerIndex].points;

  // Tool-radius inset of the outer loop — the exact face-milling
  // pattern (miter corners; the tool path must never round into the
  // wall).
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < outer.size(); ++i) {
    const auto& a = outer[i];
    const auto& b = outer[(i + 1) % outer.size()];
    BaseSegment segment;
    segment.start = a;
    segment.end = b;
    base.push_back(segment);
  }
  if (base_segments_signed_area(base) < 0) {
    reverse_segments(base);
  }
  reverse_segments(base);  // material right → +d is the inset

  const double inset = context.tool.diameter_mm / 2.0;
  std::vector<OffsetSegment> insetLoop;
  if (!offset_closed_loop(base, inset, insetLoop,
                          /*round_joins=*/false)) {
    result.ok = false;
    result.error_message =
        "The pocket is smaller than the tool — no machinable area.";
    return result;
  }
  auto insetPoints = sample_offset_loop(insetLoop, /*tolerance=*/0.05);
  // Collapsed-inset validation (face-milling pattern): every inset
  // point must stay at distance >= d from EVERY boundary segment.
  bool insetValid = insetPoints.size() >= 3;
  for (const auto& p : insetPoints) {
    double minDist = 1e9;
    for (size_t i = 0; i < outer.size(); ++i) {
      const auto& a = outer[i];
      const auto& b = outer[(i + 1) % outer.size()];
      minDist = std::min(minDist, xy_point_segment_distance(p, a, b));
    }
    if (minDist < inset - 1e-3) {
      insetValid = false;
      break;
    }
  }
  if (!insetValid || offset_loop_self_intersects(insetPoints)) {
    result.ok = false;
    result.error_message =
        "The pocket is smaller than the tool — no machinable area.";
    return result;
  }
  // The clip polygon must be CCW (inside = left of each edge); the
  // inset loop walks CW (material right), so flip it for clipping.
  if (xy_signed_area(insetPoints) < 0) {
    std::reverse(insetPoints.begin(), insetPoints.end());
  }

  // ── Avoidance regions ─────────────────────────────────────────
  std::vector<AvoidanceRegion> avoidance;

  // Face inner wires: probe the walls around each wire — bosses are
  // always avoided at every level; open holes (walls below the floor,
  // or floor seams) are crossed by the rows instead.  The boss BASE
  // loops are kept for the island-overlap hint.
  std::vector<const std::vector<XY>*> bossBaseLoops;
  for (size_t i = 0; i < loops.size(); ++i) {
    if (i == outerIndex) {
      continue;
    }
    bool isBoss = true;
    classify_inner_wire(resolved.body->shape, face, loops[i].wire, faceZ,
                        isBoss);
    if (!isBoss) {
      continue;  // open hole — the rows cross it (see header comment)
    }
    AvoidanceRegion boss;
    boss.alwaysActive = true;
    boss.label = "A boss in the pocket";
    std::string error;
    if (!grow_avoidance_loop(boss.label, loops[i].points, inset,
                             boss.clipLoop, error)) {
      result.ok = false;
      result.error_message = error;
      return result;
    }
    bossBaseLoops.push_back(&loops[i].points);
    avoidance.push_back(std::move(boss));
  }

  // Island faces (avoidance regions): near-horizontal tops, footprints
  // grown by the tool radius, subtracted per level while solid.
  int islandNumber = 0;
  for (const auto& islandRef : context.geometry.avoidance_faces) {
    ++islandNumber;
    const std::string label =
        "Island " + std::to_string(islandNumber);

    TopoDS_Face islandFace;
    std::string islandError;
    if (!cam_planning::map_face_index(islandRef.body->shape,
                                      islandRef.faceIndex, islandFace,
                                      islandError)) {
      result.ok = false;
      result.error_message = islandError;
      return result;
    }
    gp_Pnt islandCenter;
    gp_Vec islandNormal;
    if (!cam_planning::face_cut_plane(islandFace, islandCenter,
                                      islandNormal)) {
      result.ok = false;
      result.error_message = label + " is degenerate.";
      return result;
    }
    islandNormal.Normalize();
    if (islandFace.Orientation() == TopAbs_REVERSED) {
      islandNormal.Reverse();
    }
    if (islandNormal.Z() < cam_planning::kMaxUpwardFaceTilt) {
      result.ok = false;
      result.error_message =
          label + " must be a horizontal upward-facing face.";
      return result;
    }
    const double islandTopZ = islandCenter.Z();

    // Footprint = the island top's largest wire (inner wires, like a
    // hole through the boss, are covered by the grown outer one).
    std::vector<XY> islandLoop;
    double islandArea = -1.0;
    for (TopExp_Explorer wireExp(islandFace, TopAbs_WIRE); wireExp.More();
         wireExp.Next()) {
      std::vector<XY> loop;
      if (!cam_planning::sample_planar_wire(
              TopoDS::Wire(wireExp.Current()), /*chord_tolerance=*/0.05,
              loop) ||
          !clean_sampled_loop(loop)) {
        continue;
      }
      const double area = std::fabs(xy_signed_area(loop));
      if (area > islandArea) {
        islandArea = area;
        islandLoop = std::move(loop);
      }
    }
    if (islandLoop.size() < 3) {
      result.ok = false;
      result.error_message = label + " has no usable boundary.";
      return result;
    }

    // Outside the pocket = subtracts nothing (no hard failure — the
    // operator may re-pick, and the pocket itself is still valid).
    const XY centroid = xy_area_centroid(islandLoop);
    if (!xy_point_in_polygon(centroid, outer)) {
      std::ostringstream message;
      message << label
              << " lies outside the pocket — it has no effect.";
      result.warnings.push_back(message.str());
    }

    // Single-pass island-on-boss hint: with no stepdown an island
    // adds no level, and a boss that is part of the floor face is
    // avoided anyway — say so instead of leaving the operator to
    // wonder why the path did not change.
    const bool multiPass = op.parameters.stepdown_mm.has_value() &&
                           op.parameters.stepdown_mm.value() > 1e-9;
    if (!multiPass) {
      for (const auto* bossLoop : bossBaseLoops) {
        if (xy_point_in_polygon(centroid, *bossLoop)) {
          std::ostringstream message;
          message << label
                  << " lies on a boss that is part of the pocket face "
                     "(already avoided) — it only changes the toolpath "
                     "with a Stepdown set (multi-pass).";
          result.warnings.push_back(message.str());
          break;
        }
      }
    }

    AvoidanceRegion island;
    island.label = label;
    island.topZ = islandTopZ;
    std::string error;
    if (!grow_avoidance_loop(label, islandLoop, inset, island.clipLoop,
                             error)) {
      result.ok = false;
      result.error_message = error;
      return result;
    }
    avoidance.push_back(std::move(island));
  }

  // ── Levels ────────────────────────────────────────────────────
  // Shared stepdown planner (face milling lockstep).  Island tops
  // strictly between the floor and the stock top join the plan so the
  // stock above a short boss is cleared down to its top.
  std::vector<double> islandTops;
  for (const auto& region : avoidance) {
    if (!region.alwaysActive) {
      islandTops.push_back(region.topZ);
    }
  }
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
      islandTops, levels, result.warnings);

  // ── Guards ────────────────────────────────────────────────────
  const double retractZ = context.setup.retract_height;
  // Guard 1: rapid-safety (see face milling — same contract).
  if (retractZ < faceZ) {
    std::ostringstream message;
    message << "Retract height (" << retractZ
            << " mm) is below the face height (" << faceZ
            << " mm) — rapids travel through the material. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  // Guard 2 (multi-pass): rapids must clear the unmilled stock above
  // the first level, not just the face.
  if (levels.size() > 1 && haveStockTop && retractZ < stockTopZ) {
    std::ostringstream message;
    message << "Retract height (" << retractZ
            << " mm) is below the stock top (" << stockTopZ
            << " mm) — rapids travel through unmilled stock. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  if (op.parameters.strategy.has_value() &&
      op.parameters.strategy.value() != "zigzag") {
    result.warnings.push_back(
        "Only the zigzag strategy is implemented in v1 — generating "
        "zigzag rows.");
  }

  // ── Zigzag fill ───────────────────────────────────────────────
  const double stepover =
      op.parameters.stepover_percent.value_or(
          context.tool.default_stepover_percent) /
      100.0;
  const double spacing =
      std::max(context.tool.diameter_mm * stepover, 0.1);
  const double angleRad =
      op.parameters.zigzag_angle_deg.value_or(0.0) * kPi / 180.0;
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

  const double feedrate = op.parameters.feedrate_mm_per_min > 0.0
                              ? op.parameters.feedrate_mm_per_min
                              : context.tool.default_feedrate_mm_per_min;
  const double plungeFeed =
      op.parameters.plunge_feedrate_mm_per_min > 0.0
          ? op.parameters.plunge_feedrate_mm_per_min
          : context.tool.default_plunge_feedrate_mm_per_min;

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

  // One machinable piece of a row (holes and non-convex boundaries may
  // split a row into several).
  struct RowPiece {
    XY start;
    XY end;
  };
  const auto clip_to_pieces = [](const XY& a, const XY& b,
                                 const std::vector<XY>& poly) {
    std::vector<RowPiece> pieces;
    const auto clipped = clip_segment_to_polygon(a, b, poly);
    for (size_t k = 0; k + 1 < clipped.size(); k += 2) {
      pieces.push_back({clipped[k], clipped[k + 1]});
    }
    return pieces;
  };

  // One finishing contour, clipped against the given avoidance
  // polygons: segments whose tool path would pass through another
  // avoidance's clearance are removed (the wall is unreachable there).
  // Consecutive surviving segments are chained into runs, each with
  // its own rapid-plunge-feed-rapid cycle.  `reverse` walks the stored
  // orientation backwards — the climb direction for the outer inset,
  // which is stored CCW for clipping (external profile climb = CW; the
  // avoidance loops are CCW-grown and traced as-is, the climb
  // direction for an internal profile).
  const auto append_contour = [&](
      const std::vector<XY>& loop, double z, bool reverse,
      const std::vector<const std::vector<XY>*>& clipAgainst) {
    bool chainOpen = false;
    XY chainEnd{0.0, 0.0};
    const auto closeChain = [&]() {
      if (chainOpen) {
        append_rapid_xy(chainEnd, retractZ);
        chainOpen = false;
      }
    };
    for (size_t i = 0; i < loop.size(); ++i) {
      // Reversed tracing must also visit the indices backwards: stepping
      // forward through the loop while swapping endpoints emits segments
      // in non-adjacent order, so no chain ever forms and every segment
      // gets its own rapid (a string of disconnected dots instead of one
      // closed contour).
      const size_t j = reverse ? (loop.size() - 1 - i) : i;
      const XY a = loop[j];
      const XY b = reverse ? loop[(j + loop.size() - 1) % loop.size()]
                           : loop[(j + 1) % loop.size()];
      std::vector<RowPiece> current{{a, b}};
      for (const auto* avoid : clipAgainst) {
        std::vector<RowPiece> remaining;
        for (const auto& piece : current) {
          const auto clipped =
              clip_segment_outside_polygon(piece.start, piece.end, *avoid);
          for (size_t k = 0; k + 1 < clipped.size(); k += 2) {
            remaining.push_back({clipped[k], clipped[k + 1]});
          }
        }
        current = std::move(remaining);
        if (current.empty()) {
          break;
        }
      }
      for (const auto& piece : current) {
        if (!chainOpen ||
            xy_length(piece.start.x - chainEnd.x,
                      piece.start.y - chainEnd.y) > 1e-6) {
          closeChain();
          append_rapid_xy(piece.start, retractZ);
          append_feed_xy(piece.start, z, plungeFeed);
          chainOpen = true;
        }
        append_feed_xy(piece.end, z, feedrate);
        chainEnd = piece.end;
      }
    }
    closeChain();
  };

  // The GLOBAL row index keeps the zigzag alternation going across
  // levels, so consecutive cutting rows always swap direction.
  int globalRow = 0;
  for (const double levelZ : levels) {
    // Per-level island filter: an island is subtracted only while it is
    // still solid there (islandTopZ > levelZ).  At islandTopZ ≈ levelZ
    // the footprint is INCLUDED — the pass cuts the stock above the
    // island flush with its top.  Face boss wires are subtracted at
    // every level.
    std::vector<const std::vector<XY>*> active;
    for (const auto& region : avoidance) {
      if (region.alwaysActive || region.topZ > levelZ + 1e-6) {
        active.push_back(&region.clipLoop);
      }
    }

    for (int row = 0; row < rowCount; ++row, ++globalRow) {
      // Pin the final row to maxN (face-milling pattern).
      const double n = (row == rowCount - 1)
                           ? maxN
                           : minN + spacing * static_cast<double>(row);
      const XY anchor{rowNormal.x * n, rowNormal.y * n};
      const XY a{anchor.x - rowDir.x * 10000.0,
                 anchor.y - rowDir.y * 10000.0};
      const XY b{anchor.x + rowDir.x * 10000.0,
                 anchor.y + rowDir.y * 10000.0};

      std::vector<RowPiece> pieces = clip_to_pieces(a, b, insetPoints);
      // Subtract every active avoidance polygon (the outside clip keeps
      // the exterior).  Each subtraction may split pieces again.
      for (const auto* avoid : active) {
        std::vector<RowPiece> remaining;
        for (const auto& piece : pieces) {
          const auto clipped =
              clip_segment_outside_polygon(piece.start, piece.end, *avoid);
          for (size_t k = 0; k + 1 < clipped.size(); k += 2) {
            remaining.push_back({clipped[k], clipped[k + 1]});
          }
        }
        pieces = std::move(remaining);
        if (pieces.empty()) {
          break;
        }
      }

      if (zigzag && globalRow % 2 == 1) {
        // Zigzag back: cut the pieces of this row in reverse order,
        // each walked the other way.  (No linked-in move — every piece
        // retracts and re-enters in v1.)
        std::reverse(pieces.begin(), pieces.end());
        for (auto& piece : pieces) {
          std::swap(piece.start, piece.end);
        }
      }

      for (const auto& piece : pieces) {
        append_rapid_xy(piece.start, retractZ);
        append_feed_xy(piece.start, levelZ, plungeFeed);
        append_feed_xy(piece.end, levelZ, feedrate);
        append_rapid_xy(piece.end, retractZ);
      }
    }

    // Finishing contours — after the fill, climb passes around every
    // active avoidance loop (boss walls and solid islands) and around
    // the outer inset wall.  A round boss gets a circular path and the
    // zigzag scallops against the walls are cleaned up.  Each contour
    // is clipped against the OTHER active avoidances (never itself —
    // it rides its own boundary, which the outside clip would remove).
    for (const auto& region : avoidance) {
      if (!(region.alwaysActive || region.topZ > levelZ + 1e-6)) {
        continue;
      }
      std::vector<const std::vector<XY>*> others;
      for (const auto* other : active) {
        if (other != &region.clipLoop) {
          others.push_back(other);
        }
      }
      append_contour(region.clipLoop, levelZ, /*reverse=*/false, others);
    }
    append_contour(insetPoints, levelZ, /*reverse=*/true, active);
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
