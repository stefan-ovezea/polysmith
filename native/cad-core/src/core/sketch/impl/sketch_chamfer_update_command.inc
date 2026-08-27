void update_sketch_chamfer(FeatureEntry &feature, const std::string &chamfer_id,
                           double distance_a, double distance_b) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch chamfers");
  }
  if (distance_a <= kMinimumSketchDimensionValue ||
      distance_b <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch chamfer distances must be positive");
  }

  auto &parameters = *feature.sketch_parameters;
  const auto chamfer_it = std::find_if(
      parameters.chamfers.begin(), parameters.chamfers.end(),
      [&](const SketchChamfer &chamfer) { return chamfer.id == chamfer_id; });
  if (chamfer_it == parameters.chamfers.end()) {
    throw std::runtime_error("Sketch chamfer not found: " + chamfer_id);
  }

  // Defer the fit check (distance too large) to the recompute pass:
  // it'll silently skip the update if the new distance doesn't fit,
  // leaving the previous geometry in place. We do *commit* the new
  // distances so the user can drag a line longer and see the requested
  // chamfer take effect — symmetric with the fillet radius edit.
  chamfer_it->distance_a = distance_a;
  chamfer_it->distance_b = distance_b;
  refresh_sketch_derived_state(feature);
}
