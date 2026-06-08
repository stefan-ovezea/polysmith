FeatureEntry create_sketch_feature(
    int feature_index, const std::string &plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame) {
  SketchFeatureParameters parameters{
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .active_tool = "select",
      .lines = {},
      .circles = {},
      .points = {},
      .dimensions = {},
      .line_relations = {},
      .profiles = {},
  };

  FeatureEntry feature{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "sketch",
      .name = "Sketch",
      .status = "editing",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = parameters,
  };

  refresh_sketch_derived_state(feature);
  return feature;
}

void set_sketch_tool(FeatureEntry &feature, const std::string &tool) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can change sketch tools");
  }

  validate_tool(tool);
  feature.sketch_parameters->active_tool = tool;
}
