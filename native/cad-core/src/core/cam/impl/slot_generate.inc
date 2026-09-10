// ── Open Slot generator ──────────────────────────────────────────
//
// Each resolved machining-region edge is a slot's OPEN side: the tool
// cuts a tool-width groove from the edge INTO the material, on the
// side of the adjacent horizontal top face (the side the face's
// interior lies on — re-derived from the LIVE body at generate time,
// TNP-safe; the attestation stays positional only).  The groove floor
// sits `slot.depth_mm` below that face's Z; a Stepdown set cuts
// multi-pass levels from the stock top down to the floor.
//
// Per edge:
//   * the live edge must be a straight line (full circles and arcs
//     are rejected — a slot edge is an open side, not a rim);
//   * the adjacent horizontal face with the steepest upward normal
//     (>= kMaxUpwardFaceTilt) is the machining face; its centre Z is
//     the top;
//   * inward = the face-interior direction at the edge midpoint
//     (face COM probe projected onto the edge's perpendicular);
//   * the tool CENTRE path runs at edge + tool_radius × inward, so
//     the groove spans from the edge to a tool diameter into the
//     material;
//   * climb walks with the material (the inward side) on the LEFT:
//     W = (inward.y, −inward.x); "conventional" flips W.
//
// Emission is CUT-major: each edge's levels are contiguous
// (rapid-plunge-feed-rapid per pass), edges in region order.  Levels
// are planned per cut (plan_stepdown_levels), so edges with different
// top faces each get their own pass set.
//
// v1 exclusions: centerline and two-wall semantics, closed slots, arc
// edges, and slot widths beyond the tool diameter (multi-offset
// widening is a future refinement).

