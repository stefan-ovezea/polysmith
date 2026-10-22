// ── Body-modifier edge re-resolution (fillet / chamfer) ──────────
//
// The fillet/chamfer path stores a naked "<body>:edge:<index>" and
// `apply_fillet` / `apply_chamfer` resolve it by index alone — a topology
// change that shifts indices silently re-binds to a DIFFERENT edge. These
// helpers re-resolve each stored edge by its captured geometric fingerprint
// first and the index last, so the dependency refresh can flag an uncertain
// re-bind (`dependency_ambiguous`) instead of guessing silently. Mirrors the
// CAM edge resolver's scoring and 95% ambiguity rule
// (`core/cam/impl/cam_edge_reference_resolve.inc`).

namespace {

inline constexpr double kFilletScoreThreshold = 0.7;
inline constexpr double kFilletMaxCenterDistance = 5.0;
inline constexpr double kFilletMaxEndpointDistance = 5.0;

int parse_edge_index(const std::string& edge_id) {
  const std::string marker = ":edge:";
  const auto pos = edge_id.rfind(marker);
  if (pos == std::string::npos) {
    return -1;
  }
  try {
    size_t consumed = 0;
    const std::string suffix = edge_id.substr(pos + marker.size());
    const int parsed = std::stoi(suffix, &consumed);
    return (consumed == suffix.size() && parsed >= 0) ? parsed : -1;
  } catch (const std::exception&) {
    return -1;
  }
}

// Extract the same EdgeGeometry description `compute_edge_geometry` produces,
// but from a raw OCCT edge so the refresh walk can score live edges without
// going through a document.
EdgeGeometry edge_geometry_of_edge(const TopoDS_Edge& edge) {
  EdgeGeometry result{};
  TopoDS_Vertex first_vertex;
  TopoDS_Vertex last_vertex;
  TopExp::Vertices(edge, first_vertex, last_vertex, /*CumOri=*/true);
  if (first_vertex.IsNull() || last_vertex.IsNull()) {
    result.kind = "unsupported";
    return result;
  }
  const gp_Pnt start = BRep_Tool::Pnt(first_vertex);
  const gp_Pnt end = BRep_Tool::Pnt(last_vertex);
  result.start = EdgePoint{start.X(), start.Y(), start.Z()};
  result.end = EdgePoint{end.X(), end.Y(), end.Z()};

  try {
    BRepAdaptor_Curve curve(edge);
    const GeomAbs_CurveType curve_type = curve.GetType();
    if (curve_type == GeomAbs_Line) {
      result.kind = "line";
      return result;
    }
    if (curve_type == GeomAbs_Circle) {
      const gp_Circ circle = curve.Circle();
      const gp_Pnt center = circle.Location();
      const gp_Dir axis = circle.Axis().Direction();
      result.center = EdgePoint{center.X(), center.Y(), center.Z()};
      result.axis = EdgePoint{axis.X(), axis.Y(), axis.Z()};
      result.radius = circle.Radius();
      const double dx = end.X() - start.X();
      const double dy = end.Y() - start.Y();
      const double dz = end.Z() - start.Z();
      const double endpoint_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
      const double tolerance = std::max(1e-7, result.radius * 1e-7);
      result.kind = endpoint_distance <= tolerance ? "circle" : "arc";
      return result;
    }
  } catch (const std::exception&) {
    // Fall through to "unsupported".
  }

  result.kind = "unsupported";
  return result;
}

double edge_point_distance(const EdgePoint& a, const EdgePoint& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  const double dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Score a live edge's geometry against a captured fingerprint, in [0, 1].
// Returns -1 when the fingerprint carries no usable geometry (empty or
// "unsupported") so the caller falls back to the stored index.
double score_edge_fingerprint(const EdgeGeometry& fp, const EdgeGeometry& live) {
  if (fp.kind.empty() || fp.kind == "unsupported") {
    return -1.0;
  }

  if (fp.kind == "line") {
    if (live.kind != "line") {
      return 0.0;
    }
    const double forward = edge_point_distance(live.start, fp.start) +
                           edge_point_distance(live.end, fp.end);
    const double reverse = edge_point_distance(live.start, fp.end) +
                           edge_point_distance(live.end, fp.start);
    return std::max(0.0, 1.0 - std::min(forward, reverse) /
                                   kFilletMaxEndpointDistance);
  }

  if (fp.kind == "circle" || fp.kind == "arc") {
    if (live.kind != "circle" && live.kind != "arc") {
      return 0.0;
    }
    const double radius_score =
        (fp.radius > 0.0 && live.radius > 0.0)
            ? std::min(fp.radius, live.radius) / std::max(fp.radius, live.radius)
            : 0.0;
    const double center_score =
        std::max(0.0, 1.0 - edge_point_distance(live.center, fp.center) /
                               kFilletMaxCenterDistance);
    if (fp.kind == "circle") {
      return 0.5 * radius_score + 0.5 * center_score;
    }
    // Arc: the endpoints also matter (which portion of the circle).
    const double forward = edge_point_distance(live.start, fp.start) +
                           edge_point_distance(live.end, fp.end);
    const double reverse = edge_point_distance(live.start, fp.end) +
                           edge_point_distance(live.end, fp.start);
    const double endpoint_score =
        std::max(0.0, 1.0 - std::min(forward, reverse) /
                               kFilletMaxEndpointDistance);
    return 0.4 * radius_score + 0.3 * center_score + 0.3 * endpoint_score;
  }

  return 0.0;
}

struct FilletEdgeResolution {
  int index = -1;     // 0-based resolved index into the live edge map
  bool ambiguous = false;
  bool missing = false;
};

// Resolve one stored edge reference (fingerprint + stored index) against a
// live body shape.
FilletEdgeResolution resolve_body_edge(const EdgeGeometry& fingerprint,
                                       int stored_index,
                                       const TopoDS_Shape& body_shape) {
  FilletEdgeResolution result;
  if (body_shape.IsNull()) {
    result.missing = true;
    return result;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  // No usable fingerprint → index-only fallback (legacy behaviour).
  if (fingerprint.kind.empty() || fingerprint.kind == "unsupported") {
    const int one_based = stored_index + 1;
    if (one_based < 1 || one_based > edge_map.Extent()) {
      result.missing = true;
    } else {
      result.index = stored_index;
    }
    return result;
  }

  std::vector<std::pair<int, double>> candidates;
  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (edge.IsNull()) {
      continue;
    }
    const double score =
        score_edge_fingerprint(fingerprint, edge_geometry_of_edge(edge));
    if (score >= kFilletScoreThreshold) {
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

struct EdgeRefreshSummary {
  bool any_broken = false;
  bool any_ambiguous = false;
  int ambiguous_count = 0;
};

// Re-resolve every edge of a fillet/chamfer feature against the body shape
// compiled from the prefix (features before `feature_index`). Rewrites
// `edge_ids` in place when a clear or ambiguous fingerprint match lands on a
// different index, and reports broken/ambiguous so the caller can set the
// feature flags.
EdgeRefreshSummary refresh_modifier_edges(
    const DocumentState& document,
    size_t feature_index,
    const std::string& target_body_id,
    std::vector<std::string>& edge_ids,
    std::vector<EdgeGeometry>& edge_fingerprints) {
  EdgeRefreshSummary summary;

  DocumentState prefix = document;
  prefix.feature_history.resize(feature_index);
  const CompiledBodies compiled = compile_bodies(prefix, /*include_meshes=*/false);
  const TopoDS_Shape* target_shape = nullptr;
  for (const auto& body : compiled.bodies) {
    if (body.id == target_body_id && !body.shape.IsNull()) {
      target_shape = &body.shape;
      break;
    }
  }

  for (size_t j = 0; j < edge_ids.size(); ++j) {
    const int stored_index = parse_edge_index(edge_ids[j]);
    if (stored_index < 0) {
      summary.any_broken = true;
      continue;
    }
    const EdgeGeometry fingerprint =
        j < edge_fingerprints.size() ? edge_fingerprints[j] : EdgeGeometry{};
    const FilletEdgeResolution resolved =
        target_shape != nullptr
            ? resolve_body_edge(fingerprint, stored_index, *target_shape)
            : FilletEdgeResolution{};

    if (target_shape == nullptr || resolved.missing) {
      summary.any_broken = true;
    } else if (resolved.ambiguous) {
      summary.any_ambiguous = true;
      ++summary.ambiguous_count;
      if (resolved.index != stored_index) {
        edge_ids[j] =
            target_body_id + ":edge:" + std::to_string(resolved.index);
      }
    } else if (resolved.index != stored_index) {
      edge_ids[j] =
          target_body_id + ":edge:" + std::to_string(resolved.index);
    }
  }
  return summary;
}

}  // namespace
