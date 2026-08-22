// ---------------------------------------------------------------
// Sketch chamfer — parametric corner chamfer between two sketch lines.
//
// Bevel a corner shared by two `SketchLine`s by trimming each line
// back by its own distance and inserting a straight `SketchLine`
// between the two trim points. The relationship is recorded as a
// `SketchChamfer` on the sketch so the result stays parametric: the
// recompute pass re-derives the trim positions whenever the user
// moves either line, and `update_sketch_chamfer` rewrites the
// distances and re-runs the recompute. `delete_sketch_chamfer`
// restores the original corner. A corner already holding a fillet
// is rejected.
//
// Validation in `add_sketch_chamfer` is strict because a malformed
// chamfer would corrupt the lines:
//   - both lines must reference `corner_vertex_id` as one of their
//     endpoints
//   - the two lines must not be parallel (cross product within
//     `kCoincidentTolerance`)
//   - both distances must be positive and fit on their line
//   - the corner may not already be chamfered or filleted
// ---------------------------------------------------------------
void add_sketch_chamfer(FeatureEntry& feature,
                        int chamfer_index,
                        int trim_a_point_index,
                        int trim_b_point_index,
                        int line_index,
                        const std::string& corner_vertex_id,
                        const std::string& line_a_id,
                        const std::string& line_b_id,
                        double distance_a,
                        double distance_b);
void update_sketch_chamfer(FeatureEntry& feature,
                           const std::string& chamfer_id,
                           double distance_a,
                           double distance_b);
void delete_sketch_chamfer(FeatureEntry& feature,
                           const std::string& chamfer_id);
