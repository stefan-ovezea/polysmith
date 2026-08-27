  DocumentState update_sketch_circle(const std::string& circle_id,
                                     double center_x,
                                     double center_y,
                                     double radius);
  DocumentState add_sketch_line(double start_x,
                                double start_y,
                                double end_x,
                                double end_y,
                                bool is_construction = false);
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
                                  double radius);
  DocumentState update_sketch_fillet_radius(const std::string& fillet_id,
                                            double radius);
  DocumentState delete_sketch_fillet(const std::string& fillet_id);
  // Sketch chamfer — bevel a corner shared by two sketch lines into a
  // straight chamfer line, trimming each line by its own distance.
  // Parametric like the fillet (see `SketchChamfer`), with its own
  // update / delete commands. A corner already holding a fillet is
  // rejected.
  DocumentState add_sketch_chamfer(const std::string& corner_vertex_id,
                                   const std::string& line_a_id,
                                   const std::string& line_b_id,
                                   double distance_a,
                                   double distance_b);
  DocumentState update_sketch_chamfer(const std::string& chamfer_id,
                                      double distance_a,
                                      double distance_b);
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
  // Extend a sketch entity (line or arc) from the end nearest the
  // click to the nearest intersection with another non-construction
  // entity. Single undoable action via full sketch snapshot.
  DocumentState extend_sketch_entity(const std::string& entity_id,
                                     double click_x,
                                     double click_y);
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
  DocumentState add_sketch_spline(
      const std::vector<std::pair<double, double>>& pole_points,
      bool is_construction = false);

  // Straight slot (stadium): center + length + radius + rotation (rad).
  DocumentState add_sketch_slot(double center_x,
                                double center_y,
                                double length,
                                double radius,
                                double rotation,
                                bool is_construction = false);
  DocumentState update_sketch_slot(const std::string& slot_id,
                                   double center_x,
                                   double center_y,
                                   double length,
                                   double radius,
                                   double rotation);
