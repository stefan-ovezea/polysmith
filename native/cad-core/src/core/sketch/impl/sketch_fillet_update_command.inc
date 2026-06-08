void update_sketch_fillet_radius(FeatureEntry &feature,
                                 const std::string &fillet_id, double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch fillets");
  }
  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch fillets must have a positive radius");
  }

  auto &parameters = *feature.sketch_parameters;
  const auto fillet_it = std::find_if(
      parameters.fillets.begin(), parameters.fillets.end(),
      [&](const SketchFillet &fillet) { return fillet.id == fillet_id; });
  if (fillet_it == parameters.fillets.end()) {
    throw std::runtime_error("Sketch fillet not found: " + fillet_id);
  }

  // Defer the validity check (radius too large) to the recompute
  // pass: it'll silently skip the update if the new radius doesn't
  // fit, leaving the previous radius's geometry in place. We do
  // *commit* the new radius value so the user can drag a line longer
  // and see the requested radius take effect — symmetric with how
  // line-length / dimension drives behave around fixed-point
  // conflicts.
  fillet_it->radius = radius;
  refresh_sketch_derived_state(feature);
}
