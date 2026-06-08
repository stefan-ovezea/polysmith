constexpr double kProfileTolerance = 0.01;
// Per-arc sample count when materializing a closed loop into a
// polyline. 16 segments per arc gives a visually smooth approximation
// at typical sketch scales while keeping the resulting polygon cheap
// for OCCT to extrude. We sample uniformly in arc parameter (angle)
// regardless of arc sweep size; tiny arcs end up with tightly-spaced
// samples and large arcs with widely-spaced ones, which is fine.
constexpr int kArcSampleSegments = 16;
constexpr int kCircleProfileSampleSegments = 64;
// Local copy of pi — `M_PI` is non-standard (POSIX-only) and
// `std::numbers::pi` is C++20-only. Defining it here keeps this file
// portable without committing the rest of the codebase to either.
constexpr double kPi = 3.14159265358979323846;

bool nearly_equal(double left, double right) {
  return std::abs(left - right) <= kProfileTolerance;
}

bool points_match(const SketchProfilePoint& left, const SketchProfilePoint& right) {
  return nearly_equal(left.x, right.x) && nearly_equal(left.y, right.y);
}

long long quantize_coordinate(double value) {
  return std::llround(value / kProfileTolerance);
}

std::string make_node_key(const SketchProfilePoint& point) {
  return std::to_string(quantize_coordinate(point.x)) + ":" +
         std::to_string(quantize_coordinate(point.y));
}

double polygon_signed_area(const std::vector<SketchProfilePoint>& points) {
  if (points.size() < 3) {
    return 0.0;
  }

  double area = 0.0;
  for (size_t index = 0; index < points.size(); ++index) {
    const auto& current = points[index];
    const auto& next = points[(index + 1) % points.size()];
    area += current.x * next.y - next.x * current.y;
  }

  return area * 0.5;
}

std::string make_polygon_profile_id(const std::vector<std::string>& edge_ids) {
  std::vector<std::string> sorted_ids = edge_ids;
  std::sort(sorted_ids.begin(), sorted_ids.end());

  std::ostringstream stream;
  stream << "profile-poly";
  for (const auto& id : sorted_ids) {
    stream << "-" << id;
  }
  return stream.str();
}
