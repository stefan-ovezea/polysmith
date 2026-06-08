FeatureEntry create_construction_plane_feature(
    int feature_index, const std::string &source_plane_id, double offset,
    const PlaneFrame &source_frame) {
  if (source_plane_id.empty()) {
    throw std::runtime_error("Construction plane requires a source plane id");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "offset",
      .source_plane_id = source_plane_id,
      .source_plane_ids = {source_plane_id},
      .source_axis_id = "",
      .offset = offset,
      .angle_degrees = 0.0,
      .plane_frame = derive_offset_frame(source_frame, offset),
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Offset Plane",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

FeatureEntry create_midplane_feature(int feature_index,
                                     const std::string &first_source_id,
                                     const std::string &second_source_id,
                                     const PlaneFrame &first_frame,
                                     const PlaneFrame &second_frame) {
  if (first_source_id.empty() || second_source_id.empty()) {
    throw std::runtime_error("Midplane requires two source plane ids");
  }
  if (first_source_id == second_source_id) {
    throw std::runtime_error("Midplane requires two different source planes");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "midplane",
      .source_plane_id = first_source_id,
      .source_plane_ids = {first_source_id, second_source_id},
      .source_axis_id = "",
      .offset = 0.0,
      .angle_degrees = 0.0,
      .plane_frame = derive_midplane_frame(first_frame, second_frame),
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Midplane",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

FeatureEntry create_tangent_plane_feature(int feature_index,
                                          const std::string &source_face_id,
                                          const PlaneFrame &tangent_frame) {
  if (source_face_id.empty()) {
    throw std::runtime_error("Tangent plane requires a source face id");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "tangent",
      .source_plane_id = source_face_id,
      .source_plane_ids = {source_face_id},
      .source_axis_id = "",
      .offset = 0.0,
      .angle_degrees = 0.0,
      .plane_frame = tangent_frame,
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Tangent Plane",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

FeatureEntry create_angle_plane_feature(int feature_index,
                                        const std::string &source_plane_id,
                                        const std::string &source_axis_id,
                                        double angle_degrees,
                                        const PlaneFrame &source_frame,
                                        const ConstructionAxisFrame &axis) {
  if (source_plane_id.empty()) {
    throw std::runtime_error("Angle plane requires a source plane id");
  }
  if (source_axis_id.empty()) {
    throw std::runtime_error("Angle plane requires an axis id");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "angle",
      .source_plane_id = source_plane_id,
      .source_plane_ids = {source_plane_id},
      .source_axis_id = source_axis_id,
      .offset = 0.0,
      .angle_degrees = angle_degrees,
      .plane_frame =
          derive_angle_plane_frame(source_frame, axis, angle_degrees),
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Plane at Angle",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}
