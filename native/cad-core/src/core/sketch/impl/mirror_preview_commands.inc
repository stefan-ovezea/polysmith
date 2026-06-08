void start_mirror_preview(FeatureEntry &feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  // Idempotent: re-arming the mirror tool while a preview is
  // already in progress just resets the selections. Saves the
  // caller from having to cancel + start.
  feature.sketch_parameters->pending_mirror =
      SketchFeatureParameters::PendingMirror{};
}

void update_mirror_preview_axis(FeatureEntry &feature,
                                const std::string &axis_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto &parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error(
        "Mirror preview must be started before setting the axis");
  }
  if (axis_line_id.empty()) {
    parameters.pending_mirror->axis_line_id = std::nullopt;
  } else {
    parameters.pending_mirror->axis_line_id = axis_line_id;
  }
  regenerate_mirror_preview(parameters);
}

void update_mirror_preview_objects(FeatureEntry &feature,
                                   const std::vector<std::string> &object_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto &parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error(
        "Mirror preview must be started before setting objects");
  }
  parameters.pending_mirror->object_ids = object_ids;
  regenerate_mirror_preview(parameters);
}

void cancel_mirror_preview(FeatureEntry &feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  // Plain discard. `pending_mirror` was never written to the main
  // arrays so there's nothing to roll back.
  feature.sketch_parameters->pending_mirror = std::nullopt;
}
