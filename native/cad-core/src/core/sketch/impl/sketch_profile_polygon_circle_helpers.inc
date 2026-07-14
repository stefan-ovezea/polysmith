double cross(double ax, double ay, double bx, double by) {
  return ax * by - ay * bx;
}

double segment_parameter(const LineSegment& segment,
                         const SketchProfilePoint& point) {
  const double dx = segment.end.x - segment.start.x;
  const double dy = segment.end.y - segment.start.y;
  const double length_sq = dx * dx + dy * dy;
  if (length_sq <= kProfileTolerance * kProfileTolerance) {
    return 0.0;
  }
  return ((point.x - segment.start.x) * dx + (point.y - segment.start.y) * dy) /
         length_sq;
}

std::optional<SketchProfilePoint> line_intersection(
    const LineSegment& left, const LineSegment& right) {
  const double px = left.start.x;
  const double py = left.start.y;
  const double rx = left.end.x - left.start.x;
  const double ry = left.end.y - left.start.y;
  const double qx = right.start.x;
  const double qy = right.start.y;
  const double sx = right.end.x - right.start.x;
  const double sy = right.end.y - right.start.y;

  const double denominator = cross(rx, ry, sx, sy);
  if (std::abs(denominator) <= kProfileTolerance) {
    return std::nullopt;
  }

  const double qpx = qx - px;
  const double qpy = qy - py;
  const double t = cross(qpx, qpy, sx, sy) / denominator;
  const double u = cross(qpx, qpy, rx, ry) / denominator;
  if (t < -kProfileTolerance || t > 1.0 + kProfileTolerance ||
      u < -kProfileTolerance || u > 1.0 + kProfileTolerance) {
    return std::nullopt;
  }

  return SketchProfilePoint{.x = px + rx * t, .y = py + ry * t};
}

bool point_in_polygon(const SketchProfilePoint& point,
                      const std::vector<SketchProfilePoint>& polygon) {
  bool inside = false;
  for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const auto& a = polygon[i];
    const auto& b = polygon[j];
    const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                         (point.x < (b.x - a.x) * (point.y - a.y) /
                                            (b.y - a.y) +
                                        a.x);
    if (crosses) {
      inside = !inside;
    }
  }
  return inside;
}

SketchProfilePoint polygon_centroid(
    const std::vector<SketchProfilePoint>& polygon) {
  if (polygon.empty()) {
    return SketchProfilePoint{.x = 0.0, .y = 0.0};
  }

  // Use the geometric (area-weighted) centroid rather than the vertex
  // average.  The vertex average is biased toward densely-sampled
  // edges (e.g. a circle arc in a wedge profile) and can land outside
  // the polygon's interior, breaking the nesting logic in
  // apply_nested_polygon_holes.
  double signed_area = 0.0;
  double cx = 0.0;
  double cy = 0.0;

  for (size_t i = 0; i < polygon.size(); ++i) {
    const size_t j = (i + 1) % polygon.size();
    const double cross =
        polygon[i].x * polygon[j].y - polygon[j].x * polygon[i].y;
    signed_area += cross;
    cx += (polygon[i].x + polygon[j].x) * cross;
    cy += (polygon[i].y + polygon[j].y) * cross;
  }

  // Fall back to vertex average if the polygon is degenerate (zero or
  // near-zero area).
  if (std::abs(signed_area) <= kProfileTolerance * kProfileTolerance) {
    double x = 0.0;
    double y = 0.0;
    for (const auto& point : polygon) {
      x += point.x;
      y += point.y;
    }
    const double count = static_cast<double>(polygon.size());
    return SketchProfilePoint{.x = x / count, .y = y / count};
  }

  const double factor = 1.0 / (3.0 * signed_area);
  return SketchProfilePoint{.x = cx * factor, .y = cy * factor};
}

