  DocumentState update_sketch_circle(const std::string& circle_id,
                                     double center_x,
                                     double center_y,
                                     double radius);
  // `line_type` is "" for an ordinary line; "centerline" / "infinite" mark
  // a reference line (they also force the construction flag, so profiles
  // and auto dimensions ignore them).
  // ttach_to_geometry attaches the new endpoints to whatever line they were
  // drawn onto (Fusion behaviour). Only the IPC handler for a user-drawn line sets
  // it; every internal caller keeps the default so trim/import/CAM are unchanged.
  DocumentState add_sketch_line(double start_x,
                                double start_y,
                                double end_x,
                                double end_y,
                                bool is_construction = false,
                                const std::string& line_type = "",
                                bool attach_to_geometry = false);
  // Toggle the construction-line flag on an existing sketch line.
  // Construction lines render dashed and are skipped during profile
  // detection so they don't seal pickable faces.
  DocumentState set_sketch_line_construction(const std::string& line_id,
                                             bool is_construction);
  DocumentState add_sketch_rectangle(double start_x,
                                     double start_y,
                                     double end_x,
                                     double end_y,
                                     bool is_construction = false);
  // Rectangle whose four corners are filleted alike, as ONE undoable intent:
  // the rectangle is created, then its corners are filleted through the ordinary
  // fillet path inside an undo group, so the result keeps parametric fillet
  // records (radius edits, delete restores the corner) instead of four arcs
  // welded on. `radius` must be less than half the shorter side.
  DocumentState add_sketch_rounded_rectangle(double start_x,
                                             double start_y,
                                             double end_x,
                                             double end_y,
                                             double radius,
                                             bool is_construction = false);
  // Standalone sketch point (Point tool). The point is stored as a
  // `SketchPoint` record and re-emitted into the vertex table by every
  // refresh as kind "point"; its id IS the vertex id. A click within the
  // coincidence tolerance of an existing point selects that point instead
  // of stacking a second one on top of it.
  DocumentState add_sketch_point(double x, double y);
  DocumentState add_sketch_circle(
      double center_x,
      double center_y,
      double radius,
      bool is_construction = false,
      const std::string& mode = "center_radius",
      double p1_x = 0.0,
      double p1_y = 0.0,
      double p2_x = 0.0,
      double p2_y = 0.0,
      double p3_x = 0.0,
      double p3_y = 0.0,
      const std::string& line_a_id = "",
      const std::string& line_b_id = "",
      const std::string& line_c_id = "",
      double hint_x = 0.0,
      double hint_y = 0.0);
  // Add a regular polygon to the active sketch. mode is one of
  // "circumscribed", "inscribed", "edge".
  DocumentState add_sketch_polygon(int sides,
                                   const std::string& mode,
                                   double start_x,
                                   double start_y,
                                   double end_x,
                                   double end_y,
                                   bool is_construction = false);
  // Add an arc to the active sketch. `mode` selects how the three
  // input points are interpreted:
  //   - "three_point": (start, end, anchor) — anchor lies on the arc
  //     and fixes the bulge / radius. Center is the circumcenter of
  //     the three points.
  //   - "center_start_end": (start, end, anchor) — anchor is the
  //     arc's center. Radius derives from |center - start|; the end
  //     point is snapped onto the resulting circle so the arc closes
  //     cleanly.
  // Returns the document with the new arc and its endpoint points
  // appended.
  DocumentState add_sketch_arc(double start_x,
                               double start_y,
                               double end_x,
                               double end_y,
                               double anchor_x,
                               double anchor_y,
                               const std::string& mode,
                               bool is_construction = false);
  // Sketch fillet — round a corner shared by two sketch lines into
  // a tangent arc. The corner is identified by the sketch point id
  // shared by both lines; the relationship is parametric (see
  // `SketchFillet` in feature.h) so the fillet survives subsequent
  // sketch edits and has its own update / delete commands.
  DocumentState add_sketch_fillet(const std::string& corner_vertex_id,
                                  const std::string& line_a_id,
                                  const std::string& line_b_id,
                                  double radius,
                                  const std::string& arc_a_id = "",
                                  const std::string& arc_b_id = "");
  DocumentState update_sketch_fillet_radius(const std::string& fillet_id,
                                            double radius);
  DocumentState delete_sketch_fillet(const std::string& fillet_id);
  // Merge every pair of sketch vertices that sits within the
  // coincident tolerance (0.01 mm) into a single shared vertex —
  // heals legacy sketches whose projections were created before the
  // endpoint welding existed (the classic symptom: two entities meet
  // at a corner but hold two vertex ids a few microns apart, so the
  // fillet tool refuses the corner).  Entity endpoint / center ids,
  // fillet and chamfer corner/trim ids, and coincident-constraint
  // target ids are remapped; entity cached coords are snapped to the
  // representative vertex's position.  Single undoable action.
  DocumentState merge_coincident_sketch_points(const std::string& feature_id);
  // Sketch chamfer — bevel a corner shared by two sketch lines into a
  // straight chamfer line, trimming each line by its own distance.
  // Parametric like the fillet (see `SketchChamfer`), with its own
  // update / delete commands. A corner already holding a fillet is
  // rejected. `mode` is "equal" | "two_distance" | "distance_angle"; an
  // empty mode means two-distance. For "distance_angle", `distance_b` is
  // derived from `angle_degrees` (the angle between the chamfer line and
  // line A) and re-derived on every recompute.
  DocumentState add_sketch_chamfer(
      const std::string& corner_vertex_id, const std::string& line_a_id,
      const std::string& line_b_id, double distance_a, double distance_b,
      const std::string& mode = "", double angle_degrees = 0.0);
  // `mode` empty keeps the chamfer's existing mode; `angle_degrees <= 0`
  // keeps its existing angle.
  DocumentState update_sketch_chamfer(const std::string& chamfer_id,
                                      double distance_a, double distance_b,
                                      const std::string& mode = "",
                                      double angle_degrees = 0.0);
  DocumentState delete_sketch_chamfer(const std::string& chamfer_id);
  DocumentState delete_sketch_selection(
      const std::vector<std::string>& entity_ids,
      const std::vector<std::string>& point_ids,
      const std::vector<std::string>& profile_ids);
  // Trim a sketch entity at its intersection points. The entity is
  // split at all intersections with other non-construction entities and
  // only the segment under (click_x, click_y) is kept. Phase 1 handles
  // lines only. Single undoable action via full sketch snapshot.
  DocumentState trim_sketch_entity(
      const std::string& entity_id,
      double click_x,
      double click_y,
      std::optional<int> segment_index = std::nullopt,
      std::vector<std::string>* minted_vertex_ids = nullptr,
      std::optional<int> expected_revision = std::nullopt);
  // Drag-paint trim: one stroke trims every crossed entity and commits
  // as ONE undo entry (R5). Each entry re-derives its segment from the
  // click point against the CURRENT geometry. Failed entries are
  // logged and skipped by the engine.
  DocumentState trim_sketch_stroke(
      const std::vector<polysmith::core::TrimStrokeEntry>& entries);
  // Extend a sketch entity (line or arc) from the end nearest the
  // click to the nearest intersection with another non-construction
  // entity. Single undoable action via full sketch snapshot.
  DocumentState extend_sketch_entity(const std::string& entity_id,
                                     double click_x,
                                     double click_y);
  // Corner-trim two sketch entities (lines or arcs) to their virtual
  // corner: the intersection of their underlying curves nearest the
  // click. Both entities keep the portion from their far end to the
  // corner. Single undoable action via full sketch snapshot.
  DocumentState corner_trim_sketch_entities(const std::string& entity_a_id,
                                            const std::string& entity_b_id,
                                            double click_x,
                                            double click_y);
  // Split a sketch entity at all its intersections without deleting
  // anything. Circles and full ellipses need TWO split points
  // (SolidWorks rule). Single undoable action; the fresh split
  // vertices are frozen before the solver pass.
  DocumentState split_sketch_entity(const std::string& entity_id,
                                    double click_x,
                                    double click_y,
                                    double split2_x,
                                    double split2_y);
  // Offset a sketch entity (line / circle / arc) by a signed
  // distance, creating a new entity. Single undoable action.
  DocumentState offset_sketch_entity(const std::string& entity_id,
                                     double distance);
  // Parametric sketch text. The glyph geometry is derived into sketch
  // lines on every recompute (see SketchText / refresh_sketch_texts);
  // these commands manage the parameter records. `values` carries the
  // full parameter set — update callers merge partial patches first.
  DocumentState add_sketch_text(const SketchText& text);
  DocumentState update_sketch_text(const std::string& text_id,
                                   const SketchText& values);
  DocumentState delete_sketch_text(const std::string& text_id);

  // Full ellipse: center + major-axis point + minor-axis point.
  DocumentState add_sketch_ellipse(double center_x,
                                   double center_y,
                                   double axis_a_x,
                                   double axis_a_y,
                                   double axis_b_x,
                                   double axis_b_y,
                                   bool is_construction = false);

  // Control-point B-spline: the points ARE the poles (regular movable
  // vertices).  Degree = min(3, count-1), clamped open-uniform knots.
  // `mode` "control" (default) treats the points as poles; "fit" solves the
  // curve to pass through them. `degree` <= 0 means the default 3; an explicit
  // value is clamped to [1, 5] and to points.size() - 1.
  DocumentState add_sketch_spline(
      const std::vector<std::pair<double, double>>& pole_points,
      bool is_construction = false,
      const std::string& mode = "control",
      int degree = 0);

  // Straight slot (stadium): center + length + radius + rotation (rad).
  // Straight slot (stadium) by default, or — with mode "arc" — a slot whose
  // centreline is an arc (`arc_radius`, `start_angle`, `sweep_angle`; centre is
  // the ARC centre, radius is the radial half-width).
  DocumentState add_sketch_slot(double center_x,
                                double center_y,
                                double length,
                                double radius,
                                double rotation,
                                bool is_construction = false,
                                const std::string& mode = "straight",
                                double arc_radius = 0.0,
                                double start_angle = 0.0,
                                double sweep_angle = 0.0);
  DocumentState update_sketch_slot(const std::string& slot_id,
                                   double center_x,
                                   double center_y,
                                   double length,
                                   double radius,
                                   double rotation,
                                   double arc_radius = 0.0,
                                   double start_angle = 0.0,
                                   double sweep_angle = 0.0);
