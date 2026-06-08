polysmith::core::LoftFeatureParameters
loft_parameters_from_payload(const json& payload) {
  polysmith::core::LoftFeatureParameters params{};
  if (payload.contains("ruled") && payload.at("ruled").is_boolean()) {
    params.ruled = payload.at("ruled").get<bool>();
  }
  if (payload.contains("sections") && payload.at("sections").is_array()) {
    for (const auto& section_payload : payload.at("sections")) {
      polysmith::core::LoftSectionParameters section{};
      section.sketch_feature_id =
          read_string(section_payload, "sketch_feature_id");
      section.profile_id = read_string(section_payload, "profile_id");
      section.plane_id = read_string(section_payload, "plane_id");
      if (section_payload.contains("plane_frame") &&
          !section_payload.at("plane_frame").is_null()) {
        section.plane_frame =
            plane_frame_from_payload(section_payload.at("plane_frame"));
      }
      if (section_payload.contains("profile_points") &&
          section_payload.at("profile_points").is_array()) {
        for (const auto& point_payload : section_payload.at("profile_points")) {
          section.profile_points.push_back(polysmith::core::SketchProfilePoint{
              .x = point_payload.at("x").get<double>(),
              .y = point_payload.at("y").get<double>(),
          });
        }
      }
      params.sections.push_back(std::move(section));
    }
  }
  return params;
}
