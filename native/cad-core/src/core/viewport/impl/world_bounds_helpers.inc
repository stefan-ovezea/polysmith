void include_world_point_for_plane(const std::string& plane_id,
                                   double local_x,
                                   double local_y,
                                   double depth,
                                   double& min_x,
                                   double& max_x,
                                   double& min_y,
                                   double& max_y,
                                   double& min_z,
                                   double& max_z) {
  const auto include_point =
      [&](double world_x, double world_y, double world_z) {
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_y = std::min(min_y, world_y);
        max_y = std::max(max_y, world_y);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      };

  if (plane_id == "ref-plane-xy") {
    include_point(local_x, 0.0, local_y);
    include_point(local_x, depth, local_y);
    return;
  }

  if (plane_id == "ref-plane-yz") {
    include_point(0.0, local_x, local_y);
    include_point(depth, local_x, local_y);
    return;
  }

  include_point(local_x, local_y, 0.0);
  include_point(local_x, local_y, depth);
}

void include_world_point_for_frame(
    const PlaneFrame& frame,
    double local_x,
    double local_y,
    double depth,
    double& min_x,
    double& max_x,
    double& min_y,
    double& max_y,
    double& min_z,
    double& max_z) {
  const auto include_point =
      [&](double world_x, double world_y, double world_z) {
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_y = std::min(min_y, world_y);
        max_y = std::max(max_y, world_y);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      };

  const auto to_world = [&](double offset) {
    return WorldPoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y + frame.normal_x * offset,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y + frame.normal_y * offset,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y + frame.normal_z * offset,
    };
  };

  const WorldPoint start = to_world(0.0);
  const WorldPoint end = to_world(depth);
  include_point(start.x, start.y, start.z);
  include_point(end.x, end.y, end.z);
}
