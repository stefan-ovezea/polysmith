std::array<double, 3> revolve_sketch_local_to_world(
    const SketchFeatureParameters& sketch,
    double local_x,
    double local_y) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return {
        frame.origin_x + frame.x_axis_x * local_x + frame.y_axis_x * local_y,
        frame.origin_y + frame.x_axis_y * local_x + frame.y_axis_y * local_y,
        frame.origin_z + frame.x_axis_z * local_x + frame.y_axis_z * local_y,
    };
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return {local_x, 0.0, local_y};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return {0.0, local_x, local_y};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return {local_x, local_y, 0.0};
  }
  throw std::runtime_error("Unsupported sketch plane: " + sketch.plane_id);
}

SketchLine* find_line_by_id(SketchFeatureParameters& sketch,
                            const std::string& line_id);
const SketchLine* find_line_by_id(const SketchFeatureParameters& sketch,
                                  const std::string& line_id);

std::optional<RevolveFeatureParameters> make_revolve_parameters_for_profile(
    const FeatureEntry& profile_sketch_feature,
    const SketchProfileRegion& profile,
    const FeatureEntry& axis_sketch_feature,
    const SketchLine& axis_line,
    double angle_degrees) {
  if (!profile_sketch_feature.sketch_parameters.has_value() ||
      !axis_sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }

  const auto& profile_sketch = profile_sketch_feature.sketch_parameters.value();
  const auto& axis_sketch = axis_sketch_feature.sketch_parameters.value();
  const std::string plane_id =
      profile_sketch.plane_frame.has_value()
          ? plane_id_from_frame(profile_sketch.plane_frame.value())
          : profile_sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      profile_sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(
                make_plane_frame(profile_sketch.plane_frame.value()))
          : std::nullopt;
  const auto axis_start = revolve_sketch_local_to_world(
      axis_sketch, axis_line.start_x, axis_line.start_y);
  const auto axis_end = revolve_sketch_local_to_world(
      axis_sketch, axis_line.end_x, axis_line.end_y);

  return RevolveFeatureParameters{
      .sketch_feature_id = profile_sketch_feature.id,
      .profile_id = profile.id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = profile.kind,
      .profile_points = profile.kind == "circle"
                            ? sample_circle_profile_points(profile)
                            : profile.points,
      .inner_loops = profile.inner_loops,
      .axis_sketch_feature_id = axis_sketch_feature.id,
      .axis_entity_id = axis_line.id,
      .axis_start_x = axis_start[0],
      .axis_start_y = axis_start[1],
      .axis_start_z = axis_start[2],
      .axis_end_x = axis_end[0],
      .axis_end_y = axis_end[1],
      .axis_end_z = axis_end[2],
      .angle_degrees = angle_degrees,
  };
}
