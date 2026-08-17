// ── Public API ────────────────────────────────────────────────────

std::optional<CamFaceReference> capture_face_reference(
    const std::string& bodyId,
    const TopoDS_Shape& bodyShape,
    int faceIndex,
    const std::string& label) {
  if (bodyShape.IsNull() || faceIndex < 0) {
    return std::nullopt;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
  TopExp::MapShapes(bodyShape, TopAbs_FACE, faceMap);
  const int oneBased = faceIndex + 1;
  if (oneBased < 1 || oneBased > faceMap.Extent()) {
    return std::nullopt;
  }

  const TopoDS_Face face = TopoDS::Face(faceMap(oneBased));
  if (face.IsNull()) {
    return std::nullopt;
  }

  // Sample points
  const auto worldPoints = sample_face_points(face);
  if (worldPoints.empty()) {
    return std::nullopt;
  }

  // Area
  const double area = compute_face_area(face);

  // Normal
  const auto normal = face_center_normal(face);
  if (!normal.has_value()) {
    return std::nullopt;
  }

  CamFaceReference ref;
  ref.bodyId = bodyId;
  ref.label = label;
  ref.capturedArea = area;

  ref.samplePoints.reserve(worldPoints.size());
  for (const auto& p : worldPoints) {
    ref.samplePoints.push_back({p.X(), p.Y(), p.Z()});
  }

  ref.capturedNormal = {normal->X(), normal->Y(), normal->Z()};

  return ref;
}
