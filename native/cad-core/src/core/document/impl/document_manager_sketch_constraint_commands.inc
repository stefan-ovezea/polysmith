  DocumentState update_sketch_line(const std::string& line_id,
                                   double start_x,
                                   double start_y,
                                   double end_x,
                                   double end_y);
  DocumentState update_sketch_vertex(const std::string& point_id,
                                    double x,
                                    double y);
  // Rigidly move sketch entities (translate + optional rotation around
  // a center).  Pushes exactly ONE undo step; selection is preserved so
  // the user can repeat the move on the same entities.
  DocumentState move_sketch_entities(const std::vector<std::string>& entity_ids,
                                     double dx,
                                     double dy,
                                     double center_x,
                                     double center_y,
                                     double angle_deg);
  DocumentState set_sketch_line_constraint(
      const std::string& line_id,
      const std::optional<std::string>& constraint);
  DocumentState set_sketch_equal_length_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState set_sketch_perpendicular_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  // Make a sketch line tangent to a circle. Pass an empty
  // `circle_id` to clear an existing tangent relation.
  DocumentState set_sketch_tangent_constraint(const std::string& line_id,
                                              const std::string& circle_id);
  DocumentState set_sketch_parallel_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState clear_sketch_line_constraints(const std::string& line_id);
  DocumentState set_sketch_coincident_constraint(const std::string& point_id,
                                                 const std::string& other_point_id);
  DocumentState delete_sketch_coincident_constraint(const std::string& constraint_id);
  DocumentState set_sketch_vertex_fixed(const std::string& point_id,
                                       bool is_fixed);
  // Bind a sketch point to the midpoint of a host line; the solver
  // re-pulls the point on every edit. Pass an empty `host_line_id`
  // to remove an existing anchor.
  DocumentState set_sketch_midpoint_anchor(const std::string& point_id,
                                           const std::string& host_line_id);
  // Bind a sketch point to a host line's body at parametric position
  // `t` in [0, 1]. Pass an empty `host_line_id` to clear the anchor.
  DocumentState set_sketch_vertex_line_anchor(const std::string& point_id,
                                             const std::string& host_line_id,
                                             double t);

  // ── Constraint completion (feature/sketch) ─────────────────────────
  DocumentState set_sketch_symmetric_constraint(
      const std::string& point_id,
      const std::string& other_point_id,
      const std::string& axis_line_id);
  DocumentState set_sketch_midpoint_constraint(const std::string& vertex_id,
                                               const std::string& line_id);
  DocumentState set_sketch_collinear_constraint(const std::string& line_id,
                                                const std::string& other_line_id);
  DocumentState set_sketch_tangent_pair_constraint(
      const std::string& first_id,
      const std::string& second_id);
