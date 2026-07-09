void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y);
void update_sketch_vertex(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y);
void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint);
void set_sketch_equal_length_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_perpendicular_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_parallel_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
// Clear all constraints on a sketch line: both the inline H/V
// constraint and every line‑line relation (equal‑length,
// perpendicular, parallel, tangent) that involves this line.
// Dimensions and anchors are left untouched.
void clear_sketch_line_constraints(FeatureEntry& feature,
                                   const std::string& line_id);
void set_sketch_coincident_constraint(
    FeatureEntry& feature,
    const std::string& point_id,
    const std::string& other_point_id);
// Remove a coincident constraint identified by its constraint_id.
// Assigns fresh point IDs to all but the first line sharing the
// merged point so the entities can move independently again.
void delete_sketch_coincident_constraint(
    FeatureEntry& feature,
    const std::string& constraint_id);
void delete_sketch_mirror_relation(
    FeatureEntry& feature,
    const std::string& relation_id);
void set_sketch_vertex_fixed(FeatureEntry& feature,
                            const std::string& point_id,
                            bool is_fixed);
// Bind a sketch point to the midpoint of a host line. The point is
// pulled to the line's midpoint immediately and re-pulled on every
// subsequent edit (via `enforce_midpoint_anchors` inside the derived
// state refresh). Pass an empty `host_line_id` to remove an existing
// anchor for the point.
void set_sketch_midpoint_anchor(FeatureEntry& feature,
                                const std::string& point_id,
                                const std::string& host_line_id);
// Bind a sketch point to a host line's body at parametric position
// `t` in [0, 1]. The solver re-projects the bound point on every
// edit so it rides along with the host. Pass an empty
// `host_line_id` to remove an existing anchor for the point.
void set_sketch_vertex_line_anchor(FeatureEntry& feature,
                                  const std::string& point_id,
                                  const std::string& host_line_id,
                                  double t);

// Constrain a sketch line to be tangent to a circle. The relation
// is one-directional: the line's *end* point is driven onto the
// closer of the two tangent points from its start to the circle.
// Pass an empty `circle_id` to clear any existing tangent relation
// for the line. Throws when the line's start is inside or on the
// circle (no real tangent exists).
void set_sketch_tangent_constraint(FeatureEntry& feature,
                                   const std::string& line_id,
                                   const std::string& circle_id);