bool circle_contains_circle(const SketchCircle& outer,
                            const SketchCircle& inner) {
  if (outer.id == inner.id ||
      outer.radius <= inner.radius + kProfileTolerance) {
    return false;
  }
  const double dx = inner.center_x - outer.center_x;
  const double dy = inner.center_y - outer.center_y;
  const double center_distance = std::sqrt(dx * dx + dy * dy);
  return center_distance + inner.radius <= outer.radius - kProfileTolerance;
}

double circle_area(const SketchCircle& circle) {
  return kPi * circle.radius * circle.radius;
}

void apply_nested_polygon_holes(std::vector<SketchProfileRegion>& profiles) {
  for (auto& inner : profiles) {
    if (inner.kind != "polygon" || inner.points.size() < 3) {
      continue;
    }

    const SketchProfilePoint inner_center = polygon_centroid(inner.points);
    const double inner_area = std::abs(polygon_signed_area(inner.points));
    SketchProfileRegion* containing_profile = nullptr;
    double containing_area = std::numeric_limits<double>::max();

    for (auto& candidate : profiles) {
      if (&candidate == &inner || candidate.kind != "polygon" ||
          candidate.points.size() < 3) {
        continue;
      }

      const double candidate_area =
          std::abs(polygon_signed_area(candidate.points));
      if (candidate_area <= inner_area + kProfileTolerance ||
          candidate_area >= containing_area) {
        continue;
      }

      if (point_in_polygon(inner_center, candidate.points)) {
        containing_profile = &candidate;
        containing_area = candidate_area;
      }
    }

    if (containing_profile != nullptr) {
      containing_profile->inner_loops.push_back(inner.points);
      containing_profile->id += "-hole-" + inner.id;
    }
  }
}

std::vector<SketchProfilePoint> sample_circle_loop(const SketchCircle& circle) {
  std::vector<SketchProfilePoint> points;
  points.reserve(kCircleProfileSampleSegments);
  for (int index = 0; index < kCircleProfileSampleSegments; ++index) {
    const double angle =
        (static_cast<double>(index) /
         static_cast<double>(kCircleProfileSampleSegments)) *
        2.0 * kPi;
    points.push_back(SketchProfilePoint{
        .x = circle.center_x + circle.radius * std::cos(angle),
        .y = circle.center_y + circle.radius * std::sin(angle),
    });
  }
  return points;
}

std::vector<LineSegment> solid_line_segments(
    const SketchFeatureParameters& parameters) {
  std::map<std::string, SketchProfilePoint> point_by_id;
  for (const auto& line : parameters.lines) {
    if (!line.start_vertex_id.empty()) {
      point_by_id.emplace(line.start_vertex_id,
                          SketchProfilePoint{.x = line.start_x,
                                             .y = line.start_y});
    }
    if (!line.end_vertex_id.empty()) {
      point_by_id.emplace(line.end_vertex_id,
                          SketchProfilePoint{.x = line.end_x,
                                             .y = line.end_y});
    }
  }

  auto endpoint = [&](const std::string& point_id,
                      double fallback_x,
                      double fallback_y) {
    const auto point_it = point_by_id.find(point_id);
    if (point_it != point_by_id.end()) {
      return point_it->second;
    }
    return SketchProfilePoint{.x = fallback_x, .y = fallback_y};
  };

  std::vector<LineSegment> segments;
  for (const auto& line : parameters.lines) {
    if (line.is_construction) {
      continue;
    }
    const SketchProfilePoint start =
        endpoint(line.start_vertex_id, line.start_x, line.start_y);
    const SketchProfilePoint end =
        endpoint(line.end_vertex_id, line.end_x, line.end_y);
    if (points_match(start, end)) {
      continue;
    }
    segments.push_back(LineSegment{.line_id = line.id, .start = start, .end = end});
  }
  return segments;
}

std::vector<LineSegment> circle_boundary_segments(const SketchCircle& circle) {
  std::vector<LineSegment> segments;
  if (circle.radius <= kProfileTolerance) {
    return segments;
  }
  const auto samples = sample_circle_loop(circle);
  segments.reserve(samples.size());
  for (size_t index = 0; index < samples.size(); ++index) {
    segments.push_back(LineSegment{
        .line_id = circle.id,
        .start = samples[index],
        .end = samples[(index + 1) % samples.size()],
    });
  }
  return segments;
}

