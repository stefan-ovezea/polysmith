FaceResolutionResult resolve_face_reference(
    const CamFaceReference& reference,
    const TopoDS_Shape& bodyShape) {
  FaceResolutionResult result;

  if (bodyShape.IsNull()) {
    return result;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
  TopExp::MapShapes(bodyShape, TopAbs_FACE, faceMap);

  // Score every face.
  for (int i = 1; i <= faceMap.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(faceMap(i));
    if (face.IsNull()) continue;

    const double score = score_candidate(reference, face);
    if (score >= kScoreThreshold) {
      result.candidates.push_back(
          ResolvedFace{.faceIndex = i - 1, .score = score});
    }
  }

  // Sort by score descending.
  std::sort(result.candidates.begin(), result.candidates.end(),
            [](const ResolvedFace& a, const ResolvedFace& b) {
              return a.score > b.score;
            });

  // Determine outcome.
  if (result.candidates.empty()) {
    result.outcome = FaceResolutionOutcome::NotFound;
  } else if (result.candidates.size() == 1) {
    result.outcome = FaceResolutionOutcome::Found;
  } else {
    // Check if the top two candidates are close enough to be ambiguous.
    // If the second-best is within 5% of the best, treat as ambiguous.
    const double best = result.candidates[0].score;
    const double second = result.candidates[1].score;
    if (second >= best * 0.95) {
      result.outcome = FaceResolutionOutcome::Ambiguous;
    } else {
      // Clear margin — keep only the winner.
      result.candidates.resize(1);
      result.outcome = FaceResolutionOutcome::Found;
    }
  }

  return result;
}

FaceResolutionResult resolve_face_reference(
    const CamFaceReference& reference,
    const DocumentState& document) {
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.id == reference.bodyId && !body.shape.IsNull()) {
      return resolve_face_reference(reference, body.shape);
    }
  }
  return FaceResolutionResult{};
}
