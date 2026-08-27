// ================================================================
// intersect_line_line
// ================================================================

std::optional<TrimIntersection> intersect_line_line(
    const SketchLine& target,
    const SketchLine& other) {
  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.start_x,  cy = other.start_y;
  const double dx = other.end_x,    dy = other.end_y;

  const double abx = bx - ax, aby = by - ay;
  const double cdx = dx - cx, cdy = dy - cy;
  const double denom = abx * cdy - aby * cdx;

  // Parallelism is a question about DIRECTION, so the test has to be
  // relative. `denom` is a cross product in mm²; comparing it against
  // kTrimCoincidentTolerance (0.01 mm) made the answer depend on how
  // long the two lines happen to be — a pair of 0.05 mm lines meeting
  // at a right angle scored 0.0025 and was reported "parallel", while
  // two nearly-parallel metre-long lines sailed through. Normalising by
  // both lengths turns this back into the sine of the angle between
  // them, which is what we actually mean.
  const double ab_len = std::sqrt(abx * abx + aby * aby);
  const double cd_len = std::sqrt(cdx * cdx + cdy * cdy);
  if (ab_len <= 0.0 || cd_len <= 0.0) {
    return std::nullopt;
  }
  constexpr double kParallelSinEps = 1e-12;
  if (std::abs(denom) <= kParallelSinEps * ab_len * cd_len) {
    return std::nullopt;
  }

  const double acx = cx - ax, acy = cy - ay;
  const double t = (acx * cdy - acy * cdx) / denom;
  const double u = (acx * aby - acy * abx) / denom;

  constexpr double kEps = 1e-12;
  if (t < -kEps || t > 1.0 + kEps || u < -kEps || u > 1.0 + kEps) {
    return std::nullopt;
  }

  return TrimIntersection{
      .x = ax + t * abx,
      .y = ay + t * aby,
      .param_on_target = std::clamp(t, 0.0, 1.0),
      .param_on_other = std::clamp(u, 0.0, 1.0),
      .other_entity_id = other.id,
  };
}

// ================================================================
// intersect_line_circle
// ================================================================

std::vector<TrimIntersection> intersect_line_circle(
    const SketchLine& target,
    const SketchCircle& other) {
  std::vector<TrimIntersection> result;

  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.center_x, cy = other.center_y;
  const double r = other.radius;

  // Vector from target start to circle center.
  const double dx = ax - cx;
  const double dy = ay - cy;

  // Line direction (AB).
  const double abx = bx - ax;
  const double aby = by - ay;

  // Quadratic: a*t² + b*t + c = 0  where t is [0,1] along AB.
  const double a = abx * abx + aby * aby;
  const double b = 2.0 * (dx * abx + dy * aby);
  const double c_val = dx * dx + dy * dy - r * r;

  if (a < kTrimCoincidentTolerance * kTrimCoincidentTolerance) {
    // Degenerate line — no intersection.
    return result;
  }

  const double disc = b * b - 4.0 * a * c_val;
  if (disc < -kTrimCoincidentTolerance) {
    return result;
  }

  const double sqrt_disc = disc <= 0.0 ? 0.0 : std::sqrt(disc);
  const double inv_2a = 1.0 / (2.0 * a);
  const double t1 = (-b - sqrt_disc) * inv_2a;
  const double t2 = (-b + sqrt_disc) * inv_2a;

  constexpr double kEps = 1e-12;

  for (double t : {t1, t2}) {
    if (t >= -kEps && t <= 1.0 + kEps) {
      const double tc = std::clamp(t, 0.0, 1.0);
      const double ix = ax + tc * abx;
      const double iy = ay + tc * aby;
      const double angle = wrap_angle(std::atan2(iy - cy, ix - cx));

      result.push_back(TrimIntersection{
          .x = ix,
          .y = iy,
          .param_on_target = tc,
          .param_on_other = angle,
          .other_entity_id = other.id,
      });
    }
  }

  return result;
}

// ================================================================
// intersect_circle_line
// ================================================================

std::vector<TrimIntersection> intersect_circle_line(
    const SketchCircle& target,
    const SketchLine& other) {
  // Swap: call intersect_line_circle and swap param fields.
  auto results = intersect_line_circle(other, target);
  for (auto& r : results) {
    std::swap(r.param_on_target, r.param_on_other);
    r.other_entity_id = other.id;
  }
  return results;
}

// ================================================================
// intersect_circle_circle
// ================================================================

std::vector<TrimIntersection> intersect_circle_circle(
    const SketchCircle& target,
    const SketchCircle& other) {
  std::vector<TrimIntersection> result;

  const double cx1 = target.center_x, cy1 = target.center_y;
  const double r1 = target.radius;
  const double cx2 = other.center_x, cy2 = other.center_y;
  const double r2 = other.radius;

  const double dx = cx2 - cx1;
  const double dy = cy2 - cy1;
  const double d = std::sqrt(dx * dx + dy * dy);

  // No intersection: circles too far apart or one inside the other.
  if (d > r1 + r2 + kTrimCoincidentTolerance ||
      d < std::abs(r1 - r2) - kTrimCoincidentTolerance) {
    return result;
  }

  // Concentric circles have no discrete intersections, and dividing by
  // `d` below would be a division by zero. The containment test above
  // catches the different-radius case, but two circles with the SAME
  // radius and the same centre slip through it: a_val becomes 0/0, the
  // NaN propagates into param_on_target, and sorting on a NaN
  // comparator breaks strict weak ordering (undefined behaviour that
  // can corrupt the whole result vector). Centres within the
  // coincidence tolerance are the same point by this project's own
  // definition, so bail out before the division either way.
  if (d <= kTrimCoincidentTolerance) {
    if (std::abs(r1 - r2) <= kTrimCoincidentTolerance) {
      log_warn("trim",
               "coincident circles '" + target.id + "' and '" + other.id +
                   "' have no discrete intersections; ignoring as a cutting "
                   "edge");
    }
    return result;
  }

  // Distance from circle 1 center to the line connecting intersection points.
  const double a_val = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
  const double h_sq = r1 * r1 - a_val * a_val;
  const double h = h_sq <= 0.0 ? 0.0 : std::sqrt(h_sq);

  const double px = cx1 + a_val * dx / d;
  const double py = cy1 + a_val * dy / d;

  // Two intersection points (or one for tangent).
  const double hx = -dy * h / d;
  const double hy =  dx * h / d;

  const std::pair<double, double> pts[2] = {
      {px + hx, py + hy},
      {px - hx, py - hy},
  };

  for (const auto& [ix, iy] : pts) {
    const double angle = wrap_angle(std::atan2(iy - cy1, ix - cx1));
    const double other_angle = wrap_angle(std::atan2(iy - cy2, ix - cx2));
    result.push_back(TrimIntersection{
        .x = ix,
        .y = iy,
        .param_on_target = angle,
        .param_on_other = other_angle,
        .other_entity_id = other.id,
    });
  }

  // If h == 0 (tangent), we have two identical points. Dedup.
  if (result.size() == 2) {
    const double dxp = result[0].x - result[1].x;
    const double dyp = result[0].y - result[1].y;
    if (std::sqrt(dxp * dxp + dyp * dyp) < kTrimCoincidentTolerance) {
      result.pop_back();
    }
  }

  return result;
}
