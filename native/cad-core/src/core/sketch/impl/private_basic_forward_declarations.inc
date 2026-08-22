constexpr double kMinimumSketchDimensionValue = 0.001;
constexpr double kCoincidentTolerance = 0.01;

// Forward declaration: re-anchors every midpoint-bound point to its
// host line's current midpoint. Defined further down (depends on
// `propagate_connected_point_move`); called from
// `refresh_sketch_derived_state` so every public mutator picks up
// the enforcement automatically.
void enforce_midpoint_anchors(SketchFeatureParameters& parameters);
void enforce_point_line_anchors(SketchFeatureParameters& parameters);
void enforce_line_HV_constraints(SketchFeatureParameters& parameters);
void enforce_all_equal_length_relations(SketchFeatureParameters& parameters);
void enforce_all_perpendicular_relations(SketchFeatureParameters& parameters);
void enforce_all_parallel_relations(SketchFeatureParameters& parameters);
void enforce_equal_length_relations(SketchFeatureParameters& parameters,
                                    const std::string& seed_line_id);
void enforce_perpendicular_relations(SketchFeatureParameters& parameters,
                                     const std::string& seed_line_id);
void enforce_parallel_relations(SketchFeatureParameters& parameters,
                                const std::string& seed_line_id);
// Slide each tangent-bound line's end point onto the closer of the
// two tangent points from its start to the host circle. Stored as a
// `SketchLineRelation` of kind "tangent_line_circle" with
// `first_line_id` = line id and `second_line_id` = circle id. Run
// from `refresh_sketch_derived_state` so changes to either the line
// start, the circle center, or the radius all keep the relation
// satisfied without explicit re-driving.
void enforce_tangent_line_circle_relations(SketchFeatureParameters& parameters);
// Deterministic (solver-free) enforcement of driving arc_radius /
// arc_angle dimensions — see dimension_update.inc.  Runs after the
// solver and the line→arc endpoint rescue so dragged endpoints first
// move the arc, then the dimension value is re-imposed.
void enforce_arc_dimensions(SketchFeatureParameters& parameters);
// Ad-hoc enforcement for the constraint-completion kinds (symmetric,
// midpoint, collinear, tangent pairs) — see
// constraint_completion_enforcement.inc.  Runs after the solver as a
// deterministic fallback.
void enforce_constraint_completion(SketchFeatureParameters& parameters);
void sync_driven_dimensions(SketchFeatureParameters& parameters);
