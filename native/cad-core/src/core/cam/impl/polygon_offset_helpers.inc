// ── Shared 2D polygon helpers ────────────────────────────────────
//
// Used by the laser kerf offset and the face-milling inset.  Works in
// plain (x, y) doubles so both generators can feed it sketch-local or
// world-XY points interchangeably.

namespace {

struct XY {
  double x;
  double y;
};

constexpr double kOffsetEps = 1e-9;

double xy_length(double dx, double dy) {
  return std::hypot(dx, dy);
}

// Signed shoelace area (positive = counter-clockwise walk).
double xy_signed_area(const std::vector<XY>& points) {
  if (points.size() < 3) {
    return 0.0;
  }
  double twiceArea = 0.0;
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1) % points.size()];
    twiceArea += a.x * b.y - b.x * a.y;
  }
  return twiceArea / 2.0;
}

XY xy_centroid(const std::vector<XY>& points) {
  XY c{0.0, 0.0};
  if (points.empty()) {
    return c;
  }
  for (const auto& p : points) {
    c.x += p.x;
    c.y += p.y;
  }
  c.x /= points.size();
  c.y /= points.size();
  return c;
}

// Unit normal pointing RIGHT of the direction (dx, dy): (dy, -dx)/len.
// Rotating a direction by -90° (clockwise).  With the loop normalized
// so material is on the left of the walk, right is always the scrap
// side.
XY right_normal(double dx, double dy) {
  const double length = xy_length(dx, dy);
  if (length < kOffsetEps) {
    return {0.0, 0.0};
  }
  return {dy / length, -dx / length};
}

// Proper intersection of segments (a1,a2) and (b1,b2): shared
// endpoints do NOT count (offset corners legitimately touch).
bool xy_segments_intersect(const XY& a1, const XY& a2, const XY& b1,
                           const XY& b2) {
  const double dax = a2.x - a1.x;
  const double day = a2.y - a1.y;
  const double dbx = b2.x - b1.x;
  const double dby = b2.y - b1.y;
  const double denom = dax * dby - day * dbx;
  if (std::abs(denom) < kOffsetEps) {
    return false;  // parallel
  }
  const double t = ((b1.x - a1.x) * dby - (b1.y - a1.y) * dbx) / denom;
  const double u = ((b1.x - a1.x) * day - (b1.y - a1.y) * dax) / denom;
  // Strictly interior on both sides — touching endpoints is expected
  // at loop closures and corners.
  return t > 1e-9 && t < 1.0 - 1e-9 && u > 1e-9 && u < 1.0 - 1e-9;
}

}  // namespace
