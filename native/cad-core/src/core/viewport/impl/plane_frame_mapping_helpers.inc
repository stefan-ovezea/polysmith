ViewportSolidFace make_solid_face(
    const std::string& owner_id,
    const std::string& owner_kind,
    const std::string& face_suffix,
    const std::string& label,
    const std::string& sketchability,
    double center_x,
    double center_y,
    double center_z,
    double normal_x,
    double normal_y,
    double normal_z,
    const ViewportSolidFace::PlaneFrame& plane_frame,
    double width,
    double height,
    double radius,
    bool is_selected) {
  return ViewportSolidFace{
      .face_id = owner_id + ":face:" + face_suffix,
      .owner_id = owner_id,
      .owner_kind = owner_kind,
      .label = label,
      .sketchability = sketchability,
      .center_x = center_x,
      .center_y = center_y,
      .center_z = center_z,
      .normal_x = normal_x,
      .normal_y = normal_y,
      .normal_z = normal_z,
      .plane_frame = plane_frame,
      .width = width,
      .height = height,
      .radius = radius,
      .is_selected = is_selected,
  };
}

ViewportSolidFace::PlaneFrame make_plane_frame(double origin_x,
                                               double origin_y,
                                               double origin_z,
                                               double x_axis_x,
                                               double x_axis_y,
                                               double x_axis_z,
                                               double y_axis_x,
                                               double y_axis_y,
                                               double y_axis_z,
                                               double normal_x,
                                               double normal_y,
                                               double normal_z) {
  return ViewportSolidFace::PlaneFrame{
      .origin_x = origin_x,
      .origin_y = origin_y,
      .origin_z = origin_z,
      .x_axis_x = x_axis_x,
      .x_axis_y = x_axis_y,
      .x_axis_z = x_axis_z,
      .y_axis_x = y_axis_x,
      .y_axis_y = y_axis_y,
      .y_axis_z = y_axis_z,
      .normal_x = normal_x,
      .normal_y = normal_y,
      .normal_z = normal_z,
  };
}

ViewportSolidFace::PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& frame) {
  return make_plane_frame(frame.origin_x,
                          frame.origin_y,
                          frame.origin_z,
                          frame.x_axis_x,
                          frame.x_axis_y,
                          frame.x_axis_z,
                          frame.y_axis_x,
                          frame.y_axis_y,
                          frame.y_axis_z,
                          frame.normal_x,
                          frame.normal_y,
                          frame.normal_z);
}

ViewportSketchPlaneFrame make_sketch_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& frame) {
  return ViewportSketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

ViewportSketchPlaneFrame make_sketch_plane_frame(
    const PlaneFrame& frame) {
  return ViewportSketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

WorldPoint to_world_point(const std::string& plane_id,
                          double local_x,
                          double local_y,
                          double offset = kSketchPlaneOffset) {
  if (plane_id == "ref-plane-xy") {
    return WorldPoint{.x = local_x, .y = local_y, .z = offset};
  }

  if (plane_id == "ref-plane-yz") {
    return WorldPoint{.x = offset, .y = local_x, .z = local_y};
  }

  return WorldPoint{.x = local_x, .y = offset, .z = local_y};
}

WorldPoint to_world_point(const SketchFeatureParameters& parameters,
                          double local_x,
                          double local_y,
                          double offset = kSketchPlaneOffset) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    return WorldPoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y + frame.normal_x * offset,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y + frame.normal_y * offset,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y + frame.normal_z * offset,
    };
  }

  return to_world_point(parameters.plane_id, local_x, local_y, offset);
}

WorldPoint to_world_point(const SketchFeatureParameters::SketchPlaneFrame& frame,
                          double local_x,
                          double local_y,
                          double offset = 0.0) {
  return WorldPoint{
      .x = frame.origin_x + frame.x_axis_x * local_x +
           frame.y_axis_x * local_y + frame.normal_x * offset,
      .y = frame.origin_y + frame.x_axis_y * local_x +
           frame.y_axis_y * local_y + frame.normal_y * offset,
      .z = frame.origin_z + frame.x_axis_z * local_x +
           frame.y_axis_z * local_y + frame.normal_z * offset,
  };
}

WorldPoint to_world_point(const PlaneFrame& frame,
                          double local_x,
                          double local_y,
                          double offset = 0.0) {
  return WorldPoint{
      .x = frame.origin_x + frame.x_axis_x * local_x +
           frame.y_axis_x * local_y + frame.normal_x * offset,
      .y = frame.origin_y + frame.x_axis_y * local_x +
           frame.y_axis_y * local_y + frame.normal_y * offset,
      .z = frame.origin_z + frame.x_axis_z * local_x +
           frame.y_axis_z * local_y + frame.normal_z * offset,
  };
}

ViewportSolidFace::PlaneFrame make_face_frame_for_plane(const std::string& plane_id,
                                                        double origin_x,
                                                        double origin_y,
                                                        double origin_z) {
  if (plane_id == "ref-plane-xy") {
    return make_plane_frame(origin_x,
                            origin_y,
                            origin_z,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0);
  }

  if (plane_id == "ref-plane-yz") {
    return make_plane_frame(origin_x,
                            origin_y,
                            origin_z,
                            0.0,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0,
                            1.0,
                            0.0,
                            0.0);
  }

  return make_plane_frame(origin_x,
                          origin_y,
                          origin_z,
                          1.0,
                          0.0,
                          0.0,
                          0.0,
                          0.0,
                          1.0,
                          0.0,
                          1.0,
                          0.0);
}
