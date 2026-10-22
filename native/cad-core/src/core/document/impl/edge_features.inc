namespace {

// Capture a geometric fingerprint for each edge at selection time so the
// dependency refresh can re-resolve by geometry before falling back to the
// index. A failed capture (edge gone / unsupported curve) produces an empty
// `EdgeGeometry` (empty `kind`), which the resolver treats as "no fingerprint".
//
// Picked edge indices always reference the PRE-modifier body (the viewport
// picks against `body_pick_shapes`, and `apply_fillet`/`apply_chamfer` resolve
// against the pre-op shape). So when re-capturing during a pending update we
// compile the document with the in-flight modifier removed — otherwise
// `compute_edge_geometry` would resolve the index against the already-filleted
// body and store a fingerprint for the wrong edge.
std::vector<EdgeGeometry> capture_edge_fingerprints(
    const DocumentState& document,
    const std::vector<std::string>& edge_ids,
    const std::string& exclude_feature_id) {
  DocumentState source = document;
  if (!exclude_feature_id.empty()) {
    source.feature_history.erase(
        std::remove_if(source.feature_history.begin(),
                       source.feature_history.end(),
                       [&](const FeatureEntry& feature) {
                         return feature.id == exclude_feature_id;
                       }),
        source.feature_history.end());
  }
  std::vector<EdgeGeometry> fingerprints;
  fingerprints.reserve(edge_ids.size());
  for (const auto& edge_id : edge_ids) {
    const auto geometry = compute_edge_geometry(source, edge_id);
    fingerprints.push_back(geometry.value_or(EdgeGeometry{}));
  }
  return fingerprints;
}

}  // namespace

#include "core/document/impl/fillet_commands.inc"
#include "core/document/impl/chamfer_commands.inc"
#include "core/document/impl/shell_commands.inc"
