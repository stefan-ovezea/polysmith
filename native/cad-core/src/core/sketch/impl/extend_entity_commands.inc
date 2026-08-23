// ================================================================
// Extend tool: extend_sketch_entity(entity_id, click_x, click_y)
//
// Extends a line (along its infinite support) or an arc (along its
// full circle) from the end nearest the click to the nearest
// intersection with another non-construction entity. Non-parametric,
// like trim: the endpoint moves, and the recompute re-derives
// vertices/dimensions/profiles.
//
// Unlike trim, extend preserves the line's H/V constraint and
// dimensions — the direction is unchanged, so both stay valid and
// re-measure on the recompute. Arc angle dimensions flip to driven
// (extension changes the sweep); arc radius dimensions survive (the
// radius is preserved).
//
// The intersection helpers below reuse the trim math with the
// TARGET-side clipping relaxed: for line targets `param_on_target`
// is the raw t along start->end (may lie outside [0,1]); for arc
// targets it is the angle on the arc's circle. The OTHER entity
// keeps its finite-segment / angular-sweep clipping.
//
// Include AFTER trim_entity_commands.inc — this file relies on the
// trim helpers being in scope (TrimIntersection, extend_wrap_angle,
// extend_angle_in_sweep, points_match, find_coincident_endpoint).
// ================================================================

namespace {
constexpr double kExtendEpsilon = 1.0e-6;

constexpr double kExtendPi = 3.14159265358979323846;

// Local copies of the trim engine's TU-local angle helpers
// (trim_engine.cpp owns wrap_angle / angle_in_arc_sweep; sketch_feature.cpp
// cannot see them). M_PI is replaced by kExtendPi — this TU does not
// define _USE_MATH_DEFINES.
double extend_wrap_angle(double a) {
  a = std::fmod(a, 2.0 * kExtendPi);
  if (a < 0.0) a += 2.0 * kExtendPi;
  return a;
}

bool extend_angle_in_sweep(double a, double a_start, double a_end, bool ccw) {
  a = extend_wrap_angle(a);
  a_start = extend_wrap_angle(a_start);
  a_end = extend_wrap_angle(a_end);
  if (ccw) {
    if (a_end <= a_start) a_end += 2.0 * kExtendPi;
    if (a < a_start) a += 2.0 * kExtendPi;
    return a >= a_start - 1e-12 && a <= a_end + 1e-12;
  }
  if (a_start <= a_end) a_start += 2.0 * kExtendPi;
  if (a < a_end) a += 2.0 * kExtendPi;
  return a <= a_start + 1e-12 && a >= a_end - 1e-12;
}



double extend_ccw_dist(double from, double to) {
  return std::fmod(to - from + 2.0 * kExtendPi, 2.0 * kExtendPi);
}

std::pair<double, double> extend_arc_angles(const SketchArc& arc) {
  return {
      extend_wrap_angle(
          std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x)),
      extend_wrap_angle(
          std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x)),
  };
}

// ── Line targets (infinite support) ────────────────────────────

std::optional<TrimIntersection> extend_line_line(
    const SketchLine& target, const SketchLine& other) {
  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.start_x,  cy = other.start_y;
  const double dx = other.end_x,    dy = other.end_y;

  const double abx = bx - ax, aby = by - ay;
  const double cdx = dx - cx, cdy = dy - cy;
  const double denom = abx * cdy - aby * cdx;
  if (std::abs(denom) < kTrimCoincidentTolerance) {
    return std::nullopt;
  }

  const double acx = cx - ax, acy = cy - ay;
  const double t = (acx * cdy - acy * cdx) / denom;
  const double u = (acx * aby - acy * abx) / denom;
  // The target side is infinite; the other line keeps its segment.
  if (u < -kExtendEpsilon || u > 1.0 + kExtendEpsilon) {
    return std::nullopt;
  }

  return TrimIntersection{
      .x = ax + t * abx,
      .y = ay + t * aby,
      .param_on_target = t,
      .param_on_other = std::clamp(u, 0.0, 1.0),
      .other_entity_id = other.id,
  };
}

std::vector<TrimIntersection> extend_line_circle(
    const SketchLine& target, const SketchCircle& other) {
  std::vector<TrimIntersection> result;

  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.center_x, cy = other.center_y;
  const double r = other.radius;

  const double abx = bx - ax;
  const double aby = by - ay;
  const double a = abx * abx + aby * aby;
  const double dx = ax - cx;
  const double dy = ay - cy;
  const double b = 2.0 * (dx * abx + dy * aby);
  const double c_val = dx * dx + dy * dy - r * r;

  if (a < kExtendEpsilon * kExtendEpsilon) {
    return result;
  }
  const double disc = b * b - 4.0 * a * c_val;
  if (disc < -kExtendEpsilon) {
    return result;
  }
  const double sqrt_disc = disc <= 0.0 ? 0.0 : std::sqrt(disc);
  const double inv_2a = 1.0 / (2.0 * a);
  for (const double t : {-b - sqrt_disc, -b + sqrt_disc}) {
    const double tc = t * inv_2a;
    const double ix = ax + tc * abx;
    const double iy = ay + tc * aby;
    result.push_back(TrimIntersection{
        .x = ix,
        .y = iy,
        .param_on_target = tc,
        .param_on_other = extend_wrap_angle(std::atan2(iy - cy, ix - cx)),
        .other_entity_id = other.id,
    });
  }
  return result;
}

std::vector<TrimIntersection> extend_line_arc(
    const SketchLine& target, const SketchArc& other) {
  // Line vs the arc's full circle, filtered to the arc's sweep.
  std::vector<TrimIntersection> result;
  const double ax = target.start_x, ay = target.start_y;
  const double bx = target.end_x,   by = target.end_y;
  const double cx = other.center_x, cy = other.center_y;
  const double r = other.radius;

  const double abx = bx - ax;
  const double aby = by - ay;
  const double a = abx * abx + aby * aby;
  const double dx = ax - cx;
  const double dy = ay - cy;
  const double b = 2.0 * (dx * abx + dy * aby);
  const double c_val = dx * dx + dy * dy - r * r;

  if (a < kExtendEpsilon * kExtendEpsilon) {
    return result;
  }
  const double disc = b * b - 4.0 * a * c_val;
  if (disc < -kExtendEpsilon) {
    return result;
  }
  const double sqrt_disc = disc <= 0.0 ? 0.0 : std::sqrt(disc);
  const double inv_2a = 1.0 / (2.0 * a);
  const auto [a_start, a_end] = extend_arc_angles(other);
  for (const double t : {-b - sqrt_disc, -b + sqrt_disc}) {
    const double tc = t * inv_2a;
    const double ix = ax + tc * abx;
    const double iy = ay + tc * aby;
    const double angle = extend_wrap_angle(std::atan2(iy - cy, ix - cx));
    if (!extend_angle_in_sweep(angle, a_start, a_end, other.ccw)) {
      continue;
    }
    result.push_back(TrimIntersection{
        .x = ix,
        .y = iy,
        .param_on_target = tc,
        .param_on_other = angle,
        .other_entity_id = other.id,
    });
  }
  return result;
}

// ── Arc targets (full circle) ──────────────────────────────────
//
// `param_on_target` carries the intersection angle on the target's
// circle; `param_on_other` the parameter on the other entity.

std::vector<TrimIntersection> extend_arc_line(
    const SketchArc& target, const SketchLine& other) {
  // Target's full circle vs the other line's finite segment.
  std::vector<TrimIntersection> result;
  const double cx = target.center_x, cy = target.center_y;
  const double r = target.radius;
  const double ax = other.start_x, ay = other.start_y;
  const double abx = other.end_x - other.start_x;
  const double aby = other.end_y - other.start_y;

  const double a = abx * abx + aby * aby;
  const double dx = ax - cx;
  const double dy = ay - cy;
  const double b = 2.0 * (dx * abx + dy * aby);
  const double c_val = dx * dx + dy * dy - r * r;

  if (a < kExtendEpsilon * kExtendEpsilon) {
    return result;
  }
  const double disc = b * b - 4.0 * a * c_val;
  if (disc < -kExtendEpsilon) {
    return result;
  }
  const double sqrt_disc = disc <= 0.0 ? 0.0 : std::sqrt(disc);
  const double inv_2a = 1.0 / (2.0 * a);
  for (const double t : {-b - sqrt_disc, -b + sqrt_disc}) {
    const double u = t * inv_2a;
    if (u < -kExtendEpsilon || u > 1.0 + kExtendEpsilon) {
      continue;
    }
    const double ix = ax + std::clamp(u, 0.0, 1.0) * abx;
    const double iy = ay + std::clamp(u, 0.0, 1.0) * aby;
    result.push_back(TrimIntersection{
        .x = ix,
        .y = iy,
        .param_on_target = extend_wrap_angle(std::atan2(iy - cy, ix - cx)),
        .param_on_other = std::clamp(u, 0.0, 1.0),
        .other_entity_id = other.id,
    });
  }
  return result;
}

