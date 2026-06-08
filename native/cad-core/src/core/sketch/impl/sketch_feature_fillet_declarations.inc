// ---------------------------------------------------------------
// Sketch fillet — parametric corner fillet between two sketch lines.
//
// Round a corner shared by two `SketchLine`s by trimming each line
// back to its tangent point and inserting a tangent `SketchArc`
// between them. The relationship is recorded as a `SketchFillet` on
// the sketch so the result stays parametric: the recompute pass
// re-derives the trim positions whenever the user moves either
// line, and `update_sketch_fillet_radius` rewrites the radius and
// re-runs the recompute. `delete_sketch_fillet` restores the
// original corner.
//
// Validation in `add_sketch_fillet` is strict because a malformed
// fillet would corrupt the lines:
//   - both lines must reference `corner_point_id` as one of their
//     endpoints
//   - the two lines must not be parallel (cross product within
//     `kCoincidentTolerance`)
//   - the trim distance `r / tan(theta/2)` must fit on each line
//   - neither line may already be filleted at this corner
// ---------------------------------------------------------------
void add_sketch_fillet(FeatureEntry& feature,
                       int fillet_index,
                       int trim_a_point_index,
                       int trim_b_point_index,
                       int arc_index,
                       const std::string& corner_point_id,
                       const std::string& line_a_id,
                       const std::string& line_b_id,
                       double radius);
void update_sketch_fillet_radius(FeatureEntry& feature,
                                 const std::string& fillet_id,
                                 double radius);
void delete_sketch_fillet(FeatureEntry& feature,
                          const std::string& fillet_id);
