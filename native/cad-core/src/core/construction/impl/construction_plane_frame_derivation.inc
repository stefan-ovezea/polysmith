PlaneFrame derive_offset_frame(const PlaneFrame &source_frame, double offset) {
  // The offset slides the source frame along its own normal. Basis
  // vectors stay aligned with the source so a sketch on the new
  // plane keeps a predictable orientation (common CAD tools do the same).
  return PlaneFrame{
      .origin_x = source_frame.origin_x + source_frame.normal_x * offset,
      .origin_y = source_frame.origin_y + source_frame.normal_y * offset,
      .origin_z = source_frame.origin_z + source_frame.normal_z * offset,
      .x_axis_x = source_frame.x_axis_x,
      .x_axis_y = source_frame.x_axis_y,
      .x_axis_z = source_frame.x_axis_z,
      .y_axis_x = source_frame.y_axis_x,
      .y_axis_y = source_frame.y_axis_y,
      .y_axis_z = source_frame.y_axis_z,
      .normal_x = source_frame.normal_x,
      .normal_y = source_frame.normal_y,
      .normal_z = source_frame.normal_z,
  };
}

PlaneFrame derive_midplane_frame(const PlaneFrame &first_frame,
                                 const PlaneFrame &second_frame) {
  const double dot = first_frame.normal_x * second_frame.normal_x +
                     first_frame.normal_y * second_frame.normal_y +
                     first_frame.normal_z * second_frame.normal_z;
  if (std::abs(dot) < 0.999) {
    throw std::runtime_error("Midplane sources must be parallel planes");
  }

  return PlaneFrame{
      .origin_x = (first_frame.origin_x + second_frame.origin_x) * 0.5,
      .origin_y = (first_frame.origin_y + second_frame.origin_y) * 0.5,
      .origin_z = (first_frame.origin_z + second_frame.origin_z) * 0.5,
      .x_axis_x = first_frame.x_axis_x,
      .x_axis_y = first_frame.x_axis_y,
      .x_axis_z = first_frame.x_axis_z,
      .y_axis_x = first_frame.y_axis_x,
      .y_axis_y = first_frame.y_axis_y,
      .y_axis_z = first_frame.y_axis_z,
      .normal_x = first_frame.normal_x,
      .normal_y = first_frame.normal_y,
      .normal_z = first_frame.normal_z,
  };
}

PlaneFrame derive_angle_plane_frame(const PlaneFrame &source_frame,
                                    const ConstructionAxisFrame &axis,
                                    double angle_degrees) {
  if (!std::isfinite(angle_degrees)) {
    throw std::runtime_error("Angle plane requires a finite angle");
  }

  const Vec3 axis_direction = normalize(
      Vec3{
          .x = axis.end_x - axis.start_x,
          .y = axis.end_y - axis.start_y,
          .z = axis.end_z - axis.start_z,
      },
      "Angle plane axis must have length");
  const Vec3 source_normal = normalize(
      Vec3{
          .x = source_frame.normal_x,
          .y = source_frame.normal_y,
          .z = source_frame.normal_z,
      },
      "Angle plane source normal is invalid");
  if (std::abs(dot(axis_direction, source_normal)) > 1e-5) {
    throw std::runtime_error(
        "Angle plane axis must be parallel to the source plane");
  }

  constexpr double kPi = 3.14159265358979323846;
  const double radians = angle_degrees * kPi / 180.0;
  const Vec3 rotated_normal =
      normalize(rotate_around_axis(source_normal, axis_direction, radians),
                "Angle plane normal is invalid");
  const Vec3 y_axis = normalize(cross(rotated_normal, axis_direction),
                                "Angle plane basis is invalid");

  return PlaneFrame{
      .origin_x = axis.start_x,
      .origin_y = axis.start_y,
      .origin_z = axis.start_z,
      .x_axis_x = axis_direction.x,
      .x_axis_y = axis_direction.y,
      .x_axis_z = axis_direction.z,
      .y_axis_x = y_axis.x,
      .y_axis_y = y_axis.y,
      .y_axis_z = y_axis.z,
      .normal_x = rotated_normal.x,
      .normal_y = rotated_normal.y,
      .normal_z = rotated_normal.z,
  };
}
