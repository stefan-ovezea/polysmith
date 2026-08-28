// ── Laser cutting generator ───────────────────────────────────────
//
// Sketch profile → world curves → kerf offset → lead-in/out + pierce
// → cut ordering (holes before outer, small parts first) → toolpath
// IR.  Hard cases fail loudly, never silently.

namespace {

struct PlannedLoop {
  std::vector<OffsetSegment> segments;
  XY centroid;
  double area = 0.0;
  bool is_hole = false;
  bool skipped = false;
  // Face-derived loops already live in WORLD XY (sampled from the
  // body's wire geometry); sketch loops live in sketch-local 2D and
  // map through the sketch plane frame.
  bool isWorldXY = false;
  double worldZ = 0.0;  // cut-plane Z for world-XY loops
};

// ── Face wire sampling (laser from 3D geometry) ──────────────────
//
// Samples every wire of a planar face into a closed world-XY loop.
// Returns false when the sampling produced fewer than 3 points.
bool sample_face_wire_to_loop(const TopoDS_Wire& wire,
                              std::vector<XY>& loop) {
  // Sample every edge of the wire, then chain the pieces into a
  // connected loop by endpoint matching (edge orientations inside the
  // wire vary, so pieces may arrive reversed — match-and-flip handles
  // both).
  std::vector<std::vector<XY>> pieces;
  BRepTools_WireExplorer walker(wire);
  for (; walker.More(); walker.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(walker.Current());
    if (edge.IsNull()) {
      continue;
    }
    BRepAdaptor_Curve curve(edge);
    const double t0 = curve.FirstParameter();
    const double t1 = curve.LastParameter();
    const int samples = 30;
    std::vector<XY> piece;
    // k <= samples so each piece ends exactly ON the shared vertex —
    // consecutive pieces chain with zero gap.
    for (int k = 0; k <= samples; ++k) {
      const double t = t0 + (t1 - t0) * static_cast<double>(k) / samples;
      gp_Pnt pt;
      curve.D0(t, pt);
      piece.push_back({pt.X(), pt.Y()});
    }
    if (!piece.empty()) {
      pieces.push_back(std::move(piece));
    }
  }
  loop.clear();
  if (pieces.empty()) {
    return false;
  }
  loop = pieces.front();
  std::vector<bool> used(pieces.size(), false);
  used[0] = true;
  for (size_t remaining = 1; remaining < pieces.size(); ++remaining) {
    bool chained = false;
    for (size_t j = 0; j < pieces.size(); ++j) {
      if (used[j]) {
        continue;
      }
      const XY& last = loop.back();
      const auto& piece = pieces[j];
      if (xy_length(piece.front().x - last.x,
                    piece.front().y - last.y) < 0.001) {
        loop.insert(loop.end(), piece.begin() + 1, piece.end());
        used[j] = true;
        chained = true;
        break;
      }
      if (xy_length(piece.back().x - last.x,
                    piece.back().y - last.y) < 0.001) {
        // Piece arrives reversed — append it flipped.
        for (size_t k = piece.size() - 1; k > 0; --k) {
          loop.push_back(piece[k - 1]);
        }
        used[j] = true;
        chained = true;
        break;
      }
    }
    if (!chained) {
      return false;
    }
  }
  // Drop the repeated closing point.
  if (loop.size() > 1 &&
      xy_length(loop.front().x - loop.back().x,
                loop.front().y - loop.back().y) < 0.001) {
    loop.pop_back();
  }
  return loop.size() >= 3;
}

// World point of a sketch-local 2D point on the cut plane (the sketch
// plane offset along its normal).
XY world_point(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const XY& p, double cut_plane_offset) {
  return XY{frame.origin_x + frame.x_axis_x * p.x + frame.y_axis_x * p.y,
            frame.origin_y + frame.x_axis_y * p.x + frame.y_axis_y * p.y};
}

double world_z(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const XY& p, double cut_plane_offset) {
  return frame.origin_z + frame.x_axis_z * p.x + frame.y_axis_z * p.y +
         frame.normal_z * cut_plane_offset;
}

// Sketch frames: cached plane_frame when present, else the hardcoded
// origin-plane frame table (sketches on origin planes may not cache a
// frame — the history dependency pass resolves it on demand).
std::optional<SketchFeatureParameters::SketchPlaneFrame> resolve_sketch_frame(
    const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    return sketch.plane_frame;
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 1.0, .y_axis_z = 0.0,
        .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
    };
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 0.0, .x_axis_y = 1.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 0.0, .y_axis_z = 1.0,
        .normal_x = 1.0, .normal_y = 0.0, .normal_z = 0.0,
    };
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 0.0, .y_axis_z = 1.0,
        .normal_x = 0.0, .normal_y = 1.0, .normal_z = 0.0,
    };
  }
  return std::nullopt;
}

