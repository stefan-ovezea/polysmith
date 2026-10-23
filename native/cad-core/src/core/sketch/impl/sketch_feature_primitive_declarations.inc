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
                     bool infer_constraints = true,
                     // Endpoints are normally shared with any existing
                     // vertex within 0.01 mm, which is what keeps a sketch
                     // connected. Importers pass false: their geometry must
                     // stay EXACT, and a dense tessellation (a Fusion sketch
                     // export is 0.005 mm spline steps) has segments shorter
                     // than that tolerance, so coalescing would collapse
                     // them onto a single vertex.
                     bool share_endpoints = true,
                     // "" for a plain line; "centerline" / "infinite" also
                     // force is_construction and add only presentation
                     // behaviour (dash-dot / drawn to the viewport edge).
                     const std::string& line_type = "");
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

// Control-point B-spline: `points` ARE the poles (regular movable
// vertices).  Degree = min(3, count - 1), clamped open-uniform knots.
// No solver registration — pole drags re-fit the curve through the
// ordinary vertex sync.
// `mode` is "control" (clicks ARE the poles, the original behaviour) or "fit"
// (clicks are fit points; the curve is solved to pass through all of them).
// `degree` <= 0 keeps the default (3, clamped to point_count - 1); an explicit
// degree is clamped to [1, 5] and to point_count - 1.
void add_sketch_spline(FeatureEntry& feature,
                       int spline_index,
                       const std::vector<std::pair<double, double>>& points,
                       bool is_construction = false,
                       bool refresh_immediately = true,
                       const std::string& mode = "control",
                       int degree = 0);

// Straight slot (stadium): parametric record expanded by
// refresh_sketch_slots into 2 lines + 2 arcs (generated_by).
// `mode` is "straight" (stadium; length + rotation) or "arc" (a slot whose
// centreline is an arc: `arc_radius`, `start_angle`, `sweep_angle`, with
// center_x/center_y as the ARC centre and `radius` as the radial half-width).
void add_sketch_slot(FeatureEntry& feature,
                     int slot_index,
                     double center_x,
                     double center_y,
                     double length,
                     double radius,
                     double rotation,
                     bool is_construction = false,
                     const std::string& mode = "straight",
                     double arc_radius = 0.0,
                     double start_angle = 0.0,
                     double sweep_angle = 0.0);

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
                        double rotation,
                        double arc_radius = 0.0,
                        double start_angle = 0.0,
                        double sweep_angle = 0.0);

void delete_sketch_slot(FeatureEntry& feature, const std::string& slot_id);
