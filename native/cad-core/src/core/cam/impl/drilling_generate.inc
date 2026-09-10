// ── Drilling generator ────────────────────────────────────────────
//
// Drills G81 (single plunge) or G83 (peck) holes at body geometry —
// cylindrical hole-wall faces and full-circle rim edges — plus
// free-picked world points.  Each hole is one Rapid to the retract
// plane over (x, y) followed by ONE DrillCycle move carrying the
// bottom Z, the R plane, and the peck depth — the post-processor
// decides between a canned cycle and longhand pecking from the
// selected post's capabilities.
//
// Face and edge references resolve through the witness machinery like
// every other reference, but the hole XY comes from the RESOLVED live
// geometry (the wall's cylinder axis or the rim's circle center) — a
// stale attestation center would drift when the body changes.  Point
// references are bare coordinates — their own identity, no resolution
// needed.  Sketch profiles are NOT drilling inputs: legacy operations
// that still reference them fail with a migration error instead of
// silently drilling nothing.

// The topmost upward-facing horizontal body face whose XY footprint
// contains (x, y).  A circle sketched on the plate's bottom face (or
// the XY plane the part extrudes from) must drill from the MATERIAL
// TOP — measuring the blind depth from the sketch plane would punch
// through the part.  Returns nullopt when nothing qualifies (no
// bodies, or the XY sits outside every top face); callers fall back
// to the sketch plane z.
std::optional<double> top_surface_z_at(double x, double y,
                                       const CompiledBodies& bodies) {
  std::optional<double> best;
  for (const auto& body : bodies.bodies) {
    for (TopExp_Explorer explorer(body.shape, TopAbs_FACE); explorer.More();
         explorer.Next()) {
      const TopoDS_Face face = TopoDS::Face(explorer.Current());
      const occ::handle<Geom_Plane> surface =
          occ::down_cast<Geom_Plane>(BRep_Tool::Surface(face));
      if (surface.IsNull()) {
        continue;  // cylinders, cones, splines — no single plane z
      }
      gp_Dir normal = surface->Pln().Axis().Direction();
      if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
      }
      // Upward-facing and horizontal within the same tilt the sketch
      // guard allows (drilling is vertical); downward faces are the
      // material bottom and never the drill start.
      if (normal.Z() < cam_planning::kMaxUpwardFaceTilt) {
        continue;
      }
      Bnd_Box box;
      BRepBndLib::Add(face, box);
      if (box.IsVoid()) {
        continue;
      }
      double xmin, ymin, zmin, xmax, ymax, zmax;
      box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
      constexpr double kFootprintTol = 1e-4;
      if (x < xmin - kFootprintTol || x > xmax + kFootprintTol ||
          y < ymin - kFootprintTol || y > ymax + kFootprintTol) {
        continue;
      }
      const double z = surface->Pln().Location().Z();
      if (!best.has_value() || z > *best) {
        best = z;
      }
    }
  }
  return best;
}

