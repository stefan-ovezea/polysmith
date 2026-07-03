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

  const bool was_driven = dimension_it->driven;
  dimension_it->driven = !dimension_it->driven;

  // Angle dimensions are always created with is_auto=true and never
  // converted to manual during creation (the creation path for angle
  // is not yet wired through finalize_new_dimension).  Toggling from
  // driven back to driving is a backdoor to make them actually drive
  // the solver: clear is_auto so the solver constraint activates.
  if (was_driven && dimension_it->kind == "angle" && dimension_it->is_auto) {
    dimension_it->is_auto = false;
  }

  refresh_sketch_derived_state(feature);
}
