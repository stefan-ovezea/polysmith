// ── Laser kerf offset: sketch-region → base segments ─────────────
//
// Builds the exact base loop for a sketch profile region (exact
// boundary edges where possible, sampled-point fallback otherwise).
// The offset itself lives in offset_loop.inc, shared with the
// face-milling inset.

namespace {

// Builds exact base segments from the region's boundary_edges.  When
// any edge is an ellipse or spline (no exact offset in v1), falls back
// to the sampled polygon.  Returns false for the fallback.
bool build_base_segments_from_edges(const SketchProfileRegion& region,
                                    std::vector<BaseSegment>& out) {
  out.clear();
  for (const auto& edge : region.boundary_edges) {
    if (edge.entity_kind == "ellipse" || edge.entity_kind == "spline") {
      return false;
    }
    BaseSegment segment;
    segment.start = {edge.start_x, edge.start_y};
    segment.end = {edge.end_x, edge.end_y};
    if (edge.entity_kind == "circle" || edge.entity_kind == "arc") {
      segment.is_arc = true;
      segment.center = {edge.center_x, edge.center_y};
      segment.radius = edge.radius;
      segment.ccw = edge.ccw;
    }
    out.push_back(segment);
  }
  return !out.empty();
}

// Sampled-polygon fallback (legacy profiles without exact edges, or
// edges we cannot offset exactly).  Assumes the points follow the
// walk orientation.
void build_base_segments_from_points(
    const std::vector<SketchProfilePoint>& points,
    std::vector<BaseSegment>& out) {
  out.clear();
  if (points.size() < 2) {
    return;
  }
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1) % points.size()];
    BaseSegment segment;
    segment.start = {a.x, a.y};
    segment.end = {b.x, b.y};
    out.push_back(segment);
  }
}

}  // namespace