// `other` is either a SketchCircle or a SketchArc — both carry
// center_x / center_y / radius / id with identical names.
template <typename T>
std::vector<TrimIntersection> extend_arc_circle(
    const SketchArc& target, const T& other) {
  // Full circle vs full circle (mirrors intersect_circle_circle).
  std::vector<TrimIntersection> result;
  const double cx1 = target.center_x, cy1 = target.center_y;
  const double r1 = target.radius;
  const double cx2 = other.center_x, cy2 = other.center_y;
  const double r2 = other.radius;

  const double dx = cx2 - cx1, dy = cy2 - cy1;
  const double d = std::sqrt(dx * dx + dy * dy);
  if (d > r1 + r2 + kTrimCoincidentTolerance ||
      d < std::abs(r1 - r2) - kTrimCoincidentTolerance) {
    return result;
  }
  if (d < kExtendEpsilon) {
    return result;  // concentric — no extension target
  }

  const double a_val = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
  const double h_sq = r1 * r1 - a_val * a_val;
  const double h = h_sq <= 0.0 ? 0.0 : std::sqrt(h_sq);
  const double px = cx1 + a_val * dx / d;
  const double py = cy1 + a_val * dy / d;
  const double hx = -dy * h / d;
  const double hy = dx * h / d;

  for (const auto [ox, oy] : {std::pair{px + hx, py + hy},
                              std::pair{px - hx, py - hy}}) {
    result.push_back(TrimIntersection{
        .x = ox,
        .y = oy,
        .param_on_target = extend_wrap_angle(std::atan2(oy - cy1, ox - cx1)),
        .param_on_other =
            extend_wrap_angle(std::atan2(oy - cy2, ox - cx2)),
        .other_entity_id = other.id,
    });
  }
  // Tangent (h == 0): two identical points — dedup.
  if (result.size() == 2) {
    const double dxp = result[0].x - result[1].x;
    const double dyp = result[0].y - result[1].y;
    if (std::sqrt(dxp * dxp + dyp * dyp) < kTrimCoincidentTolerance) {
      result.pop_back();
    }
  }
  return result;
}

std::vector<TrimIntersection> extend_arc_arc(
    const SketchArc& target, const SketchArc& other) {
  // Full circle vs the other arc's sweep.
  std::vector<TrimIntersection> all = extend_arc_circle(target, other);
  std::vector<TrimIntersection> result;
  const auto [oa_start, oa_end] = extend_arc_angles(other);
  for (auto& is : all) {
    if (extend_angle_in_sweep(is.param_on_other, oa_start, oa_end, other.ccw)) {
      result.push_back(std::move(is));
    }
  }
  return result;
}

// True when the intersection parameter lies beyond the chosen
// endpoint in the extension direction (t > 1 past the end, t < 0
// before the start).
bool extend_beyond_side(const TrimIntersection& is, bool extend_end) {
  return extend_end ? is.param_on_target > 1.0 + kExtendEpsilon
                    : is.param_on_target < -kExtendEpsilon;
}

// True when the angle lies beyond the chosen arc endpoint in the
// outward direction (continuing the sweep at the end, going
// backwards from the start). The bound is the arc's free opening —
// extending past the opening would run into the arc's own start
// point (major arcs have openings smaller than a half-circle).
bool extend_angle_beyond_side(double angle, const SketchArc& arc,
                              bool extend_end) {
  const auto [a_start, a_end] = extend_arc_angles(arc);
  const double opening =
      arc.ccw ? extend_ccw_dist(a_end, a_start)
              : extend_ccw_dist(a_start, a_end);
  if (extend_end) {
    const double dist = arc.ccw ? extend_ccw_dist(a_end, angle)
                                : extend_ccw_dist(angle, a_end);
    return dist > kExtendEpsilon && dist < opening - kExtendEpsilon;
  }
  const double dist = arc.ccw ? extend_ccw_dist(angle, a_start)
                              : extend_ccw_dist(a_start, angle);
  return dist > kExtendEpsilon && dist < opening - kExtendEpsilon;
}

}  // namespace

void extend_sketch_entity(FeatureEntry& feature,
                          const std::string& entity_id,
                          double click_x,
                          double click_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can extend entities");
  }

  auto& params = *feature.sketch_parameters;

  // Generated geometry (text glyphs, slot outlines, fillet/chamfer
  // lines) is derived — extending it would just be overwritten by
  // the expansion pass.
  ensure_user_editable_entity(params, entity_id);

  // ── Line target ──────────────────────────────────────────────
  const auto line_it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });

  if (line_it != params.lines.end()) {
    if (line_it->is_construction) {
      throw std::runtime_error(
          "Sketch extend: construction lines cannot be extended");
    }
    const double dir_x = line_it->end_x - line_it->start_x;
    const double dir_y = line_it->end_y - line_it->start_y;
    const double len2 = dir_x * dir_x + dir_y * dir_y;
    if (len2 <= kExtendEpsilon * kExtendEpsilon) {
      throw std::runtime_error("Sketch extend: line has zero length");
    }
    // Nearest-end rule: the click's projection along the line picks
    // the end to extend.
    const double s = ((click_x - line_it->start_x) * dir_x +
                      (click_y - line_it->start_y) * dir_y) /
                     len2;
    const bool extend_end = s > 0.5;

    std::vector<TrimIntersection> candidates;
    for (const auto& other : params.lines) {
      if (other.id == line_it->id || other.is_construction) continue;
      auto is = extend_line_line(*line_it, other);
      if (is.has_value() && extend_beyond_side(*is, extend_end)) {
        candidates.push_back(std::move(*is));
      }
    }
    for (const auto& circle : params.circles) {
      if (circle.is_construction) continue;
      for (auto& is : extend_line_circle(*line_it, circle)) {
        if (extend_beyond_side(is, extend_end)) {
          candidates.push_back(std::move(is));
        }
      }
    }
    for (const auto& arc : params.arcs) {
      if (arc.is_construction) continue;
      for (auto& is : extend_line_arc(*line_it, arc)) {
        if (extend_beyond_side(is, extend_end)) {
          candidates.push_back(std::move(is));
        }
      }
    }

    if (candidates.empty()) {
      throw std::runtime_error(
          "Sketch extend: no intersection found in the extension direction");
    }

    const double ex = extend_end ? line_it->end_x : line_it->start_x;
    const double ey = extend_end ? line_it->end_y : line_it->start_y;
    const auto nearest = *std::min_element(
        candidates.begin(), candidates.end(),
        [&](const TrimIntersection& a, const TrimIntersection& b) {
          const double da = (a.x - ex) * (a.x - ex) + (a.y - ey) * (a.y - ey);
          const double db = (b.x - ex) * (b.x - ex) + (b.y - ey) * (b.y - ey);
          return da < db;
        });

    const std::string old_sp_id = line_it->start_vertex_id;
    const std::string old_ep_id = line_it->end_vertex_id;
    if (extend_end) {
      line_it->end_x = nearest.x;
      line_it->end_y = nearest.y;
    } else {
      line_it->start_x = nearest.x;
      line_it->start_y = nearest.y;
    }
    if (points_match(line_it->start_x, line_it->start_y,
                     line_it->end_x, line_it->end_y)) {
      throw std::runtime_error(
          "Sketch extend: intersection coincides with the far endpoint");
    }

    // Adopt a coincident vertex id when the extension lands on an
    // existing endpoint, and re-resolve other entities that shared
    // the old endpoint ids (mirror the trim split's rebind).
#include "core/sketch/impl/trim_line_point_rebind.inc"
    return;
  }

  // ── Arc target ───────────────────────────────────────────────
  const auto arc_it = std::find_if(
      params.arcs.begin(), params.arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });

  if (arc_it != params.arcs.end()) {
    if (arc_it->is_construction) {
      throw std::runtime_error(
          "Sketch extend: construction arcs cannot be extended");
    }
    if (arc_it->radius <= kExtendEpsilon) {
      throw std::runtime_error("Sketch extend: arc has zero radius");
    }
    // Nearest-end rule on the raw click distance.
    const double d_start = std::hypot(click_x - arc_it->start_x,
                                      click_y - arc_it->start_y);
    const double d_end = std::hypot(click_x - arc_it->end_x,
                                    click_y - arc_it->end_y);
    const bool extend_end = d_end <= d_start;

    std::vector<TrimIntersection> candidates;
    for (const auto& line : params.lines) {
      if (line.is_construction) continue;
      for (auto& is : extend_arc_line(*arc_it, line)) {
        if (extend_angle_beyond_side(is.param_on_target, *arc_it,
                                     extend_end)) {
          candidates.push_back(std::move(is));
        }
      }
    }
    for (const auto& circle : params.circles) {
      if (circle.is_construction) continue;
      for (auto& is : extend_arc_circle(*arc_it, circle)) {
        if (extend_angle_beyond_side(is.param_on_target, *arc_it,
                                     extend_end)) {
          candidates.push_back(std::move(is));
        }
      }
    }
    for (const auto& other : params.arcs) {
      if (other.id == arc_it->id || other.is_construction) continue;
      for (auto& is : extend_arc_arc(*arc_it, other)) {
        if (extend_angle_beyond_side(is.param_on_target, *arc_it,
                                     extend_end)) {
          candidates.push_back(std::move(is));
        }
      }
    }

    if (candidates.empty()) {
      throw std::runtime_error(
          "Sketch extend: no intersection found in the extension direction");
    }

    // Nearest by outward angular distance from the chosen endpoint.
    const auto [sa, ea] = extend_arc_angles(*arc_it);
    const double endpoint_angle = extend_end ? ea : sa;
    const bool measure_ccw = arc_it->ccw == extend_end;
    const auto nearest = *std::min_element(
        candidates.begin(), candidates.end(),
        [&](const TrimIntersection& a, const TrimIntersection& b) {
          const double da =
              measure_ccw
                  ? extend_ccw_dist(endpoint_angle, a.param_on_target)
                  : extend_ccw_dist(a.param_on_target, endpoint_angle);
          const double db =
              measure_ccw
                  ? extend_ccw_dist(endpoint_angle, b.param_on_target)
                  : extend_ccw_dist(b.param_on_target, endpoint_angle);
          return da < db;
        });

    auto arc_shared_pt = [&](double x, double y, const std::string& fallback) {
      const auto shared = find_coincident_endpoint(params, entity_id, x, y);
      return shared.has_value() ? std::get<0>(shared.value()) : fallback;
    };
    if (extend_end) {
      arc_it->end_x = nearest.x;
      arc_it->end_y = nearest.y;
      arc_it->end_vertex_id = arc_shared_pt(
          arc_it->end_x, arc_it->end_y,
          "vertex-" + std::to_string(params.next_vertex_index++));
    } else {
      arc_it->start_x = nearest.x;
      arc_it->start_y = nearest.y;
      arc_it->start_vertex_id = arc_shared_pt(
          arc_it->start_x, arc_it->start_y,
          "vertex-" + std::to_string(params.next_vertex_index++));
    }

    // Extension changes the sweep but not the radius: flip any
    // driving angle dimension on the arc to driven so the
    // enforcement doesn't undo the extension (mirrors the trim's
    // delete-the-dims contract, kept friendlier as reference dims).
    for (auto& dim : params.dimensions) {
      if (dim.entity_id == entity_id && dim.kind == "arc_angle") {
        dim.driven = true;
      }
    }
    return;
  }

  throw std::runtime_error(
      "Sketch extend: entity not found or not extendable (lines and arcs "
      "only)");
}
