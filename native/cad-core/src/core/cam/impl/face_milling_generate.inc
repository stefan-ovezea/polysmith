// ── Face milling generator ────────────────────────────────────────
//
// Resolves the referenced 3D face through the witness machinery (the
// TNP stress path), requires a near-horizontal upward face, insets the
// boundary by the tool radius, and fills it with clipped zigzag rows.
// Rapids at the retract height, plunges at the plunge feed, rows at
// the cutting feed.

namespace {

// Sutherland–Hodgman clip of a segment against a CCW polygon.
std::vector<XY> clip_segment_to_polygon(XY p1, XY p2,
                                        const std::vector<XY>& poly) {
  std::vector<XY> output = {p1, p2};
  for (size_t i = 0; i < poly.size(); ++i) {
    if (output.empty()) {
      break;
    }
    std::vector<XY> input = std::move(output);
    output.clear();
    const XY& edgeStart = poly[i];
    const XY& edgeEnd = poly[(i + 1) % poly.size()];
    const double edgeX = edgeEnd.x - edgeStart.x;
    const double edgeY = edgeEnd.y - edgeStart.y;
    for (size_t j = 0; j < input.size(); ++j) {
      const XY& current = input[j];
      const XY& previous = input[(j + input.size() - 1) % input.size()];
      const double dCurrent = edgeX * (current.y - edgeStart.y) -
                              edgeY * (current.x - edgeStart.x);
      const double dPrevious = edgeX * (previous.y - edgeStart.y) -
                               edgeY * (previous.x - edgeStart.x);
      if (dCurrent >= 0) {
        if (dPrevious < 0) {
          const double t = dPrevious / (dPrevious - dCurrent);
          output.push_back({previous.x + t * (current.x - previous.x),
                            previous.y + t * (current.y - previous.y)});
        }
        output.push_back(current);
      } else if (dPrevious >= 0) {
        const double t = dPrevious / (dPrevious - dCurrent);
        output.push_back({previous.x + t * (current.x - previous.x),
                          previous.y + t * (current.y - previous.y)});
      }
    }
  }
  return output;
}

CamGenerateResult generate_face_milling_toolpath(
    const CamGenerateContext& context) {
  CamGenerateResult result;
  const auto& op = context.operation;

  if (context.geometry.faces.empty()) {
    result.ok = false;
    result.error_message =
        "Face milling requires a selected face (geometry changed — "
        "re-select it).";
    return result;
  }
  const auto& resolved = context.geometry.faces[0];

  // Resolve the live TopoDS_Face from the compiled body's face map.
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
  TopExp::MapShapes(resolved.body->shape, TopAbs_FACE, faceMap);
  if (resolved.faceIndex < 0 || resolved.faceIndex >= faceMap.Extent()) {
    result.ok = false;
    result.error_message =
        "The referenced face no longer exists (geometry changed — "
        "re-select it).";
    return result;
  }
  const TopoDS_Face face = TopoDS::Face(faceMap(resolved.faceIndex + 1));

  // Near-horizontal upward face only (v1: 2.5D).
  gp_Pnt center;
  gp_Vec d1u, d1v;
  BRepAdaptor_Surface surface(face);
  const double uMid =
      0.5 * (surface.FirstUParameter() + surface.LastUParameter());
  const double vMid =
      0.5 * (surface.FirstVParameter() + surface.LastVParameter());
  surface.D1(uMid, vMid, center, d1u, d1v);
  gp_Vec normal = d1u.Crossed(d1v);
  if (normal.Magnitude() < 1e-12) {
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
  if (normal.Z() < 0.9962) {  // within ~5° of +Z
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

  // Boundary polygon.  Sample every edge of the face's single wire,
  // then CHAIN the pieces into a connected loop by endpoint matching
  // (edge orientations inside the wire vary, so the pieces may arrive
  // reversed — match-and-flip handles both).
  std::vector<std::vector<XY>> pieces;
  for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
       wireExp.Next()) {
    const TopoDS_Wire& wire = TopoDS::Wire(wireExp.Current());
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
  }
  std::vector<XY> boundary;
  if (!pieces.empty()) {
    boundary = pieces.front();
    std::vector<bool> used(pieces.size(), false);
    used[0] = true;
    for (size_t remaining = 1; remaining < pieces.size(); ++remaining) {
      bool chained = false;
      for (size_t j = 0; j < pieces.size(); ++j) {
        if (used[j]) {
          continue;
        }
        const XY& last = boundary.back();
        const auto& piece = pieces[j];
        if (xy_length(piece.front().x - last.x,
                      piece.front().y - last.y) < 0.001) {
          boundary.insert(boundary.end(), piece.begin() + 1, piece.end());
          used[j] = true;
          chained = true;
          break;
        }
        if (xy_length(piece.back().x - last.x,
                      piece.back().y - last.y) < 0.001) {
          // Piece arrives reversed — append it flipped.
          for (size_t k = piece.size() - 1; k > 0; --k) {
            boundary.push_back(piece[k - 1]);
          }
          used[j] = true;
          chained = true;
          break;
        }
      }
      if (!chained) {
        break;  // disconnected piece — v1 faces are single wires, so
                // this means sampling failed; the size check below
                // reports it.
      }
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
  const auto point_segment_distance = [](const XY& p, const XY& a,
                                         const XY& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) {
      return xy_length(p.x - a.x, p.y - a.y);
    }
    const double t = std::max(
        0.0, std::min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2));
    return xy_length(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
  };
  bool insetValid = insetPoints.size() >= 3;
  for (const auto& p : insetPoints) {
    double minDist = 1e9;
    for (size_t i = 0; i < deduped.size(); ++i) {
      const auto& a = deduped[i];
      const auto& b = deduped[(i + 1) % deduped.size()];
      minDist = std::min(minDist, point_segment_distance(p, a, b));
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
  const double feedrate = op.parameters.feedrate_mm_per_min > 0.0
                              ? op.parameters.feedrate_mm_per_min
                              : context.tool.default_feedrate_mm_per_min;
  const double plungeFeed =
      op.parameters.plunge_feedrate_mm_per_min > 0.0
          ? op.parameters.plunge_feedrate_mm_per_min
          : context.tool.default_plunge_feedrate_mm_per_min;

  if (op.parameters.stepdown_mm.has_value() &&
      op.parameters.stepdown_mm.value() <
          std::abs(faceZ - context.setup.safety_height)) {
    result.warnings.push_back(
        "Multi-pass face milling is not supported yet — a single pass "
        "at the face height is generated.");
  }

  Toolpath toolpath;
  toolpath.op_id = op.op_id;

  const auto append_rapid_xy = [&](const XY& p, double z) {
    ToolpathMove move;
    move.kind = ToolpathMoveKind::Rapid;
    move.x = p.x;
    move.y = p.y;
    move.z = z;
    move.laser_on = true;
    toolpath.moves.push_back(move);
  };
  const auto append_feed_xy = [&](const XY& p, double z, double feed) {
    ToolpathMove move;
    move.kind = ToolpathMoveKind::FeedLinear;
    move.x = p.x;
    move.y = p.y;
    move.z = z;
    move.feedrate_mm_per_min = feed;
    move.laser_on = true;
    toolpath.moves.push_back(move);
  };

  for (int row = 0; row < rowCount; ++row) {
    const double n = minN + spacing * static_cast<double>(row);
    const XY anchor{rowNormal.x * n, rowNormal.y * n};
    XY a{anchor.x - rowDir.x * 10000.0, anchor.y - rowDir.y * 10000.0};
    XY b{anchor.x + rowDir.x * 10000.0, anchor.y + rowDir.y * 10000.0};
    const auto clipped = clip_segment_to_polygon(a, b, insetPoints);
    if (clipped.size() < 2) {
      continue;
    }
    XY rowStart = clipped[0];
    XY rowEnd = clipped.back();
    if (zigzag && row % 2 == 1) {
      std::swap(rowStart, rowEnd);
    }

    append_rapid_xy(rowStart, retractZ);
    append_feed_xy(rowStart, faceZ, plungeFeed);
    append_feed_xy(rowEnd, faceZ, feedrate);
    append_rapid_xy(rowEnd, retractZ);
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
