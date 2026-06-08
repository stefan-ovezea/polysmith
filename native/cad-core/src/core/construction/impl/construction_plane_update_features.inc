void update_construction_plane(FeatureEntry &feature, double offset,
                               const PlaneFrame &source_frame) {
  if (feature.kind != "construction_plane" ||
      !feature.construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "Only construction_plane features can be updated with an offset");
  }

  ConstructionPlaneFeatureParameters next =
      feature.construction_plane_parameters.value();
  next.offset = offset;
  next.plane_frame = derive_offset_frame(source_frame, offset);

  feature.parameters_summary = make_parameters_summary(next);
  feature.construction_plane_parameters = next;
}

void update_angle_plane(FeatureEntry &feature, double angle_degrees,
                        const PlaneFrame &source_frame,
                        const ConstructionAxisFrame &axis) {
  if (feature.kind != "construction_plane" ||
      !feature.construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "Only construction_plane features can be updated with an angle");
  }

  ConstructionPlaneFeatureParameters next =
      feature.construction_plane_parameters.value();
  next.angle_degrees = angle_degrees;
  next.plane_frame =
      derive_angle_plane_frame(source_frame, axis, angle_degrees);

  feature.parameters_summary = make_parameters_summary(next);
  feature.construction_plane_parameters = next;
}
