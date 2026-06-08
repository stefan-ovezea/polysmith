void delete_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });
  if (dimension_it == parameters.dimensions.end()) {
    // Dimension may not exist: construction lines don't get auto-dims,
    // and the TS side may fire deletion for a shape that was just
    // committed without a dimension. Silently ignore.
    return;
  }

  parameters.dimensions.erase(dimension_it);
  refresh_sketch_derived_state(feature);
}