bool segment_intersects_circle_boundary(const LineSegment& segment,
                                        const SketchCircle& circle) {
  const double dx = segment.end.x - segment.start.x;
  const double dy = segment.end.y - segment.start.y;
  const double fx = segment.start.x - circle.center_x;
  const double fy = segment.start.y - circle.center_y;
  const double a = dx * dx + dy * dy;
  if (a <= kProfileTolerance * kProfileTolerance) {
    return false;
  }

  const double b = 2.0 * (fx * dx + fy * dy);
  const double c = fx * fx + fy * fy - circle.radius * circle.radius;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < -kProfileTolerance) {
    return false;
  }

  const double sqrt_discriminant = std::sqrt(std::max(0.0, discriminant));
  const double t0 = (-b - sqrt_discriminant) / (2.0 * a);
  const double t1 = (-b + sqrt_discriminant) / (2.0 * a);
  return (t0 >= -kProfileTolerance && t0 <= 1.0 + kProfileTolerance) ||
         (t1 >= -kProfileTolerance && t1 <= 1.0 + kProfileTolerance);
}

bool any_line_intersects_circle_boundary(
    const std::vector<LineSegment>& segments,
    const std::vector<const SketchCircle*>& circles) {
  for (const auto& segment : segments) {
    for (const auto* circle : circles) {
      if (segment_intersects_circle_boundary(segment, *circle)) {
        return true;
      }
    }
  }
  return false;
}

// Split circle boundary segments at line endpoints that lie on the circle
// boundary. Tangent lines have endpoints on the mathematical circle that may
// not intersect any polygon chord (chords are slightly inside the true circle
// arc).  Without splitting, the arrangement graph never connects the line to
// the circle and the line-circle-line wedge region is missed.
void split_circle_segments_at_line_endpoints(
    std::vector<LineSegment>& circle_segments,
    const SketchCircle& circle,
    const std::vector<SketchLine>& lines) {
  if (circle_segments.empty()) {
    return;
  }

  for (const auto& line : lines) {
    if (line.is_construction) {
      continue;
    }

    // Check both endpoints of every solid line.
    const SketchProfilePoint endpoints[2] = {
        {.x = line.start_x, .y = line.start_y},
        {.x = line.end_x, .y = line.end_y}};

    for (const auto& endpoint : endpoints) {
      const double dx = endpoint.x - circle.center_x;
      const double dy = endpoint.y - circle.center_y;
      const double dist = std::sqrt(dx * dx + dy * dy);
      if (std::abs(dist - circle.radius) > kProfileTolerance) {
        continue;
      }

      // Find the circle segment that contains this endpoint.
      // Compute angle and locate the segment whose arc covers it.
      const double angle = std::atan2(dy, dx);
      const double angle_norm =
          angle < 0.0 ? angle + 2.0 * kPi : angle;
      const double seg_width = 2.0 * kPi / circle_segments.size();
      int seg_index =
          static_cast<int>(std::floor(angle_norm / seg_width));
      if (seg_index < 0) seg_index += circle_segments.size();
      if (seg_index >= static_cast<int>(circle_segments.size())) {
        seg_index = 0;
      }

      auto& seg = circle_segments[seg_index];

      // Skip if the endpoint already coincides with a segment vertex.
      if (points_match(endpoint, seg.start) ||
          points_match(endpoint, seg.end)) {
        continue;
      }

      // Split the segment at the endpoint.
      const auto original_end = seg.end;
      seg.end = endpoint;
      circle_segments.push_back(LineSegment{
          .line_id = circle.id,
          .start = endpoint,
          .end = original_end,
      });

      // After the push_back, circle_segments might have reallocated,
      // so the `seg` reference is invalid. That's fine — we only
      // modify one segment per endpoint and break out of the loop
      // for this endpoint.
    }
  }
}
