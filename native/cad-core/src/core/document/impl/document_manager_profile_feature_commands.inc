  DocumentState add_box_feature(const BoxFeatureParameters& parameters);
  DocumentState add_cylinder_feature(const CylinderFeatureParameters& parameters);
  DocumentState update_box_feature(const std::string& feature_id,
                                   const BoxFeatureParameters& parameters);
  DocumentState update_cylinder_feature(
      const std::string& feature_id,
      const CylinderFeatureParameters& parameters);
  DocumentState update_extrude_depth(const std::string& feature_id,
                                     double depth);
  DocumentState update_extrude_mode(const std::string& feature_id,
                                    const std::string& mode);
  DocumentState update_extrude_target_body(
      const std::string& feature_id,
      const std::optional<std::string>& target_body_id);
  DocumentState update_extrude_parameters(
      const std::string& feature_id,
      const ExtrudeFeatureParameters& parameters);
  DocumentState update_extrude_profiles(
      const std::string& feature_id,
      const std::vector<std::string>& profile_ids);
  DocumentState loft_profiles(const std::vector<std::string>& profile_ids,
                              bool ruled = false);
  DocumentState update_loft_profiles(
      const std::string& feature_id,
      const std::vector<std::string>& profile_ids);
  DocumentState update_loft_ruled(const std::string& feature_id, bool ruled);
  DocumentState revolve_profile(const std::string& profile_id,
                                const std::string& axis_entity_id,
                                double angle_degrees = 360.0);
  DocumentState update_revolve_profile(const std::string& feature_id,
                                       const std::string& profile_id);
  DocumentState update_revolve_axis(const std::string& feature_id,
                                    const std::string& axis_entity_id);
  DocumentState update_revolve_angle(const std::string& feature_id,
                                     double angle_degrees);
  DocumentState sweep_profile(const std::string& profile_id,
                              const std::string& path_entity_id);
  DocumentState update_sweep_profile(const std::string& feature_id,
                                     const std::string& profile_id);
  DocumentState update_sweep_path(const std::string& feature_id,
                                  const std::string& path_entity_id);
  DocumentState extrude_profile(
      const std::string& profile_id,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState extrude_profiles(
      const std::vector<std::string>& profile_ids,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState extrude_open_entities(
      const std::vector<std::string>& entity_ids,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
  DocumentState extrude_face(
      const std::string& face_id,
      double depth,
      const std::string& mode = "new_body",
      const std::optional<std::string>& target_body_id = std::nullopt,
      const std::optional<ExtrudeFeatureParameters>& parameters =
          std::nullopt);
