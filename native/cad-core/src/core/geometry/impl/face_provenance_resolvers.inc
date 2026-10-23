// ── Face-based sketch plane provenance ────────────────────────────
//
// A sketch placed on a body face stores "<body>:face:<index>" and the
// refresh walk re-resolves that index directly — a topology change that
// shifts face indices silently re-binds the sketch to a different face.
// `SketchFeatureParameters::face_fingerprint` captures the face's plane
// (origin + unit normal) at placement time; these helpers re-resolve the
// face by that fingerprint before the index, mirroring the CAM face
// resolver's 95% ambiguity rule.

namespace {

inline constexpr double kFaceOriginMaxDistance = 5.0;
inline constexpr double kFaceNormalMaxAngle = 0.2618;  // 15 degrees
inline constexpr double kFaceScoreThreshold = 0.7;

struct FaceProvenanceResolution {
  int index = -1;     // 0-based resolved index into the live face map
  bool ambiguous = false;
  bool missing = false;
};

FaceProvenanceResolution resolve_face_fingerprint(
    const SketchFeatureParameters::FaceFingerprint& fingerprint,
    const TopoDS_Shape& body_shape) {
  FaceProvenanceResolution result;
  if (body_shape.IsNull() || !fingerprint.valid) {
    result.missing = true;
    return result;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(body_shape, TopAbs_FACE, face_map);

  std::vector<std::pair<int, double>> candidates;
  for (int i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(face_map(i));
    if (face.IsNull()) {
      continue;
    }
    const std::optional<PlaneFrame> frame = frame_from_planar_face(face);
    if (!frame.has_value()) {
      continue;
    }

    const double dx = frame->origin_x - fingerprint.origin_x;
    const double dy = frame->origin_y - fingerprint.origin_y;
    const double dz = frame->origin_z - fingerprint.origin_z;
    const double origin_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double origin_score =
        std::max(0.0, 1.0 - origin_distance / kFaceOriginMaxDistance);

    const double dot = frame->normal_x * fingerprint.normal_x +
                       frame->normal_y * fingerprint.normal_y +
                       frame->normal_z * fingerprint.normal_z;
    const double clamped = std::max(-1.0, std::min(1.0, dot));
    const double angle = std::acos(clamped);
    const double normal_score =
        1.0 - std::min(1.0, angle / kFaceNormalMaxAngle);

    const double score = 0.5 * origin_score + 0.5 * normal_score;
    if (score >= kFaceScoreThreshold) {
      candidates.emplace_back(i - 1, score);
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
              return a.second > b.second;
            });

  if (candidates.empty()) {
    result.missing = true;
    return result;
  }
  result.index = candidates[0].first;
  if (candidates.size() > 1 &&
      candidates[1].second >= candidates[0].second * 0.95) {
    result.ambiguous = true;
  }
  return result;
}

}  // namespace
