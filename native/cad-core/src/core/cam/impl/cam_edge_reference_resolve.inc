// ── Edge resolution ───────────────────────────────────────────────
//
// Scores live edges against a captured witness.  Circle references
// (drilling rims) score by radius proximity blended with 3D center
// distance — the center Z is what disambiguates the top vs bottom rim
// of one bore.  Line references (open slot edges) score by endpoint
// proximity blended with length and direction — the endpoint pairing
// is orientation-agnostic, so a live edge that reversed between
// recomputes still scores.

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

// Score a candidate edge against a LINE witness.  Returns 0.0 (no
// match) to 1.0 (perfect match).
double score_line_edge_candidate(const CamEdgeReference& reference,
                                 const TopoDS_Edge& edge) {
  try {
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
      return 0.0;
    }
    TopoDS_Vertex firstVertex;
    TopoDS_Vertex lastVertex;
    TopExp::Vertices(edge, firstVertex, lastVertex, /*CumOri=*/true);
    const gp_Pnt p0 = BRep_Tool::Pnt(firstVertex);
    const gp_Pnt p1 = BRep_Tool::Pnt(lastVertex);

    // ── Endpoint match (weight: 0.5) ─────────────────────────
    // The live edge may have reversed orientation since capture, so
    // try both start/end pairings and take the better one.
    const auto endpointDistance = [&](const gp_Pnt& a,
                                      const std::array<double, 3>& b) {
      const double dx = a.X() - b[0];
      const double dy = a.Y() - b[1];
      const double dz = a.Z() - b[2];
      return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    const double dForward = endpointDistance(p0, reference.startPoint) +
                            endpointDistance(p1, reference.endPoint);
    const double dReverse = endpointDistance(p0, reference.endPoint) +
                            endpointDistance(p1, reference.startPoint);
    const double endpointScore =
        std::max(0.0, 1.0 - std::min(dForward, dReverse) /
                                kEdgeMaxEndpointDistance);

    // ── Length match (weight: 0.25) ──────────────────────────
    const double dx = p1.X() - p0.X();
    const double dy = p1.Y() - p0.Y();
    const double dz = p1.Z() - p0.Z();
    const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double lengthScore =
        (reference.length > 0.0 && length > 0.0)
            ? std::min(length, reference.length) /
                  std::max(length, reference.length)
            : 0.0;

    // ── Direction match (weight: 0.25) ───────────────────────
    // Absolute dot — a reversed edge is still the same slot side.
    const double dirDot =
        (length > 1e-9)
            ? std::abs((dx / length) * reference.tangent[0] +
                       (dy / length) * reference.tangent[1] +
                       (dz / length) * reference.tangent[2])
            : 0.0;

    return 0.5 * endpointScore + 0.25 * lengthScore + 0.25 * dirDot;
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

  // Circle witnesses resolve against circle edges; anything else is a
  // line witness (capture only ever stores those two kinds).
  const bool circleWitness =
      reference.center.has_value() && reference.axis.has_value() &&
      reference.radius.has_value();

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
  TopExp::MapShapes(bodyShape, TopAbs_EDGE, edgeMap);

  // Score every edge.
  for (int i = 1; i <= edgeMap.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edgeMap(i));
    if (edge.IsNull()) continue;

    const double score = circleWitness
                             ? score_edge_candidate(reference, edge)
                             : score_line_edge_candidate(reference, edge);
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
