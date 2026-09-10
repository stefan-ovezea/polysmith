// ── Adaptive Clearing generator (v1: contour-parallel spiral) ─────
//
// Clears the referenced 3D face (a pocket floor or open area) with
// concentric offset loops: the boundary is inset by the effective tool
// radius (tool radius + stock allowance — the allowance rides the tool
// centre on every pass), then offset inward again at spacing =
// max(diameter × stepover%, 0.1) until the loop collapses.  Every loop
// is linked by rapids at the retract plane + plunges; islands and
// floor bosses are machined AROUND by their own outward-growing loop
// families, and all segments are clipped so the tool never enters a
// grown avoidance clearance or the wall band (the rEff band between
// the outer inset and the wall).
//
// Climb directions (the 2D contour rule — climb keeps the material on
// the toolpath's left, offset_closed_loop offsets to the RIGHT of the
// walk):
//   * outer boundary = an INSIDE contour (the wall is outside the
//     walk): climb walks CW, +d steps inward.
//   * island/boss families = OUTSIDE contours (the boss is inside the
//     walk): climb walks CCW, +d steps outward.
//   compact: reverse = inside XOR conventional,
//            dSign  = conventional ? −1 : +1 (applied to every d).
//   "conventional" flips both walks; "mixed"/unknown warn and behave
//   as climb.
//
// engagement_angle_deg is accepted and reserved for a future
// trochoidal upgrade — it does not drive v1 geometry (see
// wiki/CAM-Development.md).
//
// v1 limitations: holes in the floor are CROSSED (the stock plug is
// cleared, matching the pocket policy); the innermost region is
// covered only by the last loop's slot sweep; islands close to the
// wall may leave small uncut pockets where the tool cannot fit.

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
//     a seam between floor patches) — the spiral crosses it and clears
//     the stock plug that sits in the finished part's hole.
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
// the region in errors ("Island 1" / "A boss in the pocket").
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
  //       Vertices are the tightest probe for round-join growth: the
  //       corner join arcs sit exactly at distance d from the corner
  //       vertex but CLOSER to the adjacent base segments (the probe
  //       would falsely reject them), and sampled chords of curved
  //       bases lie inside the true boundary (segments under-measure).
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

// One avoidance region: a grown clip loop (any orientation —
// subtraction clips to the exterior), its outward-growing spiral
// family, and its per-level activation state.  Islands also carry
// their top Z for the level plan.
struct AvoidanceRegion {
  std::vector<XY> clipLoop;              // grown polygon (subtracted)
  std::vector<std::vector<XY>> family;   // concentric loops around it
  double topZ = 0.0;         // islands only — face wires are always active
  bool alwaysActive = false; // a boss wire of the pocket floor
  std::string label;         // "Island N" or "A boss in the pocket"
};

// Builds one concentric loop family from a WALK-ORIENTED closed
// boundary: loop k is offset from the FAMILY BASE at
// d_k = dSign · (startDist + k · spacing) — never iterated from the
// previous loop (no error accumulation).  A loop is accepted only when
// the offset succeeds, samples to ≥ 3 points, does not self-intersect,
// and every sample stays ≥ |d_k| − 1e-3 from the base boundary —
// SEGMENT probe for miter families (corner points sit exactly d from
// both adjacent edges) and VERTEX probe for round-join families (the
// corner join arcs are d from the corner vertex but closer to the
// adjacent segments — see grow_avoidance_loop).  A failing k=0 is a
// hard error when hardFailFirst is set (the area is smaller than the
// tool); a later failure silently ends the family — the remaining
// centre is covered by the innermost loop's slot sweep (v1
// limitation).  startDist ≤ 1e-9 emits the boundary itself as loop 0
// (an avoidance family rides its grown loop — the island finishing
// pass — with d = 0).
bool build_spiral_family(const std::vector<XY>& boundary,
                         double startDist, double spacing, double dSign,
                         bool roundJoins, bool hardFailFirst,
                         std::vector<std::vector<XY>>& outLoops,
                         std::string& error) {
  std::vector<BaseSegment> base;
  for (size_t i = 0; i < boundary.size(); ++i) {
    BaseSegment segment;
    segment.start = boundary[i];
    segment.end = boundary[(i + 1) % boundary.size()];
    base.push_back(segment);
  }
  outLoops.clear();
  for (int k = 0;; ++k) {
    const double d = dSign * (startDist + k * spacing);
    std::vector<XY> loop;
    if (k == 0 && startDist <= 1e-9) {
      loop = boundary;
    } else {
      std::vector<OffsetSegment> offsetSegments;
      if (!offset_closed_loop(base, d, offsetSegments, roundJoins)) {
        if (k == 0 && hardFailFirst) {
          error = "The pocket is smaller than the tool — no machinable "
                  "area.";
          return false;
        }
        return true;
      }
      loop = sample_offset_loop(offsetSegments, /*tolerance=*/0.05);
    }
    bool valid = loop.size() >= 3;
    if (valid) {
      valid = !offset_loop_self_intersects(loop);
    }
    if (valid) {
      const double target = std::fabs(d);
      for (const auto& p : loop) {
        double minDist = 1e9;
        if (roundJoins) {
          for (const auto& vertex : boundary) {
            minDist = std::min(minDist, xy_length(p.x - vertex.x,
                                                  p.y - vertex.y));
          }
        } else {
          for (size_t i = 0; i < boundary.size(); ++i) {
            const auto& a = boundary[i];
            const auto& b = boundary[(i + 1) % boundary.size()];
            minDist =
                std::min(minDist, xy_point_segment_distance(p, a, b));
          }
        }
        if (minDist < target - 1e-3) {
          valid = false;
          break;
        }
      }
    }
    if (!valid) {
      if (k == 0 && hardFailFirst) {
        error = "The pocket is smaller than the tool — no machinable "
                "area.";
        return false;
      }
      return true;  // collapse — the family is complete
    }
    outLoops.push_back(std::move(loop));
  }
}

