WorldVector subtract_points(const WorldPoint& left, const WorldPoint& right) {
  return WorldVector{
      .x = left.x - right.x,
      .y = left.y - right.y,
      .z = left.z - right.z,
  };
}

WorldVector cross_product(const WorldVector& left, const WorldVector& right) {
  return WorldVector{
      .x = left.y * right.z - left.z * right.y,
      .y = left.z * right.x - left.x * right.z,
      .z = left.x * right.y - left.y * right.x,
  };
}

double vector_length(const WorldVector& vector) {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y +
                   vector.z * vector.z);
}

WorldVector normalize_vector(const WorldVector& vector) {
  const double length = vector_length(vector);
  if (length <= 0.0) {
    return WorldVector{.x = 0.0, .y = 0.0, .z = 0.0};
  }

  return WorldVector{
      .x = vector.x / length,
      .y = vector.y / length,
      .z = vector.z / length,
  };
}

double dot_product(const WorldVector& left, const WorldVector& right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}
