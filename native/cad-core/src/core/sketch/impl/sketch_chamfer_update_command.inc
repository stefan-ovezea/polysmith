void update_sketch_chamfer(FeatureEntry &feature, const std::string &chamfer_id,
                           double distance_a, double distance_b,
                           const std::string &mode, double angle_degrees) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch chamfers");
  }
  if (distance_a <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch chamfer distances must be positive");
  }

  auto &parameters = *feature.sketch_parameters;
  const auto chamfer_it = std::find_if(
      parameters.chamfers.begin(), parameters.chamfers.end(),
      [&](const SketchChamfer &chamfer) { return chamfer.id == chamfer_id; });
  if (chamfer_it == parameters.chamfers.end()) {
    throw std::runtime_error("Sketch chamfer not found: " + chamfer_id);
  }

  // An empty mode means "keep the record's mode": the plain distance edit
  // must not silently convert a distance+angle chamfer into a
  // two-distance one (which would freeze the angle-derived distance).
  const std::string resolved_mode =
      mode.empty() ? chamfer_it->mode
                   : (mode == "equal" || mode == "distance_angle"
                          ? mode
                          : std::string("two_distance"));
  double resolved_angle = chamfer_it->angle_degrees;
  if (resolved_mode == "distance_angle" && angle_degrees > 0.0) {
    resolved_angle = angle_degrees;
  }
  if (resolved_mode == "distance_angle" &&
      (!(resolved_angle > 0.0) || resolved_angle >= 180.0)) {
    throw std::runtime_error(
        "Sketch chamfer: the angle must be between 0 and 180 degrees");
  }
  if (resolved_mode == "equal") {
    distance_b = distance_a;
  } else if (resolved_mode == "two_distance" &&
             distance_b <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch chamfer distances must be positive");
  }

  // Defer the fit check (distance too large, angle that no longer fits)
  // to the recompute pass: it'll silently skip the update, leaving the
  // previous geometry in place. We do *commit* the requested values so
  // the user can drag a line longer and see the chamfer take effect —
  // symmetric with the fillet radius edit. For "distance_angle" the
  // recompute derives distance_b from the angle itself, so the value
  // stored here is only a placeholder.
  chamfer_it->distance_a = distance_a;
  chamfer_it->distance_b = distance_b;
  chamfer_it->mode = resolved_mode;
  chamfer_it->angle_degrees =
      resolved_mode == "distance_angle" ? resolved_angle : 0.0;
  refresh_sketch_derived_state(feature);
}