CamGenerateResult generate_drilling_toolpath(
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

  // Legacy migration guard — runs BEFORE hole collection so a document
  // saved with sketch-circle references never degrades into a silent
  // empty hole list.  Drilling no longer accepts sketch input; the
  // user must re-pick the holes on the body.
  if (!context.geometry.profiles.empty()) {
    result.ok = false;
    result.error_message =
        "This drilling operation still uses sketch circles, which are "
        "no longer drilling inputs. Re-pick the holes on the body "
        "(hole wall or rim edge).";
    return result;
  }

  // Cycle type: v1 supports G81 and G83 only.  Absent defaults to G81
  // (documents saved before the parameter existed).
  const std::string cycle =
      op.parameters.cycle_type.value_or("g81_standard");
  const bool peck = cycle == "g83_peck";
  if (cycle != "g81_standard" && !peck) {
    result.ok = false;
    result.error_message =
        "Drilling supports the G81 and G83 cycles only (got \"" + cycle +
        "\").";
    return result;
  }

  const bool through = op.parameters.through_hole;
  const double holeDepth = op.parameters.hole_depth_mm.value_or(0.0);
  if (!through && holeDepth <= 1e-9) {
    result.ok = false;
    result.error_message =
        "Drilling needs a positive hole depth (or the Through toggle).";
    return result;
  }
  if (peck && op.parameters.peck_depth_mm.value_or(0.0) <= 1e-9) {
    result.ok = false;
    result.error_message = "G83 peck drilling needs a positive peck depth.";
    return result;
  }
  const double peckDepth = op.parameters.peck_depth_mm.value_or(0.0);

  // The stock defines the through-hole bottom (and feeds the
  // retract-safety warnings).
  const auto extents =
      cam_stock::stock_box_extents(context.document, context.setup.stock);
  if (through && !extents.valid) {
    result.ok = false;
    result.error_message =
        "Through drilling needs a resolvable stock (its bottom is the "
        "hole target).";
    return result;
  }
  const double stockTopZ =
      extents.valid ? extents.center[2] + extents.size[2] / 2.0 : 0.0;
  const double stockBottomZ =
      extents.valid ? extents.center[2] - extents.size[2] / 2.0 : 0.0;

  // A drill is expected — an endmill in a drilling op is almost always
  // a mistake.  Warn, don't fail: flat-bottom endmills do drill.
  if (context.tool.type != "drill") {
    result.warnings.push_back(
        "This operation uses a " + context.tool.type +
        " tool, not a drill — check the tool library.");
  }

  // ── Collect holes ───────────────────────────────────────────────
  struct Hole {
    double x = 0.0;
    double y = 0.0;
    double start_z = 0.0;
  };
  std::vector<Hole> holes;

  // Hole-wall faces: the resolved wall's cylinder axis location gives
  // the hole XY.  Only cylindrical walls qualify — planar faces,
  // cones, and splines are not drilling inputs and fail with a clear
  // error (the user picked the wrong thing, silently skipping would
  // hide it).
  for (const auto& resolvedFace : context.geometry.faces) {
    TopoDS_Face face;
    std::string faceError;
    if (!cam_planning::map_face_index(resolvedFace.body->shape,
                                      resolvedFace.faceIndex, face,
                                      faceError)) {
      result.ok = false;
      result.error_message = faceError;
      return result;
    }
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Cylinder) {
      result.ok = false;
      result.error_message =
          "A selected face is not a cylindrical hole wall — drilling "
          "targets hole-wall faces, rim edges, or points (re-select on "
          "the body).";
      return result;
    }
    const gp_Cylinder cylinder = surface.Cylinder();
    const gp_Pnt location = cylinder.Location();
    const gp_Dir axis = cylinder.Axis().Direction();
    // A 3-axis machine drills vertically in v1 — a tilted or
    // horizontal hole wall cannot be drilled.
    if (std::abs(axis.Z()) < cam_planning::kMaxUpwardFaceTilt) {
      result.ok = false;
      result.error_message =
          "A selected hole wall is not vertical — 3-axis drilling "
          "targets vertical holes only.";
      return result;
    }
    const double x = location.X();
    const double y = location.Y();
    // Lift to the material top above the hole when one exists — the
    // wall spans the bore, so the axis location Z can sit below the
    // top face (blind depth measures from the material top).
    double startZ = location.Z();
    if (context.geometry.bodies != nullptr) {
      const auto top = top_surface_z_at(x, y, *context.geometry.bodies);
      if (top.has_value() && *top > startZ) {
        startZ = *top;
      }
    }
    holes.push_back(Hole{x, y, startZ});
  }

  // Hole-rim edges: a full-circle edge's circle center gives the hole
  // XY (the RESOLVED live edge — never the stale attestation center).
  // Arcs and lines are rejected with a clear error: a partial arc is
  // not a stable hole reference.
  for (const auto& resolvedEdge : context.geometry.edges) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
    TopExp::MapShapes(resolvedEdge.body->shape, TopAbs_EDGE, edgeMap);
    if (resolvedEdge.edgeIndex < 0 ||
        resolvedEdge.edgeIndex >= edgeMap.Extent()) {
      result.ok = false;
      result.error_message =
          "The referenced rim edge no longer exists (geometry changed — "
          "re-select it).";
      return result;
    }
    const TopoDS_Edge edge =
        TopoDS::Edge(edgeMap(resolvedEdge.edgeIndex + 1));
    try {
      BRepAdaptor_Curve curve(edge);
      if (curve.GetType() != GeomAbs_Circle) {
        result.ok = false;
        result.error_message =
            "A selected edge is not a circular rim — drilling targets "
            "full-circle rim edges only (re-select the closed rim).";
        return result;
      }
      // Full-circle test: a closed circular edge collapses both
      // endpoints to the same point (the OCCT full-circle marker).
      TopoDS_Vertex firstVertex;
      TopoDS_Vertex lastVertex;
      TopExp::Vertices(edge, firstVertex, lastVertex, /*CumOri=*/true);
      const gp_Pnt startPnt = BRep_Tool::Pnt(firstVertex);
      const gp_Pnt endPnt = BRep_Tool::Pnt(lastVertex);
      const double radius = curve.Circle().Radius();
      const double dx = endPnt.X() - startPnt.X();
      const double dy = endPnt.Y() - startPnt.Y();
      const double dz = endPnt.Z() - startPnt.Z();
      const double endpointDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
      if (endpointDistance > std::max(1e-7, radius * 1e-7)) {
        result.ok = false;
        result.error_message =
            "A selected edge is only an arc — drilling targets "
            "full-circle rim edges only (re-select the closed rim).";
        return result;
      }
      const gp_Pnt center = curve.Circle().Location();
      const gp_Dir axis = curve.Circle().Axis().Direction();
      if (std::abs(axis.Z()) < cam_planning::kMaxUpwardFaceTilt) {
        result.ok = false;
        result.error_message =
            "A selected rim edge is not horizontal — 3-axis drilling "
            "targets vertical holes only.";
        return result;
      }
      const double x = center.X();
      const double y = center.Y();
      // Same material-top lift as the wall case (the rim itself usually
      // IS the top surface, so the lift is a no-op there).
      double startZ = center.Z();
      if (context.geometry.bodies != nullptr) {
        const auto top = top_surface_z_at(x, y, *context.geometry.bodies);
        if (top.has_value() && *top > startZ) {
          startZ = *top;
        }
      }
      holes.push_back(Hole{x, y, startZ});
    } catch (const std::exception&) {
      result.ok = false;
      result.error_message =
          "A selected rim edge could not be read (geometry changed — "
          "re-select it).";
      return result;
    }
  }

  // Free-picked points: raw world coordinates (a coordinate is its own
  // identity — the TNP doctrine has no topology to protect).
  for (const auto& point : context.geometry.points) {
    holes.push_back(Hole{point.point[0], point.point[1], point.point[2]});
  }

  if (holes.empty()) {
    result.ok = false;
    result.error_message =
        "Drilling needs at least one hole face, rim edge, or point.";
    return result;
  }

  // ── Guards ─────────────────────────────────────────────────────
  // The retract plane is WCS-relative (origin Z + retract height).
  const double retractZ = cam_planning::setup_retract_plane_z(context.setup);
  double maxStartZ = holes[0].start_z;
  double minTargetZ = 1e18;
  for (const auto& hole : holes) {
    maxStartZ = std::max(maxStartZ, hole.start_z);
    minTargetZ = std::min(
        minTargetZ, through ? stockBottomZ : hole.start_z - holeDepth);
  }
  // Rapid-safety guards (face-milling precedent): a retract below the
  // highest hole start travels through the material between holes, and
  // a retract below the stock top travels through unmilled stock.
  if (retractZ < maxStartZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the highest hole start (" << maxStartZ
            << " mm) — rapids travel through the material. Raise it "
               "in the Setup.";
    result.warnings.push_back(message.str());
  }
  if (extents.valid && retractZ < stockTopZ) {
    std::ostringstream message;
    message << "Retract height (" << context.setup.retract_height
            << " mm above the setup origin) reaches " << retractZ
            << " mm — below the stock top (" << stockTopZ
            << " mm) — rapids travel through unmilled stock. Raise "
               "it in the Setup.";
    result.warnings.push_back(message.str());
  }
  // A blind hole that pierces below the stock bottom drills into the
  // bed — warn instead of failing (an operator may use a spoil board).
  if (!through && extents.valid && minTargetZ < stockBottomZ - 1e-6) {
    std::ostringstream message;
    message << "The hole depth reaches " << minTargetZ
            << " mm — below the stock bottom (" << stockBottomZ
            << " mm) — the drill breaks through the stock. Check the "
               "hole depth.";
    result.warnings.push_back(message.str());
  }

  const double plungeFeed =
      op.parameters.plunge_feedrate_mm_per_min > 0.0
          ? op.parameters.plunge_feedrate_mm_per_min
          : context.tool.default_plunge_feedrate_mm_per_min;

  // Nearest-neighbor greedy order (short XY hops between holes).
  std::vector<size_t> order(holes.size());
  std::iota(order.begin(), order.end(), 0);
  for (size_t i = 1; i < order.size(); ++i) {
    size_t best = i;
    double bestDist = 1e18;
    const auto& prev = holes[order[i - 1]];
    for (size_t j = i; j < order.size(); ++j) {
      const auto& candidate = holes[order[j]];
      const double dx = candidate.x - prev.x;
      const double dy = candidate.y - prev.y;
      const double d = dx * dx + dy * dy;
      if (d < bestDist) {
        bestDist = d;
        best = j;
      }
    }
    std::swap(order[i], order[best]);
  }

  // ── Emission ───────────────────────────────────────────────────
  Toolpath toolpath;
  toolpath.op_id = op.op_id;

  for (const size_t index : order) {
    const auto& hole = holes[index];
    ToolpathMove rapid;
    rapid.kind = ToolpathMoveKind::Rapid;
    rapid.x = hole.x;
    rapid.y = hole.y;
    rapid.z = retractZ;
    rapid.laser_on = false;
    toolpath.moves.push_back(rapid);

    ToolpathMove drill;
    drill.kind = ToolpathMoveKind::DrillCycle;
    drill.x = hole.x;
    drill.y = hole.y;
    drill.z = through ? stockBottomZ : hole.start_z - holeDepth;
    drill.feedrate_mm_per_min = plungeFeed;
    drill.laser_on = false;
    // R plane = retract height; peck depth rides the move so the post
    // can emit G83 Q words without re-deriving anything.
    drill.r_plane_z = retractZ;
    drill.peck_depth_mm = peck ? peckDepth : 0.0;
    toolpath.moves.push_back(drill);
  }

  result.toolpath = std::move(toolpath);
  return result;
}
