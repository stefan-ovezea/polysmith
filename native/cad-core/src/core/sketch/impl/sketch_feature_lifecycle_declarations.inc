FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame = std::nullopt);
// Re-run the post-edit sketch recompute pipeline (anchors, fillets,
// rebuild_sketch_vertices, projected-point lock, profile detection).
// Most public mutators in sketch_feature.cpp call this internally; the
// Project tool's `project_vertex_into_sketch` path needs to call it
// directly because it appends to `projected_points` *after* every
// mutator has already returned.
void refresh_sketch_derived_state(FeatureEntry& feature);
// Refresh with append-mode freeze. The solver freezes all existing
// geometry and only adjusts entities referenced by `focus_ids`.
// Use when adding new geometry or constraints to an already-constrained
// sketch to prevent ghost movements of unrelated elements.
void refresh_sketch_with_append_focus(FeatureEntry& feature,
                                       const std::vector<std::string>& focus_ids);
void set_sketch_tool(FeatureEntry& feature, const std::string& tool);
// Look up the current world-space position of a sketch point by
// searching lines first (authoritative for endpoints), then the
// points list (centers, quadrants, fillet corners, projected points).
std::optional<std::tuple<double, double>> find_point_position(
    const SketchFeatureParameters& parameters,
    const std::string& point_id);