// Builds and offsets one loop (outer or hole) into a PlannedLoop.
// Returns false with a human message on hard failure.
bool plan_loop(const std::vector<BaseSegment>& base, double kerf,
               bool is_hole, const XY& centroid, PlannedLoop& out,
               std::string& error) {
  out.is_hole = is_hole;
  out.area = std::abs(base_segments_signed_area(base));
  out.centroid = centroid;
  if (!offset_closed_loop(base, kerf, out.segments)) {
    error = "A profile contour could not be offset (check the kerf width).";
    return false;
  }
  const auto samples = sample_offset_loop(out.segments, /*tolerance=*/0.05);
  if (offset_loop_self_intersects(samples)) {
    error = std::string("A ") + (is_hole ? "hole" : "profile") +
            " contour self-intersects after the kerf offset — the feature "
            "is narrower than the kerf width; enlarge it or reduce the kerf.";
    return false;
  }
  return true;
}

CamGenerateResult generate_laser_cut_toolpath(
    const CamGenerateContext& context) {
  CamGenerateResult result;
  const auto& op = context.operation;

  if (!op.parameters.laser.has_value()) {
    result.ok = false;
    result.error_message =
        "The laser operation is missing its laser parameters.";
    return result;
  }
  const auto& laser = op.parameters.laser.value();
  if (context.geometry.profiles.empty() &&
      context.geometry.faces.empty()) {
    result.ok = false;
    result.error_message =
        "Laser cut requires sketch profiles or a selected face.";
    return result;
  }
  // v1: every selected profile must belong to the same sketch plane.
  std::optional<SketchFeatureParameters::SketchPlaneFrame> frame;
  if (!context.geometry.profiles.empty()) {
    const SketchFeatureParameters* owningSketch =
        context.geometry.sketches.empty() ? nullptr
                                          : context.geometry.sketches[0];
    if (owningSketch == nullptr) {
      result.ok = false;
      result.error_message =
          "The sketch plane could not be resolved.";
      return result;
    }
    for (const auto* sketch : context.geometry.sketches) {
      if (sketch != owningSketch) {
        result.ok = false;
        result.error_message =
            "All laser profiles must lie on the same sketch plane.";
        return result;
      }
    }
    frame = resolve_sketch_frame(*owningSketch);
    if (!frame.has_value()) {
      result.ok = false;
      result.error_message = "The sketch plane could not be resolved.";
      return result;
    }
  }
  const bool engrave = laser.mode == "engrave";
  // Kerf compensation applies to cuts that separate material; engraving
  // marks the surface and follows the exact contour.
  const double kerf = engrave ? 0.0 : laser.kerf_width_mm / 2.0;
  const double feedrate = op.parameters.feedrate_mm_per_min > 0.0
                              ? op.parameters.feedrate_mm_per_min
                              : context.tool.default_feedrate_mm_per_min;

  // ── Plan loops: per selected region, holes first, then the outer ─
  std::vector<std::vector<PlannedLoop>> groups;
  for (size_t r = 0; r < context.geometry.profiles.size(); ++r) {
    const auto& region = *context.geometry.profiles[r].region;
    std::vector<PlannedLoop> group;

    // Outer loop.
    std::vector<BaseSegment> base;
    const bool exact = build_base_segments_from_edges(region, base);
    if (!exact) {
      result.warnings.push_back(
          "Profile contour is tessellated (no exact arcs).");
      build_base_segments_from_points(region.points, base);
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
    if (base.size() >= 2 ||
        (base.size() == 1 && base[0].is_arc)) {
      if (base.size() >= 2 && base_segments_signed_area(base) < 0) {
        reverse_segments(base);  // normalize: material on the left (CCW)
      }
      std::vector<XY> basePoints;
      for (const auto& segment : base) {
        basePoints.push_back(segment.start);
      }
      // Circles report their center as the pierce anchor centroid.
      const XY centroid =
          base.size() == 1 && base[0].is_arc
              ? base[0].center
              : xy_centroid(basePoints);

      PlannedLoop loop;
      std::string error;
      if (!plan_loop(base, kerf, /*is_hole=*/false, centroid, loop, error)) {
        result.ok = false;
        result.error_message = error;
        return result;
      }
      group.push_back(std::move(loop));
    }

    // Hole loops.
    for (const auto& holePoints : region.inner_loops) {
      std::vector<BaseSegment> holeBase;
      build_base_segments_from_points(holePoints, holeBase);
      if (holeBase.size() < 2) {
        continue;
      }
      if (base_segments_signed_area(holeBase) > 0) {
        reverse_segments(holeBase);  // normalize: hole interior on the right
      }
      std::vector<XY> holePointList;
      for (const auto& segment : holeBase) {
        holePointList.push_back(segment.start);
      }
      PlannedLoop loop;
      std::string error;
      if (!plan_loop(holeBase, kerf, /*is_hole=*/true,
                     xy_centroid(holePointList), loop, error)) {
        result.ok = false;
        result.error_message = error;
        return result;
      }
      group.push_back(std::move(loop));
    }

    if (!group.empty()) {
      groups.push_back(std::move(group));
    }
  }

  // ── Face-derived loops (laser from 3D geometry) ────────────────
  // A selected planar face cuts its boundary outline: the outer wire
  // plus every hole wire, projected at the face height.
  if (!context.geometry.faces.empty()) {
    for (const auto& faceRef : context.geometry.faces) {
      NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
      TopExp::MapShapes(faceRef.body->shape, TopAbs_FACE, faceMap);
      if (faceRef.faceIndex < 0 || faceRef.faceIndex >= faceMap.Extent()) {
        result.ok = false;
        result.error_message =
            "The referenced face no longer exists (geometry changed — "
            "re-select it).";
        return result;
      }
      const TopoDS_Face face = TopoDS::Face(faceMap(faceRef.faceIndex + 1));

      std::vector<std::vector<XY>> wireLoops;
      for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
           wireExp.Next()) {
        std::vector<XY> loop;
        if (sample_face_wire_to_loop(TopoDS::Wire(wireExp.Current()), loop)) {
          wireLoops.push_back(std::move(loop));
        }
      }
      if (wireLoops.empty()) {
        result.ok = false;
        result.error_message = "The face boundary could not be sampled.";
        return result;
      }
      // Cut plane: the face's own height (planar face) plus the offset.
      double faceZ = 0.0;
      {
        BRepAdaptor_Surface surface(face);
        const double uMid =
            0.5 * (surface.FirstUParameter() + surface.LastUParameter());
        const double vMid =
            0.5 * (surface.FirstVParameter() + surface.LastVParameter());
        gp_Pnt center;
        surface.D0(uMid, vMid, center);
        faceZ = center.Z();
      }

      // The largest loop is the outer boundary; the rest are holes.
      std::sort(wireLoops.begin(), wireLoops.end(),
                [](const auto& a, const auto& b) {
                  return std::abs(xy_signed_area(a)) >
                         std::abs(xy_signed_area(b));
                });

      std::vector<PlannedLoop> faceGroup;
      for (size_t w = 0; w < wireLoops.size(); ++w) {
        const bool isHole = w > 0;
        std::vector<BaseSegment> base;
        for (size_t i = 0; i < wireLoops[w].size(); ++i) {
          const auto& a = wireLoops[w][i];
          const auto& b = wireLoops[w][(i + 1) % wireLoops[w].size()];
          BaseSegment segment;
          segment.start = a;
          segment.end = b;
          base.push_back(segment);
        }
        // Normalize: outer CCW (material left), holes CW.
        const double area = base_segments_signed_area(base);
        if ((!isHole && area < 0) || (isHole && area > 0)) {
          reverse_segments(base);
        }
        PlannedLoop loop;
        std::string error;
        if (!plan_loop(base, kerf, isHole, xy_centroid(wireLoops[w]), loop,
                       error)) {
          result.ok = false;
          result.error_message = error;
          return result;
        }
        loop.isWorldXY = true;
        loop.worldZ = faceZ + laser.cut_plane_offset_mm;
        faceGroup.push_back(std::move(loop));
      }
      if (!faceGroup.empty()) {
        groups.push_back(std::move(faceGroup));
      }
    }
  }

  if (groups.empty()) {
    result.ok = false;
    result.error_message = "No cuttable profile loops were found.";
    return result;
  }

  // ── Warn once when a selected region duplicates a hole loop ────
  // (Nested-but-separate profiles are legitimately distinct parts;
  // only geometry that coincides with another region's inner loop is
  // genuinely cut twice.)
  {
    int duplicates = 0;
    for (size_t a = 0; a < groups.size(); ++a) {
      for (size_t b = 0; b < groups.size(); ++b) {
        if (a == b) {
          continue;
        }
        const auto& regionB = *context.geometry.profiles[b].region;
        for (const auto& holeLoop : regionB.inner_loops) {
          double cx = 0.0;
          double cy = 0.0;
          for (const auto& point : holeLoop) {
            cx += point.x;
            cy += point.y;
          }
          if (holeLoop.empty()) {
            continue;
          }
          cx /= holeLoop.size();
          cy /= holeLoop.size();
          double radius = 0.0;
          for (const auto& point : holeLoop) {
            radius += xy_length(point.x - cx, point.y - cy);
          }
          radius /= holeLoop.size();
          const double distance =
              xy_length(groups[a].front().centroid.x - cx,
                        groups[a].front().centroid.y - cy);
          if (distance < 0.05 + 0.02 * radius &&
              std::abs(groups[a].front().area /
                           (3.14159265358979323846 * radius) - radius) <
                  0.05 + 0.02 * radius) {
            ++duplicates;
            break;
          }
        }
      }
    }
    if (duplicates > 0) {
      result.warnings.push_back(
          std::to_string(duplicates) +
          " selected profile(s) duplicate holes of other regions — they may "
          "be cut twice.");
    }
  }

  // ── Order: regions by outer area ascending; holes before outer ──
  // (The outer loop is the FIRST element of each group; holes follow.)
  std::sort(groups.begin(), groups.end(),
            [](const auto& a, const auto& b) {
              return a.front().area < b.front().area;
            });

  std::vector<PlannedLoop> ordered;
  for (auto& group : groups) {
    PlannedLoop outer = std::move(group.front());
    std::vector<PlannedLoop> holes;
    for (size_t i = 1; i < group.size(); ++i) {
      holes.push_back(std::move(group[i]));
    }
    std::sort(holes.begin(), holes.end(),
              [](const auto& a, const auto& b) { return a.area < b.area; });
    for (auto& hole : holes) {
      ordered.push_back(std::move(hole));
    }
    ordered.push_back(std::move(outer));
  }

  // ── Emit moves ──────────────────────────────────────────────────
  Toolpath toolpath;
  toolpath.op_id = op.op_id;
  const double cut_plane_offset = laser.cut_plane_offset_mm;

  const double leadIn = engrave ? 0.0 : laser.lead_in_mm;
  const double leadOut = engrave ? 0.0 : laser.lead_out_mm;
  const double dwell = engrave ? 0.0 : laser.pierce_dwell_seconds;

  int skippedTooShort = 0;
  for (auto& loop : ordered) {
    // Loop-coordinate → world mapping: sketch loops go through the
    // sketch plane frame; face loops are already world XY at a fixed
    // cut-plane height.
    // Sketch loops only exist when a sketch frame resolved above.
    const auto toWorld = [&](const XY& p) {
      return loop.isWorldXY ? XY{p.x, p.y}
                            : world_point(frame.value(), p, cut_plane_offset);
    };
    const auto toZ = [&](const XY& p) {
      return loop.isWorldXY ? loop.worldZ
                            : world_z(frame.value(), p, cut_plane_offset);
    };
    const auto append_linear = [&](const XY& end, bool laserOn, double power,
                                   double dwellSeconds) {
      const XY w = toWorld(end);
      ToolpathMove move;
      move.kind = ToolpathMoveKind::FeedLinear;
      move.x = w.x;
      move.y = w.y;
      move.z = toZ(end);
      move.feedrate_mm_per_min = feedrate;
      move.power_percent = power;
      move.laser_on = laserOn;
      move.dwell_seconds = dwellSeconds;
      toolpath.moves.push_back(move);
    };
    const auto append_rapid = [&](const XY& target) {
      const XY w = toWorld(target);
      ToolpathMove move;
      move.kind = ToolpathMoveKind::Rapid;
      move.x = w.x;
      move.y = w.y;
      move.z = toZ(target);
      move.feedrate_mm_per_min = 0.0;
      move.power_percent = 0.0;
      move.laser_on = false;
      toolpath.moves.push_back(move);
    };

    const auto samples = sample_offset_loop(loop.segments, /*tolerance=*/0.05);
    if (samples.size() < 3) {
      continue;
    }
    const double length = offset_loop_length(loop.segments);
    if (!engrave && length < 2.0 * (leadIn + leadOut)) {
      ++skippedTooShort;
      continue;
    }

    // Pierce at the sampled vertex nearest the loop centroid.
    size_t pierceIndex = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (size_t i = 0; i < samples.size(); ++i) {
      const double d = xy_length(samples[i].x - loop.centroid.x,
                                 samples[i].y - loop.centroid.y);
      if (d < bestDistance) {
        bestDistance = d;
        pierceIndex = i;
      }
    }
    const XY pierce = samples[pierceIndex];
    const XY dirOut{
        samples[(pierceIndex + 1) % samples.size()].x - pierce.x,
        samples[(pierceIndex + 1) % samples.size()].y - pierce.y};
    const XY dirIn{
        pierce.x - samples[(pierceIndex - 1 + samples.size()) % samples.size()].x,
        pierce.y - samples[(pierceIndex - 1 + samples.size()) % samples.size()].y};
    const double outLen = xy_length(dirOut.x, dirOut.y);
    const double inLen = xy_length(dirIn.x, dirIn.y);
    const XY outUnit = outLen > kOffsetEps ? XY{dirOut.x / outLen, dirOut.y / outLen}
                                           : XY{1.0, 0.0};
    const XY inUnit = inLen > kOffsetEps ? XY{dirIn.x / inLen, dirIn.y / inLen}
                                         : XY{1.0, 0.0};

    const XY leadInStart{pierce.x - outUnit.x * leadIn,
                         pierce.y - outUnit.y * leadIn};
    const XY leadOutEnd{pierce.x + inUnit.x * leadOut,
                        pierce.y + inUnit.y * leadOut};

    // Rapid to the pierce point (laser off).
    append_rapid(leadInStart);
    // Pierce in place: laser on, dwell.
    append_linear(leadInStart, /*laserOn=*/true, laser.power_percent, dwell);
    // Lead-in cut.
    if (leadIn > 0.0) {
      append_linear(pierce, /*laserOn=*/true, laser.power_percent,
                    /*dwell=*/0.0);
    }

    // Contour, starting at the segment whose start lies at the pierce
    // vertex.
    size_t startSegment = 0;
    double closest = std::numeric_limits<double>::max();
    for (size_t s = 0; s < loop.segments.size(); ++s) {
      const double d = xy_length(loop.segments[s].start.x - pierce.x,
                                 loop.segments[s].start.y - pierce.y);
      if (d < closest) {
        closest = d;
        startSegment = s;
      }
    }
    const size_t count = loop.segments.size();
    for (size_t k = 0; k < count; ++k) {
      const auto& segment = loop.segments[(startSegment + k) % count];
      const XY wEnd = toWorld(segment.end);
      ToolpathMove move;
      move.feedrate_mm_per_min = feedrate;
      move.power_percent = laser.power_percent;
      move.laser_on = true;
      move.dwell_seconds = 0.0;
      move.z = toZ(segment.end);
      move.x = wEnd.x;
      move.y = wEnd.y;
      if (segment.is_arc) {
        move.kind = segment.cw ? ToolpathMoveKind::FeedArcCW
                               : ToolpathMoveKind::FeedArcCCW;
        // IR convention: i/j = center offset from the arc START.
        move.i = segment.center.x - segment.start.x;
        move.j = segment.center.y - segment.start.y;
      } else {
        move.kind = ToolpathMoveKind::FeedLinear;
      }
      toolpath.moves.push_back(move);
    }

    // Lead-out cut.
    if (leadOut > 0.0) {
      append_linear(leadOutEnd, /*laserOn=*/true, laser.power_percent,
                    /*dwell=*/0.0);
    }
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No cuttable contours were produced.";
    return result;
  }

  if (skippedTooShort > 0) {
    result.warnings.push_back(
        std::to_string(skippedTooShort) +
        " contour(s) are shorter than the lead allowance and were skipped "
        "(reduce lead-in/lead-out to cut them).");
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace
