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
  DocumentState add_sketch_circle(double center_x,
                                  double center_y,
                                  double radius,
                                  bool is_construction = false);
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
  DocumentState add_sketch_fillet(const std::string& corner_point_id,
                                  const std::string& line_a_id,
                                  const std::string& line_b_id,
                                  double radius);
  DocumentState update_sketch_fillet_radius(const std::string& fillet_id,
                                            double radius);
  DocumentState delete_sketch_fillet(const std::string& fillet_id);
  DocumentState delete_sketch_selection(
      const std::vector<std::string>& entity_ids,
      const std::vector<std::string>& point_ids,
      const std::vector<std::string>& profile_ids);
  // Trim a sketch entity at its intersection points. The entity is
  // split at all intersections with other non-construction entities and
  // only the segment under (click_x, click_y) is kept. Phase 1 handles
  // lines only. Single undoable action via full sketch snapshot.
  DocumentState trim_sketch_entity(const std::string& entity_id,
                                   double click_x,
                                   double click_y);
