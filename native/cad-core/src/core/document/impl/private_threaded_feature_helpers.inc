std::vector<double> make_helix_points(const ConstructionAxisFeatureParameters& axis,
                                      const HelixFeatureParameters& params) {
  const double ax = axis.end_x - axis.start_x;
  const double ay = axis.end_y - axis.start_y;
  const double az = axis.end_z - axis.start_z;
  const double length = std::sqrt(ax * ax + ay * ay + az * az);
  if (length <= 1.0e-9 || params.radius <= 0.0 || params.pitch <= 0.0 ||
      params.height <= 0.0) {
    return {};
  }
  const std::array<double, 3> dir{ax / length, ay / length, az / length};
  std::array<double, 3> seed =
      std::abs(dir[0]) < 0.9 ? std::array<double, 3>{1.0, 0.0, 0.0}
                             : std::array<double, 3>{0.0, 1.0, 0.0};
  std::array<double, 3> u{
      dir[1] * seed[2] - dir[2] * seed[1],
      dir[2] * seed[0] - dir[0] * seed[2],
      dir[0] * seed[1] - dir[1] * seed[0],
  };
  const double u_len = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  if (u_len <= 1.0e-9) {
    return {};
  }
  u = {u[0] / u_len, u[1] / u_len, u[2] / u_len};
  const std::array<double, 3> v{
      dir[1] * u[2] - dir[2] * u[1],
      dir[2] * u[0] - dir[0] * u[2],
      dir[0] * u[1] - dir[1] * u[0],
  };
  const double turns = params.height / params.pitch;
  const int samples = std::max(24, static_cast<int>(std::ceil(turns * 48.0)));
  const double handed = params.handedness == "left" ? -1.0 : 1.0;
  const double start = params.start_angle_degrees * kPi / 180.0;
  std::vector<double> points;
  points.reserve(static_cast<size_t>(samples + 1) * 3);
  for (int i = 0; i <= samples; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(samples);
    const double along = params.height * t;
    const double angle = start + handed * 2.0 * kPi * turns * t;
    const double radial_u = std::cos(angle) * params.radius;
    const double radial_v = std::sin(angle) * params.radius;
    points.push_back(axis.start_x + dir[0] * along + u[0] * radial_u + v[0] * radial_v);
    points.push_back(axis.start_y + dir[1] * along + u[1] * radial_u + v[1] * radial_v);
    points.push_back(axis.start_z + dir[2] * along + u[2] * radial_u + v[2] * radial_v);
  }
  return points;
}
