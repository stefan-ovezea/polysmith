namespace {

constexpr double kPi = 3.14159265358979323846;

// ── 2D point helpers ────────────────────────────────────────────

struct Point2D {
  double x;
  double y;
};

double cross2D(const Point2D &a, const Point2D &b) {
  return a.x * b.y - a.y * b.x;
}

Point2D sub2D(const Point2D &a, const Point2D &b) {
  return {a.x - b.x, a.y - b.y};
}

// Clip a line segment (p1→p2) to a convex polygon using
// Sutherland–Hodgman. Returns the clipped segment endpoints,
// or empty vector if completely outside.
std::vector<Point2D> clipLineToPolygon(Point2D p1, Point2D p2,
                                       const std::vector<Point2D> &poly) {
  std::vector<Point2D> output = {p1, p2};

  for (size_t i = 0; i < poly.size(); ++i) {
    if (output.empty())
      break;
    std::vector<Point2D> input = std::move(output);
    output.clear();

    const Point2D &edgeStart = poly[i];
    const Point2D &edgeEnd = poly[(i + 1) % poly.size()];
    const Point2D edgeVec = sub2D(edgeEnd, edgeStart);

    for (size_t j = 0; j < input.size(); ++j) {
      const Point2D &current = input[j];
      const Point2D &previous = input[(j + input.size() - 1) % input.size()];

      const double dCurrent = cross2D(edgeVec, sub2D(current, edgeStart));
      const double dPrevious = cross2D(edgeVec, sub2D(previous, edgeStart));

      // Current point is inside (left of edge for CCW polygon).
      if (dCurrent >= 0) {
        // Previous was outside → add intersection.
        if (dPrevious < 0) {
          const double t = dPrevious / (dPrevious - dCurrent);
          output.push_back({
              previous.x + t * (current.x - previous.x),
              previous.y + t * (current.y - previous.y),
          });
        }
        output.push_back(current);
      } else if (dPrevious >= 0) {
        // Current is outside, previous was inside → add intersection.
        const double t = dPrevious / (dPrevious - dCurrent);
        output.push_back({
            previous.x + t * (current.x - previous.x),
            previous.y + t * (current.y - previous.y),
        });
      }
    }
  }

  return output;
}

// Sample the boundary of a face as a 2D polygon in world XY space.
// Assumes the face is roughly horizontal (within ~5° of XY plane).
// Returns polygon vertices projected to the face's Z height.
std::vector<Point2D> faceBoundaryPolygonXY(const TopoDS_Face &face,
                                           double faceZ) {
  std::vector<Point2D> poly;

  TopExp_Explorer edgeExp(face, TopAbs_EDGE);
  for (; edgeExp.More(); edgeExp.Next()) {
    const TopoDS_Edge &edge = TopoDS::Edge(edgeExp.Current());
    if (edge.IsNull())
      continue;

    BRepAdaptor_Curve curve(edge);
    const double t0 = curve.FirstParameter();
    const double t1 = curve.LastParameter();

    const int samples = 30;
    for (int k = 0; k < samples; ++k) {
      const double t = t0 + (t1 - t0) * static_cast<double>(k) / samples;
      gp_Pnt pt;
      curve.D0(t, pt);
      poly.push_back({pt.X(), pt.Y()});
    }
  }

  // Remove near-duplicate consecutive points.
  if (poly.size() > 2) {
    std::vector<Point2D> deduped;
    deduped.push_back(poly[0]);
    for (size_t i = 1; i < poly.size(); ++i) {
      const double dx = poly[i].x - deduped.back().x;
      const double dy = poly[i].y - deduped.back().y;
      if (dx * dx + dy * dy > 0.01) {
        deduped.push_back(poly[i]);
      }
    }
    poly = std::move(deduped);
  }

  return poly;
}

} // namespace
