void update_sketch_circle(FeatureEntry& feature,
                          const std::string& circle_id,
                          double center_x,
                          double center_y,
                          double radius);
void add_sketch_line(FeatureEntry& feature,
                     int line_index,
                     double start_x,
                     double start_y,
                     double end_x,
                     double end_y,
                     bool is_construction = false,
                     bool snap_start = true,
                     // User-drawn lines get automatic driving length +
                     // angle dimensions. Projected geometry is derived
                     // (fixed endpoints) — its callers pass false so
                     // the solver isn't fed conflicting constraints.
                     bool auto_dimensions = true,
                     // Runs inference + the sketch derived-state
                     // refresh per entity. Bulk inserters (projection
                     // of outlines with many corners) pass false and
                     // refresh once at the end with the full id set.
                     bool refresh_immediately = true,
                     // Near-axis lines get a horizontal/vertical
                     // constraint hint applied, which snaps their
                     // coordinates exactly axis-aligned. Importers of
                     // arbitrary geometry (DXF) pass false — the
                     // constraint would deform the imported shape.
                     bool infer_constraints = true);
void add_sketch_rectangle(FeatureEntry& feature,
                          int& next_line_index,
                          double start_x,
                          double start_y,
                          double end_x,
                          double end_y,
                          bool is_construction = false);
// Toggle the construction-line flag on an existing line. Construction
// lines render dashed in the viewport and are filtered out of profile
// detection so they don't seal pickable faces / extrude sources.
void set_sketch_line_construction(FeatureEntry& feature,
                                  const std::string& line_id,
                                  bool is_construction);

void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius,
                       bool is_construction = false,
                       // Same semantics as add_sketch_line — projected
                       // circles pass false.
                       bool auto_dimensions = true,
                       bool refresh_immediately = true);

// Build a regular polygon on the sketch. mode is "circumscribed",
// "inscribed", or "edge". For circumscribed/inscribed the polygon is
// computed from (center, radius, sides). For edge mode the polygon is
// computed from (start, end, sides) and center/radius are derived.
void add_sketch_polygon(FeatureEntry& feature,
                        int polygon_index,
                        int sides,
                        const std::string& mode,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y,
                        bool is_construction = false);

// Build a SketchArc on the feature. Caller is expected to have already
// computed the arc's center, radius, and ccw direction from whichever
// creation flow they ran (three-point or center+start+end). The arc's
// endpoint points are added to the points list as kind="endpoint" and
// flagged is_fixed=true so v1 keeps the cached params authoritative.
// `start_point_index` / `end_point_index` are the trailing integers
// used to mint the synthesized point ids; they're sourced from the
// shared sketch line counter so arc endpoints can't collide with
// line endpoints.
void add_sketch_arc(FeatureEntry& feature,
                    int arc_index,
                    int start_point_index,
                    int end_point_index,
                    double start_x,
                    double start_y,
                    double end_x,
                    double end_y,
                    double center_x,
                    double center_y,
                    double radius,
                    bool ccw,
                    bool is_construction = false,
                    // Same semantics as add_sketch_line — projected
                    // arcs pass false.
                    bool auto_dimensions = true,
                    bool refresh_immediately = true);

// Full ellipse from three clicks: center, a major-axis point, and a
// minor-axis point (the minor radius is the perpendicular distance from
// the third click to the major axis).  v1 freezes the axis points —
// no solver registration yet.
void add_sketch_ellipse(FeatureEntry& feature,
                        int ellipse_index,
                        double center_x,
                        double center_y,
                        double axis_a_x,
                        double axis_a_y,
                        double axis_b_x,
                        double axis_b_y,
                        bool is_construction = false,
                        bool refresh_immediately = true);

// Straight slot (stadium): parametric record expanded by
// refresh_sketch_slots into 2 lines + 2 arcs (generated_by).
void add_sketch_slot(FeatureEntry& feature,
                     int slot_index,
                     double center_x,
                     double center_y,
                     double length,
                     double radius,
                     double rotation,
                     bool is_construction = false);

// Strips previously generated "slot:" entities and re-expands every
// slot record into 2 lines + 2 arcs. Called right after
// refresh_sketch_texts at the top of refresh_sketch_derived_state.
void refresh_sketch_slots(SketchFeatureParameters& parameters);

void update_sketch_slot(FeatureEntry& feature,
                        const std::string& slot_id,
                        double center_x,
                        double center_y,
                        double length,
                        double radius,
                        double rotation);

void delete_sketch_slot(FeatureEntry& feature, const std::string& slot_id);