namespace {

// One resolved slot edge turned into a toolpath cut.
struct SlotCut {
  XY start;              // tool centre at the walk start
  XY end;                // tool centre at the walk end
  double topZ = 0.0;     // the adjacent machining face height
  std::vector<double> levels;  // descending cut levels
};

CamGenerateResult generate_slot_toolpath(
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

  if (context.geometry.edges.empty()) {
    result.ok = false;
    result.error_message =
        "Slot requires at least one selected edge (geometry changed — "
        "re-select it).";
    return result;
  }

  // An absent slot block falls back to the struct defaults so old
  // documents still generate.
  const SlotParameters slotParams =
      op.parameters.slot.has_value() ? op.parameters.slot.value()
                                     : SlotParameters{};
  if (slotParams.depth_mm <= 0.0) {
    result.ok = false;
    result.error_message = "The slot depth must be positive.";
    return result;
  }

  // cutting_direction: "conventional" flips the walk; "mixed" and
  // unknown values warn and behave as climb (the 2D contour contract).
  bool conventional = false;
  if (op.parameters.cutting_direction == "conventional") {
    conventional = true;
  } else if (op.parameters.cutting_direction == "mixed") {
    result.warnings.push_back(
        "Cutting direction 'mixed' is treated as climb for a slot.");
  } else if (op.parameters.cutting_direction != "climb") {
    result.warnings.push_back(
        "Unknown cutting direction '" + op.parameters.cutting_direction +
        "' — treated as climb.");
  }

  const double toolRadius = context.tool.diameter_mm / 2.0;
  const bool multiPass = op.parameters.stepdown_mm.has_value() &&
                         op.parameters.stepdown_mm.value() > 1e-9;
  double stockTopZ = 0.0;
  bool haveStockTop = false;
  if (multiPass) {
    haveStockTop = cam_stock::stock_top_z(context.document,
                                          context.setup.stock, stockTopZ);
  }
  const double retractZ =
      cam_planning::setup_retract_plane_z(context.setup);

  // ── Per-edge resolution ────────────────────────────────────────
  std::vector<SlotCut> cuts;
  double maxTopZ = -1e9;
  for (const auto& resolvedEdge : context.geometry.edges) {
    // Re-open the live edge from the resolved body's edge map (the
    // drilling pattern — never trust stale indices).
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
    TopExp::MapShapes(resolvedEdge.body->shape, TopAbs_EDGE, edgeMap);
    if (resolvedEdge.edgeIndex < 0 ||
        resolvedEdge.edgeIndex >= edgeMap.Extent()) {
      result.ok = false;
      result.error_message =
          "The referenced edge no longer exists (geometry changed — "
          "re-select it).";
      return result;
    }
    const TopoDS_Edge edge = TopoDS::Edge(edgeMap(resolvedEdge.edgeIndex + 1));

    try {
      BRepAdaptor_Curve curve(edge);
      if (curve.GetType() != GeomAbs_Line) {
        result.ok = false;
        result.error_message =
            "Slot requires straight line edges (a selected edge is not "
            "a line — re-select a straight edge).";
        return result;
      }
      TopoDS_Vertex firstVertex;
      TopoDS_Vertex lastVertex;
      TopExp::Vertices(edge, firstVertex, lastVertex, /*CumOri=*/true);
      gp_Pnt p0 = BRep_Tool::Pnt(firstVertex);
      gp_Pnt p1 = BRep_Tool::Pnt(lastVertex);
      const double edgeDx = p1.X() - p0.X();
      const double edgeDy = p1.Y() - p0.Y();
      const double xyLength = std::sqrt(edgeDx * edgeDx + edgeDy * edgeDy);
      if (xyLength < 1e-9) {
        result.ok = false;
        result.error_message =
            "The selected edge is vertical — slot edges must lie "
            "horizontally.";
        return result;
      }
      const double ex = edgeDx / xyLength;
      const double ey = edgeDy / xyLength;
      const double midX = 0.5 * (p0.X() + p1.X());
      const double midY = 0.5 * (p0.Y() + p1.Y());

      // Top-face find (the classify_inner_wire map pattern): the
      // adjacent face with the steepest UPWARD orientation-corrected
      // normal is the machining face.
      NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                 TopTools_ShapeMapHasher>
          edgeFaces;
      TopExp::MapShapesAndAncestors(resolvedEdge.body->shape, TopAbs_EDGE,
                                    TopAbs_FACE, edgeFaces);
      if (!edgeFaces.Contains(edge)) {
        result.ok = false;
        result.error_message =
            "The referenced edge no longer exists (geometry changed — "
            "re-select it).";
        return result;
      }
      TopoDS_Face topFace;
      gp_Pnt topFaceCenter;
      bool haveTopFace = false;
      double bestNormalZ = -1.0;
      for (const auto& ancestor : edgeFaces.FindFromKey(edge)) {
        const TopoDS_Face candidateFace = TopoDS::Face(ancestor);
        gp_Pnt center;
        gp_Vec normal;
        if (!cam_planning::face_cut_plane(candidateFace, center, normal)) {
          continue;
        }
        normal.Normalize();
        // Orientation-correct the normal (the raw surface
        // parameterization gives +Z for both box caps).
        if (candidateFace.Orientation() == TopAbs_REVERSED) {
          normal.Reverse();
        }
        if (normal.Z() >= cam_planning::kMaxUpwardFaceTilt &&
            normal.Z() > bestNormalZ) {
          bestNormalZ = normal.Z();
          topFace = candidateFace;
          topFaceCenter = center;
          haveTopFace = true;
        }
      }
      if (!haveTopFace) {
        result.ok = false;
        result.error_message =
            "Slot requires a horizontal face adjacent to the selected "
            "edge.";
        return result;
      }
      const double topZ = topFaceCenter.Z();

      // Inward probe: the machining face's interior as seen from the
      // edge midpoint (COM probe, mid-UV fallback — the pocket's
      // classify_inner_wire pattern).
      gp_Pnt probe = topFaceCenter;
      try {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(topFace, props);
        probe = props.CentreOfMass();
      } catch (const Standard_Failure&) {
        // Keep the mid-UV fallback.
      }
      const double vx = probe.X() - midX;
      const double vy = probe.Y() - midY;
      // Strip the edge-parallel component — the inward direction is
      // the face-interior perpendicular of the edge.
      const double along = vx * ex + vy * ey;
      const double inwardRawX = vx - ex * along;
      const double inwardRawY = vy - ey * along;
      const double inwardLen = std::sqrt(inwardRawX * inwardRawX +
                                         inwardRawY * inwardRawY);
      if (inwardLen < 1e-9) {
        result.ok = false;
        result.error_message =
            "The slot's inward direction could not be determined (the "
            "selected edge crosses the face centre).";
        return result;
      }
      const double inwardX = inwardRawX / inwardLen;
      const double inwardY = inwardRawY / inwardLen;

      // Walk direction: climb keeps the material (the inward side) on
      // the toolpath's LEFT — left(W) = (−W.y, W.x) == inward means
      // W = (inward.y, −inward.x).  Conventional flips.
      double wx = inwardY;
      double wy = -inwardX;
      if (conventional) {
        wx = -wx;
        wy = -wy;
      }
      if (edgeDx * wx + edgeDy * wy < 0.0) {
        std::swap(p0, p1);
      }

      SlotCut cut;
      cut.topZ = topZ;
      cut.start = XY{p0.X() + toolRadius * inwardX,
                     p0.Y() + toolRadius * inwardY};
      cut.end = XY{p1.X() + toolRadius * inwardX,
                   p1.Y() + toolRadius * inwardY};
      const double bottomZ = topZ - slotParams.depth_mm;
      if (multiPass) {
        cam_planning::plan_stepdown_levels(
            stockTopZ, bottomZ, op.parameters.stepdown_mm.value(),
            /*extra_levels=*/{}, cut.levels, result.warnings);
      } else {
        cut.levels = {bottomZ};
      }
      maxTopZ = std::max(maxTopZ, topZ);
      cuts.push_back(std::move(cut));
    } catch (const std::exception&) {
      result.ok = false;
      result.error_message =
          "A selected edge could not be read (geometry changed — "
          "re-select it).";
      return result;
    }
  }

  // ── Guards ────────────────────────────────────────────────────
  if (retractZ < maxTopZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the face height (" << maxTopZ
            << " mm) — rapids travel through the material. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  if (multiPass && haveStockTop && retractZ < stockTopZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the stock top (" << stockTopZ
            << " mm) — rapids travel through unmilled stock. Raise it "
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

  // ── Emission (cut-major — each edge's levels contiguous) ───────
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

  for (const auto& cut : cuts) {
    for (const double levelZ : cut.levels) {
      append_rapid_xy(cut.start, retractZ);
      append_feed_xy(cut.start, levelZ, plungeFeed);
      append_feed_xy(cut.end, levelZ, feedrate);
      append_rapid_xy(cut.end, retractZ);
    }
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No slot moves were produced.";
    return result;
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace
