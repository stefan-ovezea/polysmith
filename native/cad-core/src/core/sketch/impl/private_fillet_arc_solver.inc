// ─────────────────────────────────────────────────────────────────
// Tangent-circle solver for sketch fillets with arc operands
// (line-arc and arc-arc).  Shared by `add_sketch_fillet` (create)
// and `enforce_sketch_fillets` (recompute): both re-derive the
// trim points and the generated fillet arc from the operands'
// current geometry and the fillet radius, so a single solve path
// keeps them in lockstep.
//
// Math (per operand):
//   - Line: the fillet center O must sit on one of the two offset
//     lines at distance r; the trim point is the perpendicular foot
//     of O onto the carrier.  The foot's parameter t must lie
//     strictly inside (0, |far - corner|).
//   - Arc: O must sit on the concentric circle R±r (internal
//     preferred when it exists); the trim point is the radial point
//     Ca + R·unit(O - Ca).  Its angle must lie strictly inside the
//     arc body, measured as the walk-directed angle from the corner
//     (corner at the START endpoint walks toward the far end; at the
//     END endpoint the body runs from far back to the corner, so the
//     direction flips).
//
// Candidates come from intersecting the offset carriers (line×line
// is never reached — the v1 path handles it).  A candidate is valid
// when BOTH trim points fit their operands and are distinct; among
// the valid ones the solve picks the minimal total trim (the fillet
// hugging the corner), which disambiguates wide-sweep arcs.
// `nullopt` means no radius fits — create throws, enforcement
// silently leaves the cached geometry alone (the v1 contract).
// ─────────────────────────────────────────────────────────────────

