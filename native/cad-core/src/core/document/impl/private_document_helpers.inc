void mark_extrude_preview_warning(FeatureEntry& feature,
                                  const std::string& message) {
  feature.status = "warning";
  feature.dependency_broken = true;
  feature.dependency_warning = message;
  feature.parameters_summary = "Preview unavailable";
}

void apply_extrude_parameters_with_preview_validation(
    FeatureEntry& feature,
    const ExtrudeFeatureParameters& parameters) {
  feature.extrude_parameters = parameters;
  try {
    const auto preview_shape = build_extrude_shape(parameters);
    if (preview_shape.IsNull()) {
      throw std::runtime_error("Extrude preview produced an empty shape");
    }
    mark_feature_healthy(feature);
    feature.parameters_summary =
        parameters.profile_id + " · " + std::to_string(parameters.depth) + " mm";
  } catch (const std::exception& error) {
    mark_extrude_preview_warning(feature, error.what());
  }
}