CamGenerateResult generate_adaptive_clearing_toolpath(
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
        "Adaptive Clearing requires a selected face (geometry changed — "
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
        "Adaptive Clearing requires a horizontal upward-facing face.";
    return result;
  }
  const double faceZ = center.Z();

  // Sample EVERY wire of the floor face.  The largest |signed area| is
  // the area boundary; the rest are probed for boss-vs-hole walls.
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

  // cutting_direction: "conventional" flips the walks; "mixed" and
  // unknown values warn and behave as climb (the 2D contour contract).
  bool conventional = false;
  if (op.parameters.cutting_direction == "conventional") {
    conventional = true;
  } else if (op.parameters.cutting_direction == "mixed") {
    result.warnings.push_back(
        "Cutting direction 'mixed' is treated as climb for adaptive "
        "clearing.");
  } else if (op.parameters.cutting_direction != "climb") {
    result.warnings.push_back(
        "Unknown cutting direction '" + op.parameters.cutting_direction +
        "' — treated as climb.");
  }

  // The effective tool radius rides the stock allowance on every pass
  // (adaptive is roughing — the allowance must stay on the wall).
  const double rEff =
      context.tool.diameter_mm / 2.0 + op.parameters.stock_allowance_mm;
  const double stepover =
      op.parameters.stepover_percent.value_or(
          context.tool.default_stepover_percent) /
      100.0;
  const double spacing =
      std::max(context.tool.diameter_mm * stepover, 0.1);

  // ── Outer boundary family ─────────────────────────────────────
  // Climb walks CW around an inside contour, and offsetting to the
  // RIGHT of a CW walk steps inward — +d with the base normalized CCW
  // and then reversed.  Conventional flips the walk and the sign
  // together.
  std::vector<XY> outerBoundary = outer;
  if (xy_signed_area(outerBoundary) < 0) {
    std::reverse(outerBoundary.begin(), outerBoundary.end());
  }
  if (!conventional) {
    std::reverse(outerBoundary.begin(), outerBoundary.end());
  }
  const double dSign = conventional ? -1.0 : 1.0;
  std::vector<std::vector<XY>> outerFamily;
  std::string familyError;
  if (!build_spiral_family(outerBoundary, rEff, spacing, dSign,
                           /*roundJoins=*/false, /*hardFailFirst=*/true,
                           outerFamily, familyError)) {
    result.ok = false;
    result.error_message = familyError;
    return result;
  }
  // The clip polygon must be CCW (inside = left of each edge); the
  // climb walk is CW, so flip it for clipping.
  std::vector<XY> insetPoints = outerFamily[0];
  if (xy_signed_area(insetPoints) < 0) {
    std::reverse(insetPoints.begin(), insetPoints.end());
  }

  // ── Avoidance regions ─────────────────────────────────────────
  std::vector<AvoidanceRegion> avoidance;

  // Face inner wires: probe the walls around each wire — bosses are
  // always avoided at every level; open holes (walls below the floor,
  // or floor seams) are crossed by the spiral instead.  The boss BASE
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
      continue;  // open hole — the spiral crosses it (see header comment)
    }
    AvoidanceRegion boss;
    boss.alwaysActive = true;
    boss.label = "A boss in the pocket";
    std::string error;
    if (!grow_avoidance_loop(boss.label, loops[i].points, rEff,
                             boss.clipLoop, error)) {
      result.ok = false;
      result.error_message = error;
      return result;
    }
    bossBaseLoops.push_back(&loops[i].points);
    avoidance.push_back(std::move(boss));
  }

  // Island faces (avoidance regions): near-horizontal tops, footprints
  // grown by the effective tool radius, subtracted per level while
  // solid.
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
    if (!grow_avoidance_loop(label, islandLoop, rEff, island.clipLoop,
                             error)) {
      result.ok = false;
      result.error_message = error;
      return result;
    }
    avoidance.push_back(std::move(island));
  }

  // ── Avoidance families ────────────────────────────────────────
  // Each region's family grows OUTWARD from its grown clip loop
  // (outside contour: climb walks CCW, +d outward; conventional flips).
  for (auto& region : avoidance) {
    std::vector<XY> regionBoundary = region.clipLoop;
    if (xy_signed_area(regionBoundary) < 0) {
      std::reverse(regionBoundary.begin(), regionBoundary.end());
    }
    if (conventional) {
      std::reverse(regionBoundary.begin(), regionBoundary.end());
    }
    std::string error;
    if (!build_spiral_family(regionBoundary, /*startDist=*/0.0, spacing,
                             dSign, /*roundJoins=*/true,
                             /*hardFailFirst=*/false, region.family,
                             error)) {
      // Unreachable: loop 0 is pushed directly and cannot fail.
      result.ok = false;
      result.error_message = error;
      return result;
    }
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
  // The retract plane is WCS-relative (origin Z + retract height).
  const double retractZ =
      cam_planning::setup_retract_plane_z(context.setup);
  // Guard 1: rapid-safety (see face milling — same contract).
  if (retractZ < faceZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the face height (" << faceZ
            << " mm) — rapids travel through the material. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  // Guard 2 (multi-pass): rapids must clear the unmilled stock above
  // the first level, not just the face.
  if (levels.size() > 1 && haveStockTop && retractZ < stockTopZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the stock top (" << stockTopZ
            << " mm) — rapids travel through unmilled stock. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  if (op.parameters.strategy.has_value() &&
      op.parameters.strategy.value() != "adaptive") {
    result.warnings.push_back(
        "Only the adaptive strategy is implemented in v1 — generating "
        "the adaptive spiral.");
  }

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

  // One machinable piece of a loop segment (clipping may split a
  // segment into several).
  struct RowPiece {
    XY start;
    XY end;
  };

  // One spiral loop, clipped: `clipInside` (optional CCW polygon)
  // keeps the segments inside it (an avoidance family must not reach
  // into the wall band), then every `clipAgainst` polygon removes the
  // segments passing through another avoidance's clearance.  The loop
  // is already in climb walk order.  Consecutive surviving segments
  // chain into runs, each with its own rapid-plunge-feed-rapid cycle
  // (the pocket finishing-contour contract).
  const auto append_spiral_loop = [&](
      const std::vector<XY>& loop, double z,
      const std::vector<const std::vector<XY>*>& clipAgainst,
      const std::vector<XY>* clipInside) {
    bool chainOpen = false;
    XY chainEnd{0.0, 0.0};
    const auto closeChain = [&]() {
      if (chainOpen) {
        append_rapid_xy(chainEnd, retractZ);
        chainOpen = false;
      }
    };
    for (size_t i = 0; i < loop.size(); ++i) {
      const XY a = loop[i];
      const XY b = loop[(i + 1) % loop.size()];
      std::vector<RowPiece> current{{a, b}};
      if (clipInside != nullptr) {
        std::vector<RowPiece> inside;
        for (const auto& piece : current) {
          const auto clipped =
              clip_segment_to_polygon(piece.start, piece.end, *clipInside);
          for (size_t k = 0; k + 1 < clipped.size(); k += 2) {
            inside.push_back({clipped[k], clipped[k + 1]});
          }
        }
        current = std::move(inside);
      }
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

  // ── Emission ──────────────────────────────────────────────────
  // Per level: the outer boundary family first (loop 0 rides the rEff
  // inset — the wall finishing pass — no separate finish contours),
  // then each active avoidance family.  Clipping forbids entering a
  // grown avoidance clearance or the wall band; overlap between
  // families at the same Z is allowed (harmless double-cutting).
  for (const double levelZ : levels) {
    std::vector<const std::vector<XY>*> active;
    for (const auto& region : avoidance) {
      if (region.alwaysActive || region.topZ > levelZ + 1e-6) {
        active.push_back(&region.clipLoop);
      }
    }
    for (const auto& loop : outerFamily) {
      append_spiral_loop(loop, levelZ, active, nullptr);
    }
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
      for (const auto& loop : region.family) {
        append_spiral_loop(loop, levelZ, others, &insetPoints);
      }
    }
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No machinable loops were produced.";
    return result;
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace
