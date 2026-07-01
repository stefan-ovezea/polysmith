void toggle_sketch_dimension_driven(FeatureEntry& feature,
                                    const std::string& dimension_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can toggle sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });
  if (dimension_it == parameters.dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  dimension_it->driven = !dimension_it->driven;
  refresh_sketch_derived_state(feature);
}