namespace {

constexpr double kFilletArcSolverAngleEpsilon = 0.001;  // rad
constexpr double kFilletArcSolverOnCircleTolerance = 0.01;
constexpr double kFilletArcSolverDistinctTrimTolerance = 0.01;

// Walk-directed angular distance from `from_angle` to `to_angle`,
// normalized to [0, 2π).  `ccw` = the arc's walk direction.
double fillet_arc_walk_angle(double from_angle, double to_angle,
                             bool ccw) {
  constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
  double delta = ccw ? (to_angle - from_angle) : (from_angle - to_angle);
  delta = std::fmod(delta, kTwoPi);
  if (delta < 0.0) delta += kTwoPi;
  return delta;
}

// One operand of a fillet solve.  Line operands carry the away unit
// vector + far distance; arc operands carry the carrier circle and
// the sweep geometry the fit rule needs.
struct FilletArcSolverOperand {
  bool is_arc = false;
  double corner_x = 0.0;
  double corner_y = 0.0;
  // Line operand:
  double away_x = 0.0;  // unit vector corner → far
  double away_y = 0.0;
  double far_dist = 0.0;  // |far - corner|
  // Arc operand:
  double arc_cx = 0.0;
  double arc_cy = 0.0;
  double arc_radius = 0.0;
  double corner_angle = 0.0;  // angle of the corner around the center
  double sweep_span = 0.0;    // walk distance corner → far along the body
  bool corner_at_start = false;  // which arc endpoint holds the corner
  bool arc_ccw = false;
};

struct FilletArcSolveResult {
  double trim_a_x = 0.0;
  double trim_a_y = 0.0;
  double trim_b_x = 0.0;
  double trim_b_y = 0.0;
  double center_x = 0.0;
  double center_y = 0.0;
  bool ccw = false;
};

// Line operand: corner + far endpoint.  Returns nullopt when the two
// points collapse (zero-length line — rejected before the solve).
std::optional<FilletArcSolverOperand> fillet_line_operand(
    double corner_x, double corner_y, double far_x, double far_y) {
  const double dx = far_x - corner_x;
  const double dy = far_y - corner_y;
  const double len = std::hypot(dx, dy);
  if (len <= kMinimumSketchDimensionValue) {
    return std::nullopt;
  }
  FilletArcSolverOperand operand;
  operand.corner_x = corner_x;
  operand.corner_y = corner_y;
  operand.away_x = dx / len;
  operand.away_y = dy / len;
  operand.far_dist = len;
  return operand;
}

// Arc operand: the live arc entity, the corner position (must lie on
// the carrier circle), the far endpoint coords, and which endpoint
// holds the corner.  Returns nullopt when the corner is off the
// circle (stale geometry — enforcement leaves it alone) or the sweep
// is degenerate.
std::optional<FilletArcSolverOperand> fillet_arc_operand(
    const SketchArc& arc, double corner_x, double corner_y,
    double far_x, double far_y, bool corner_at_start) {
  const double dist_to_center =
      std::hypot(corner_x - arc.center_x, corner_y - arc.center_y);
  if (std::abs(dist_to_center - arc.radius) >
      kFilletArcSolverOnCircleTolerance) {
    return std::nullopt;
  }
  const double corner_angle =
      std::atan2(corner_y - arc.center_y, corner_x - arc.center_x);
  const double far_angle =
      std::atan2(far_y - arc.center_y, far_x - arc.center_x);
  // The arc body runs from its START endpoint to its END endpoint
  // following `ccw`.  Measured as the walk distance from the corner
  // toward the far endpoint *along the body* (which flips when the
  // corner is the END endpoint).
  const double span = corner_at_start
                          ? fillet_arc_walk_angle(corner_angle, far_angle,
                                                  arc.ccw)
                          : fillet_arc_walk_angle(far_angle, corner_angle,
                                                  arc.ccw);
  if (span <= kFilletArcSolverAngleEpsilon) {
    return std::nullopt;
  }
  FilletArcSolverOperand operand;
  operand.is_arc = true;
  operand.corner_x = corner_x;
  operand.corner_y = corner_y;
  operand.arc_cx = arc.center_x;
  operand.arc_cy = arc.center_y;
  operand.arc_radius = arc.radius;
  operand.corner_angle = corner_angle;
  operand.sweep_span = span;
  operand.corner_at_start = corner_at_start;
  operand.arc_ccw = arc.ccw;
  return operand;
}

// Circle×circle intersections (standard construction).  Up to two
// points, deterministic order.  Returns 0 for disjoint/identical
// circles.
std::vector<std::pair<double, double>> fillet_circle_circle_intersections(
    double c1x, double c1y, double r1, double c2x, double c2y, double r2) {
  std::vector<std::pair<double, double>> points;
  const double dx = c2x - c1x;
  const double dy = c2y - c1y;
  const double d = std::hypot(dx, dy);
  if (d <= kMinimumSketchDimensionValue ||
      d > r1 + r2 + kFilletArcSolverOnCircleTolerance ||
      d < std::abs(r1 - r2) - kFilletArcSolverOnCircleTolerance) {
    return points;
  }
  const double a = (d * d + r1 * r1 - r2 * r2) / (2.0 * d);
  const double h2 = r1 * r1 - a * a;
  const double h = h2 > 0.0 ? std::sqrt(h2) : 0.0;
  const double mx = c1x + a * dx / d;
  const double my = c1y + a * dy / d;
  // Perpendicular unit: (-dy, dx)/d.  Both signs, +h first.
  points.emplace_back(mx - h * (-dy / d), my - h * (dx / d));
  points.emplace_back(mx + h * (-dy / d), my + h * (dx / d));
  return points;
}

// Trim-point evaluation for one operand at a candidate center O.
// Returns the trim point, the trim distance from the corner along
// the body (the tie-break cost), and whether the fit rules pass.
struct FilletArcFit {
  double trim_x = 0.0;
  double trim_y = 0.0;
  double trim_len = 0.0;
  bool fits = false;
};

FilletArcFit fillet_fit_operand(const FilletArcSolverOperand& operand,
                                double center_x, double center_y) {
  FilletArcFit fit;
  if (!operand.is_arc) {
    // Perpendicular foot onto the carrier line.
    const double t = (center_x - operand.corner_x) * operand.away_x +
                     (center_y - operand.corner_y) * operand.away_y;
    if (t <= kMinimumSketchDimensionValue ||
        t >= operand.far_dist - kMinimumSketchDimensionValue) {
      return fit;
    }
    fit.trim_x = operand.corner_x + t * operand.away_x;
    fit.trim_y = operand.corner_y + t * operand.away_y;
    fit.trim_len = t;
    fit.fits = true;
    return fit;
  }
  // Radial point of O onto the arc's circle.
  const double vx = center_x - operand.arc_cx;
  const double vy = center_y - operand.arc_cy;
  const double dist = std::hypot(vx, vy);
  if (dist <= kMinimumSketchDimensionValue) {
    return fit;
  }
  const double trim_x = operand.arc_cx + vx * operand.arc_radius / dist;
  const double trim_y = operand.arc_cy + vy * operand.arc_radius / dist;
  const double trim_angle = std::atan2(trim_y - operand.arc_cy,
                                       trim_x - operand.arc_cx);
  // Distance from the corner to the trim angle along the arc body.
  const double s = operand.corner_at_start
                       ? fillet_arc_walk_angle(operand.corner_angle,
                                               trim_angle, operand.arc_ccw)
                       : fillet_arc_walk_angle(trim_angle,
                                               operand.corner_angle,
                                               operand.arc_ccw);
  if (s <= kFilletArcSolverAngleEpsilon ||
      s >= operand.sweep_span - kFilletArcSolverAngleEpsilon) {
    return fit;
  }
  fit.trim_x = trim_x;
  fit.trim_y = trim_y;
  fit.trim_len = s * operand.arc_radius;
  fit.fits = true;
  return fit;
}

// Solve the tangent fillet circle of `radius` between two operands.
// Both operands must be line or arc (exactly one arc minimum — the
// v1 line-line path never reaches this solver).  Returns the trim
// points, the fillet arc center and its ccw flag; nullopt when no
// candidate fits (or the operands are malformed).
std::optional<FilletArcSolveResult> solve_fillet_tangent_circle(
    const FilletArcSolverOperand& operand_a,
    const FilletArcSolverOperand& operand_b, double radius) {
  // Offset carriers: each operand contributes lines or circles at
  // distance `radius` from the carrier curve.  Fixed enumeration
  // order (line: +r then -r; arc: internal then external) makes the
  // minimal-trim tie-break deterministic.
  std::vector<std::pair<double, double>> carriers_a;  // {origin_x, origin_y} for offset lines
  std::vector<std::pair<double, double>> carriers_b;
  std::vector<double> line_dirs_a;  // unit direction for offset lines (paired with carriers_a)
  std::vector<double> line_dirs_b;
  std::vector<std::pair<double, double>> circles_a;  // {cx, cy} — radius paired
  std::vector<double> circle_radii_a;
  std::vector<std::pair<double, double>> circles_b;
  std::vector<double> circle_radii_b;

  const auto add_carriers = [&](const FilletArcSolverOperand& operand,
                                std::vector<std::pair<double, double>>& line_origins,
                                std::vector<double>& line_dirs,
                                std::vector<std::pair<double, double>>& circles,
                                std::vector<double>& circle_radii,
                                double radius) {
    if (!operand.is_arc) {
      // Two offset lines: C + s·n, s = ±radius (n = left normal).
      const double nx = -operand.away_y;
      const double ny = operand.away_x;
      line_origins.emplace_back(operand.corner_x + radius * nx,
                                operand.corner_y + radius * ny);
      line_dirs.push_back(operand.away_x);
      line_dirs.push_back(operand.away_y);
      line_origins.emplace_back(operand.corner_x - radius * nx,
                                operand.corner_y - radius * ny);
      line_dirs.push_back(operand.away_x);
      line_dirs.push_back(operand.away_y);
      return;
    }
    // Concentric circles: internal R-r (when positive), external R+r.
    const double internal = operand.arc_radius - radius;
    if (internal > kMinimumSketchDimensionValue) {
      circles.emplace_back(operand.arc_cx, operand.arc_cy);
      circle_radii.push_back(internal);
    }
    circles.emplace_back(operand.arc_cx, operand.arc_cy);
    circle_radii.push_back(operand.arc_radius + radius);
  };
  add_carriers(operand_a, carriers_a, line_dirs_a, circles_a,
               circle_radii_a, radius);
  add_carriers(operand_b, carriers_b, line_dirs_b, circles_b,
               circle_radii_b, radius);

  // Enumerate candidate centers in fixed order and keep the first
  // minimal-total-trim solution.
  std::optional<FilletArcSolveResult> best;
  double best_total = 0.0;

  const auto evaluate = [&](double cx, double cy) {
    const FilletArcFit fit_a = fillet_fit_operand(operand_a, cx, cy);
    if (!fit_a.fits) return;
    const FilletArcFit fit_b = fillet_fit_operand(operand_b, cx, cy);
    if (!fit_b.fits) return;
    // Distinct trim points (a collapsed fillet would degenerate).
    if (std::hypot(fit_a.trim_x - fit_b.trim_x,
                   fit_a.trim_y - fit_b.trim_y) <=
        kFilletArcSolverDistinctTrimTolerance) {
      return;
    }
    const double total = fit_a.trim_len + fit_b.trim_len;
    if (best.has_value() && !(total < best_total)) return;
    best_total = total;
    FilletArcSolveResult result;
    result.trim_a_x = fit_a.trim_x;
    result.trim_a_y = fit_a.trim_y;
    result.trim_b_x = fit_b.trim_x;
    result.trim_b_y = fit_b.trim_y;
    result.center_x = cx;
    result.center_y = cy;
    // Same convention as the v1 line-line path: positive cross
    // product (trim_a - center) × (trim_b - center) = ccw sweep.
    result.ccw =
        (fit_a.trim_x - cx) * (fit_b.trim_y - cy) -
            (fit_a.trim_y - cy) * (fit_b.trim_x - cx) >
        0.0;
    best = result;
  };

  if (!operand_a.is_arc && !operand_b.is_arc) {
    // Line×line — the v1 path owns this shape; never reached.
    return std::nullopt;
  } else if (operand_a.is_arc && operand_b.is_arc) {
    for (size_t ia = 0; ia < circles_a.size(); ++ia) {
      for (size_t ib = 0; ib < circles_b.size(); ++ib) {
        for (const auto& p : fillet_circle_circle_intersections(
                 circles_a[ia].first, circles_a[ia].second,
                 circle_radii_a[ia], circles_b[ib].first,
                 circles_b[ib].second, circle_radii_b[ib])) {
          evaluate(p.first, p.second);
        }
      }
    }
  } else {
    // Line×arc (either order).  Offset lines of the line operand
    // crossed with offset circles of the arc operand.
    const bool a_is_line = !operand_a.is_arc;
    const auto& line_origins = a_is_line ? carriers_a : carriers_b;
    const auto& line_dirs = a_is_line ? line_dirs_a : line_dirs_b;
    const auto& circles = a_is_line ? circles_b : circles_a;
    const auto& circle_radii = a_is_line ? circle_radii_b : circle_radii_a;
    for (size_t il = 0; il < line_origins.size(); ++il) {
      const double ox = line_origins[il].first;
      const double oy = line_origins[il].second;
      const double ux = line_dirs[2 * il];
      const double uy = line_dirs[2 * il + 1];
      for (size_t ic = 0; ic < circles.size(); ++ic) {
        // |(ox + t·u) - C|² = ρ²  →  t² + 2·(ox-C)·u·t + |ox-C|²-ρ² = 0
        const double px = ox - circles[ic].first;
        const double py = oy - circles[ic].second;
        const double b = 2.0 * (px * ux + py * uy);
        const double c = px * px + py * py - circle_radii[ic] * circle_radii[ic];
        const double disc = b * b - 4.0 * c;
        if (disc < 0.0) continue;
        const double root_disc = std::sqrt(disc);
        evaluate(ox + 0.5 * (-b - root_disc) * ux,
                 oy + 0.5 * (-b - root_disc) * uy);
        evaluate(ox + 0.5 * (-b + root_disc) * ux,
                 oy + 0.5 * (-b + root_disc) * uy);
      }
    }
  }
  return best;
}

// Away tangent at the corner for a parallel-tangent rejection:
// the arc's walk tangent at the corner, flipped when the corner is
// the END endpoint (the body runs backward from the corner).
std::optional<std::pair<double, double>> fillet_arc_away_tangent(
    const FilletArcSolverOperand& operand) {
  if (!operand.is_arc) {
    return std::nullopt;
  }
  const double radial_x =
      (operand.corner_x - operand.arc_cx) / operand.arc_radius;
  const double radial_y =
      (operand.corner_y - operand.arc_cy) / operand.arc_radius;
  // Walk tangent at the corner (direction of increasing sweep).
  double tx = operand.arc_ccw ? -radial_y : radial_y;
  double ty = operand.arc_ccw ? radial_x : -radial_x;
  if (!operand.corner_at_start) {
    tx = -tx;
    ty = -ty;
  }
  return std::pair{tx, ty};
}

}  // namespace
