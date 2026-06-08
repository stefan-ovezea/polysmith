namespace {

std::string
make_parameters_summary(const ConstructionPlaneFeatureParameters &parameters) {
  std::ostringstream stream;
  if (parameters.plane_type == "midplane") {
    stream << "Between " << parameters.source_plane_ids.size() << " sources";
  } else if (parameters.plane_type == "tangent") {
    stream << "Tangent to face";
  } else if (parameters.plane_type == "angle") {
    stream << parameters.angle_degrees << " deg";
  } else {
    stream << parameters.source_plane_id << " · " << parameters.offset << " mm";
  }
  return stream.str();
}

struct Vec3 {
  double x;
  double y;
  double z;
};

double dot(const Vec3 &a, const Vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3 &a, const Vec3 &b) {
  return Vec3{
      .x = a.y * b.z - a.z * b.y,
      .y = a.z * b.x - a.x * b.z,
      .z = a.x * b.y - a.y * b.x,
  };
}

double magnitude(const Vec3 &value) { return std::sqrt(dot(value, value)); }

Vec3 normalize(const Vec3 &value, const std::string &message) {
  const double length = magnitude(value);
  if (length <= 1e-9) {
    throw std::runtime_error(message);
  }
  return Vec3{
      .x = value.x / length, .y = value.y / length, .z = value.z / length};
}

Vec3 rotate_around_axis(const Vec3 &value, const Vec3 &axis, double radians) {
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  const Vec3 axis_cross_value = cross(axis, value);
  const double axis_dot_value = dot(axis, value);
  return Vec3{
      .x = value.x * c + axis_cross_value.x * s +
           axis.x * axis_dot_value * (1.0 - c),
      .y = value.y * c + axis_cross_value.y * s +
           axis.y * axis_dot_value * (1.0 - c),
      .z = value.z * c + axis_cross_value.z * s +
           axis.z * axis_dot_value * (1.0 - c),
  };
}

} // namespace
