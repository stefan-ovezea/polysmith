  // Add an angle dimension between two sketch lines that share an
  // endpoint. The dimension's `value` is initialized to the current
  // angle (radians); subsequent `update_sketch_dimension` calls
  // drive the second line's rotation about the shared endpoint.
  DocumentState add_sketch_angle_dimension(const std::string& first_line_id,
                                           const std::string& second_line_id,
                                           std::optional<double> value = std::nullopt);
  DocumentState add_sketch_distance_dimension(
      const std::string& first_entity_id,
      const std::string& second_entity_id);
  // Single-entity dimension creation through the Dimension tool.
  // Each method finds the active sketch feature, pushes an undo state,
  // delegates to the sketch_feature core helper, selects the new
  // dimension, and bumps the geometry revision.
  DocumentState add_sketch_line_length_dimension(const std::string& line_id);
  DocumentState add_sketch_line_angle_dimension(const std::string& line_id);
  DocumentState add_sketch_circle_radius_dimension(
      const std::string& circle_id,
      std::optional<std::string> display_as = std::nullopt);
  DocumentState add_sketch_arc_radius_dimension(
      const std::string& arc_id);
  DocumentState add_sketch_polygon_radius_dimension(
      const std::string& polygon_id);
  DocumentState add_sketch_vertex_distance_dimension(
      const std::string& point_a_id,
      const std::string& point_b_id,
      std::optional<std::string> axis = std::nullopt);
  DocumentState update_sketch_dimension(const std::string& dimension_id,
                                        double value,
                                        std::optional<std::string> expression = std::nullopt);
  DocumentState update_sketch_dimension_label_position(
      const std::string& dimension_id,
      double label_x,
      double label_y);
  DocumentState delete_sketch_dimension(const std::string& dimension_id);
  DocumentState update_sketch_dimension_display(const std::string& dimension_id,
                                                const std::string& display_as);
  DocumentState toggle_sketch_dimension_driven(const std::string& dimension_id);
