// Add a new "angle" dimension between two sketch lines that share an
// endpoint. The dimension is stored with `entity_id = first_line_id`,
// `secondary_entity_id = second_line_id`, and `value` initialized to
// the current angle (radians) between their outgoing direction
// vectors. Throws if the two lines do not share an endpoint within
// the coincident tolerance.
void add_sketch_angle_dimension(FeatureEntry& feature,
                                const std::string& first_line_id,
                                const std::string& second_line_id,
                                std::optional<double> value = std::nullopt);
void add_sketch_distance_dimension(FeatureEntry& feature,
                                   const std::string& first_entity_id,
                                   const std::string& second_entity_id);
void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value,
                             std::optional<std::string> expression = std::nullopt);
// Re-evaluate every dimension that has a non-empty `expression` field
// against the given parameter table. Dimension values are overwritten
// with the resolved result; dimensions whose expressions fail to
// resolve keep their last good value silently.
void reify_dimension_expressions(
    FeatureEntry& feature,
    const std::vector<struct ParameterEntry>& parameters);
void delete_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id);

// Single-entity dimension creation (for the Dimension tool clicking a
// lone line, circle, or polygon that has no auto-dimension — e.g.
// because the fusion-style on-demand system deleted it). Each function
// validates the target entity exists, checks that the dimension id
// isn't already in use, and pushes a SketchDimension computed from the
// entity's current geometry.
void add_sketch_line_length_dimension(FeatureEntry& feature,
                                      const std::string& line_id);
void add_sketch_line_angle_dimension(FeatureEntry& feature,
                                     const std::string& line_id);
void add_sketch_circle_radius_dimension(FeatureEntry& feature,
                                        const std::string& circle_id,
                                        std::optional<std::string> display_as = std::nullopt);
void add_sketch_polygon_radius_dimension(FeatureEntry& feature,
                                         const std::string& polygon_id);
void add_sketch_point_distance_dimension(FeatureEntry& feature,
                                         const std::string& point_a_id,
                                         const std::string& point_b_id,
                                         std::optional<std::string> axis = std::nullopt);
