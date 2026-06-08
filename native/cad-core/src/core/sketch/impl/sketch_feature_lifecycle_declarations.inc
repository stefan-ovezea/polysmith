FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame = std::nullopt);
// Re-run the post-edit sketch recompute pipeline (anchors, fillets,
// rebuild_sketch_points, projected-point lock, profile detection).
// Most public mutators in sketch_feature.cpp call this internally; the
// Project tool's `project_vertex_into_sketch` path needs to call it
// directly because it appends to `projected_points` *after* every
// mutator has already returned.
void refresh_sketch_derived_state(FeatureEntry& feature);
void set_sketch_tool(FeatureEntry& feature, const std::string& tool);
// Look up the current world-space position of a sketch point by
// searching lines first (authoritative for endpoints), then the
// points list (centers, quadrants, fillet corners, projected points).
std::optional<std::tuple<double, double>> find_point_position(
    const SketchFeatureParameters& parameters,
    const std::string& point_id);
