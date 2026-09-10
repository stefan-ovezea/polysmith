// ── Edge resolution ───────────────────────────────────────────────
//
// Scores live edges against a captured witness.  Circle references
// (drilling rims) score by radius proximity blended with 3D center
// distance — the center Z is what disambiguates the top vs bottom rim
// of one bore.  Line references have no v1 consumer: capture accepts
// them so the capture command stays generic, but nothing re-resolves
// them, so they score NotFound.

namespace {

// Score a candidate edge against a circle witness.  Returns 0.0 (no
// match) to 1.0 (perfect match).
double score_edge_candidate(const CamEdgeReference& reference,
                            const TopoDS_Edge& edge) {
  try {
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Circle) {
      return 0.0;
    }
    const gp_Circ circle = curve.Circle();
    const gp_Pnt center = circle.Location();
    const double radius = circle.Radius();

    // ── Radius match (weight: 0.5) ────────────────────────────
    const double refRadius = reference.radius.value();
    const double radiusScore =
        (refRadius > 0.0 && radius > 0.0)
            ? std::min(radius, refRadius) / std::max(radius, refRadius)
            : 0.0;

    // ── Center distance (weight: 0.5) ─────────────────────────
    // The center Z is what separates the top rim from the bottom rim
    // of one bore — both share xy and radius.
    const double dx = center.X() - reference.center.value()[0];
    const double dy = center.Y() - reference.center.value()[1];
    const double dz = center.Z() - reference.center.value()[2];
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double centerScore =
        std::max(0.0, 1.0 - distance / kEdgeMaxCenterDistance);

    return 0.5 * radiusScore + 0.5 * centerScore;
  } catch (const std::exception&) {
    return 0.0;
  }
}

}  // namespace

EdgeResolutionResult resolve_edge_reference(
    const CamEdgeReference& reference,
    const TopoDS_Shape& bodyShape) {
  EdgeResolutionResult result;

  if (bodyShape.IsNull()) {
    return result;
  }

  // Only circle witnesses are resolvable in v1.
  if (!reference.center.has_value() || !reference.axis.has_value() ||
      !reference.radius.has_value()) {
    return result;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
  TopExp::MapShapes(bodyShape, TopAbs_EDGE, edgeMap);

  // Score every edge.
  for (int i = 1; i <= edgeMap.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edgeMap(i));
    if (edge.IsNull()) continue;

    const double score = score_edge_candidate(reference, edge);
    if (score >= kScoreThreshold) {
      result.candidates.push_back(
          ResolvedEdge{.edgeIndex = i - 1, .score = score});
    }
  }

  // Sort by score descending.
  std::sort(result.candidates.begin(), result.candidates.end(),
            [](const ResolvedEdge& a, const ResolvedEdge& b) {
              return a.score > b.score;
            });

  // Determine outcome — the same 95% ambiguity rule as face resolution.
  if (result.candidates.empty()) {
    result.outcome = EdgeResolutionOutcome::NotFound;
  } else if (result.candidates.size() == 1) {
    result.outcome = EdgeResolutionOutcome::Found;
  } else {
    const double best = result.candidates[0].score;
    const double second = result.candidates[1].score;
    if (second >= best * 0.95) {
      result.outcome = EdgeResolutionOutcome::Ambiguous;
    } else {
      // Clear margin — keep only the winner.
      result.candidates.resize(1);
      result.outcome = EdgeResolutionOutcome::Found;
    }
  }

  return result;
}

EdgeResolutionResult resolve_edge_reference(
    const CamEdgeReference& reference,
    const DocumentState& document) {
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.id == reference.bodyId && !body.shape.IsNull()) {
      return resolve_edge_reference(reference, body.shape);
    }
  }
  return EdgeResolutionResult{};
}
