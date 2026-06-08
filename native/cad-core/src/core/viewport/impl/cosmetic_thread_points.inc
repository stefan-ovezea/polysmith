std::vector<double> make_cosmetic_thread_points(
    double start_x,
    double start_y,
    double start_z,
    double end_x,
    double end_y,
    double end_z,
    double radius,
    double pitch,
    double length,
    double start_offset,
    bool right_handed) {
  if (radius <= 0.0 || pitch <= 0.0 || length <= 0.0) {
    return {};
  }
  const WorldPoint start{.x = start_x, .y = start_y, .z = start_z};
  const WorldPoint end{.x = end_x, .y = end_y, .z = end_z};
  const WorldVector axis = subtract_points(end, start);
  const WorldVector dir = normalize_vector(axis);
  if (vector_length(dir) <= 0.0) {
    return {};
  }

  WorldVector seed{.x = 0.0, .y = 1.0, .z = 0.0};
  if (std::abs(dot_product(dir, seed)) > 0.9) {
    seed = WorldVector{.x = 1.0, .y = 0.0, .z = 0.0};
  }
  WorldVector u = normalize_vector(cross_product(dir, seed));
  if (vector_length(u) <= 0.0) {
    return {};
  }
  const WorldVector v = normalize_vector(cross_product(dir, u));

  const double direction_sign = right_handed ? 1.0 : -1.0;
  const int steps = std::max(
      24, static_cast<int>(std::ceil((length / pitch) * 32.0)));
  std::vector<double> points;
  points.reserve(static_cast<size_t>(steps + 1) * 3);
  for (int i = 0; i <= steps; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(steps);
    const double along = start_offset + length * t;
    const double angle = direction_sign * 2.0 * kPi * (along / pitch);
    points.push_back(start.x + dir.x * along + u.x * std::cos(angle) * radius +
                     v.x * std::sin(angle) * radius);
    points.push_back(start.y + dir.y * along + u.y * std::cos(angle) * radius +
                     v.y * std::sin(angle) * radius);
    points.push_back(start.z + dir.z * along + u.z * std::cos(angle) * radius +
                     v.z * std::sin(angle) * radius);
  }
  return points;
}
