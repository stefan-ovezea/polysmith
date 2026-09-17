// ================================================================
// Corner trim (R2) — SolidWorks-style.
//
// Two picked entities (lines or arcs) are trimmed/extended to their
// VIRTUAL corner: the intersection of their underlying curves
// nearest the click (line support x line support; line support x
// circle; circle x circle). Each entity keeps the portion from its
// far end (the end farther from the click) to the corner. An entity
// whose near end already sits at the corner is left untouched.
//
// This file is self-contained: it defines its own curve-curve
// intersection math (virtual corners are unbounded — the extend
// helpers search finite segments, which is the wrong domain here)
// and only relies on the shared sketch helpers (points_match,
// find_coincident_endpoint, live_sketch_point_ids).
// ================================================================

namespace {

constexpr double kCornerPi = 3.14159265358979323846;
// Below this sweep the corner would collapse the arc (matches the
// arc endpoint angle tolerance used across the trim engine).
constexpr double kCornerAngEps = 1e-6;

// The resolved corner: the intersection point, which end of each
// entity is the NEAR end (moves to the corner), and the resulting
// segment geometry for the preview.
struct CornerResolve {
  bool valid = false;
  double corner_x = 0.0, corner_y = 0.0;
  // Near end flags: true = the START is the near end (moves to the
  // corner; the far end stays).
  bool a_near_is_start = false;
  bool b_near_is_start = false;
};

const SketchLine* corner_find_line(const SketchFeatureParameters& params,
                                   const std::string& id) {
  for (const auto& l : params.lines) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

const SketchArc* corner_find_arc(const SketchFeatureParameters& params,
                                 const std::string& id) {
  for (const auto& a : params.arcs) {
    if (a.id == id) return &a;
  }
  return nullptr;
}

double corner_wrap_angle(double a) {
  a = std::fmod(a, 2.0 * kCornerPi);
  if (a < 0.0) a += 2.0 * kCornerPi;
  return a;
}

// Intersection of two LINE SUPPORTS (infinite lines). Empty when
// parallel/coincident.
std::vector<std::pair<double, double>> corner_line_line(double ax1,
                                                        double ay1,
                                                        double ax2,
                                                        double ay2,
                                                        double bx1,
                                                        double by1,
                                                        double bx2,
                                                        double by2) {
  std::vector<std::pair<double, double>> pts;
  const double adx = ax2 - ax1, ady = ay2 - ay1;
  const double bdx = bx2 - bx1, bdy = by2 - by1;
  const double denom = adx * bdy - ady * bdx;
  if (std::abs(denom) < 1e-12) return pts;
  const double t = ((bx1 - ax1) * bdy - (by1 - ay1) * bdx) / denom;
  pts.push_back({ax1 + t * adx, ay1 + t * ady});
  return pts;
}

// Intersection of a line support with a full circle. 0/1/2 points.
std::vector<std::pair<double, double>> corner_line_circle(
    double sx, double sy, double ex, double ey, double cx, double cy,
    double r) {
  std::vector<std::pair<double, double>> pts;
  const double dx = ex - sx, dy = ey - sy;
  const double len2 = dx * dx + dy * dy;
  if (len2 < 1e-18) return pts;
  const double fx = sx - cx, fy = sy - cy;
  const double b = 2.0 * (fx * dx + fy * dy) / len2;
  const double c = (fx * fx + fy * fy - r * r) / len2;
  const double disc = b * b - 4.0 * c;
  if (disc < -1e-12) return pts;
  if (std::abs(disc) <= 1e-12) {
    const double t = -0.5 * b;
    pts.push_back({sx + t * dx, sy + t * dy});
    return pts;
  }
  const double sq = std::sqrt(disc);
  const double t1 = 0.5 * (-b - sq), t2 = 0.5 * (-b + sq);
  pts.push_back({sx + t1 * dx, sy + t1 * dy});
  pts.push_back({sx + t2 * dx, sy + t2 * dy});
  return pts;
}

// Intersection of two full circles. 0/1/2 points.
std::vector<std::pair<double, double>> corner_circle_circle(
    double c1x, double c1y, double r1, double c2x, double c2y,
    double r2) {
  std::vector<std::pair<double, double>> pts;
  const double dx = c2x - c1x, dy = c2y - c1y;
  const double d = std::hypot(dx, dy);
  if (d < 1e-12) return pts;  // concentric
  if (d > r1 + r2 || d < std::abs(r1 - r2)) return pts;  // disjoint
  const double a = (d * d + r1 * r1 - r2 * r2) / (2.0 * d);
  const double h2 = r1 * r1 - a * a;
  if (h2 < -1e-12) return pts;
  const double h = h2 <= 1e-12 ? 0.0 : std::sqrt(h2);
  const double mx = c1x + a * dx / d, my = c1y + a * dy / d;
  if (h == 0.0) {
    pts.push_back({mx, my});  // tangent
    return pts;
  }
  const double hx = -dy * h / d, hy = dx * h / d;
  pts.push_back({mx + hx, my + hy});
  pts.push_back({mx - hx, my - hy});
  return pts;
}

// The virtual intersection candidates of A's and B's underlying
// curves (unbounded).
std::vector<std::pair<double, double>> corner_intersections(
    const SketchFeatureParameters& params, const std::string& a_id,
    const std::string& b_id) {
  const SketchLine* al = corner_find_line(params, a_id);
  const SketchLine* bl = corner_find_line(params, b_id);
  const SketchArc* aa = corner_find_arc(params, a_id);
  const SketchArc* ba = corner_find_arc(params, b_id);

  if (al != nullptr && bl != nullptr) {
    return corner_line_line(al->start_x, al->start_y, al->end_x,
                            al->end_y, bl->start_x, bl->start_y,
                            bl->end_x, bl->end_y);
  }
  if (al != nullptr && ba != nullptr) {
    return corner_line_circle(al->start_x, al->start_y, al->end_x,
                              al->end_y, ba->center_x, ba->center_y,
                              ba->radius);
  }
  if (aa != nullptr && bl != nullptr) {
    return corner_line_circle(bl->start_x, bl->start_y, bl->end_x,
                              bl->end_y, aa->center_x, aa->center_y,
                              aa->radius);
  }
  if (aa != nullptr && ba != nullptr) {
    return corner_circle_circle(aa->center_x, aa->center_y, aa->radius,
                                ba->center_x, ba->center_y, ba->radius);
  }
  return {};
}

// Resolves the corner and the near-end choice without mutating.
CornerResolve corner_resolve(const SketchFeatureParameters& params,
                             const std::string& a_id,
                             const std::string& b_id, double click_x,
                             double click_y) {
  CornerResolve r;
  const auto pts = corner_intersections(params, a_id, b_id);
  if (pts.empty()) return r;

  // Nearest candidate to the click is the corner.
  const auto nearest = *std::min_element(
      pts.begin(), pts.end(), [&](const auto& p, const auto& q) {
        return std::hypot(p.first - click_x, p.second - click_y) <
               std::hypot(q.first - click_x, q.second - click_y);
      });
  r.corner_x = nearest.first;
  r.corner_y = nearest.second;

  const SketchLine* al = corner_find_line(params, a_id);
  const SketchLine* bl = corner_find_line(params, b_id);
  const SketchArc* aa = corner_find_arc(params, a_id);
  const SketchArc* ba = corner_find_arc(params, b_id);
  const double a_sx = al != nullptr ? al->start_x : aa->start_x;
  const double a_sy = al != nullptr ? al->start_y : aa->start_y;
  const double a_ex = al != nullptr ? al->end_x : aa->end_x;
  const double a_ey = al != nullptr ? al->end_y : aa->end_y;
  const double b_sx = bl != nullptr ? bl->start_x : ba->start_x;
  const double b_sy = bl != nullptr ? bl->start_y : ba->start_y;
  const double b_ex = bl != nullptr ? bl->end_x : ba->end_x;
  const double b_ey = bl != nullptr ? bl->end_y : ba->end_y;

  // Near end = endpoint closer to the click.
  r.a_near_is_start =
      std::hypot(click_x - a_sx, click_y - a_sy) <
      std::hypot(click_x - a_ex, click_y - a_ey);
  r.b_near_is_start =
      std::hypot(click_x - b_sx, click_y - b_sy) <
      std::hypot(click_x - b_ex, click_y - b_ey);

  r.valid = true;
  return r;
}

// The arc's sweep after moving the near end to the corner (throws
// when the corner would invert or collapse the arc).
double corner_new_arc_sweep(const SketchArc& arc, bool near_is_start,
                            double corner_x, double corner_y) {
  const double s = corner_wrap_angle(
      std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x));
  const double e = corner_wrap_angle(
      std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x));
  const double c = corner_wrap_angle(
      std::atan2(corner_y - arc.center_y, corner_x - arc.center_x));
  double sweep = 0.0;
  if (arc.ccw) {
    sweep = near_is_start ? corner_wrap_angle(e - c)
                          : corner_wrap_angle(c - s);
  } else {
    sweep = near_is_start ? corner_wrap_angle(c - e)
                          : corner_wrap_angle(s - c);
  }
  return sweep;
}

// Moves the near end of `entity_id` to the corner: coordinates, a
// shared/adopted vertex id, and sibling re-pointing (the same
// contract as the trim rebind).
void corner_move_endpoint(SketchFeatureParameters& params,
                          const std::string& entity_id, double new_x,
                          double new_y, double old_x, double old_y,
                          const std::string& old_id,
                          std::string& endpoint_id) {
  const auto shared = find_coincident_endpoint(params, entity_id, new_x, new_y);
  const std::string new_id =
      shared.has_value()
          ? std::get<0>(shared.value())
          : "vertex-" + std::to_string(params.next_vertex_index++);
  endpoint_id = new_id;

  // Sibling re-point: entities that shared the old id either follow
  // the move (still coincident) or re-resolve on their own.
  auto update_shared_pt = [&](std::string& pt_id, double pt_x, double pt_y) {
    if (pt_id != old_id) return;
    if (points_match(pt_x, pt_y, new_x, new_y)) {
      pt_id = new_id;
      return;
    }
    const auto resolved =
        find_coincident_endpoint(params, entity_id, pt_x, pt_y);
    pt_id = resolved.has_value()
                ? std::get<0>(resolved.value())
                : "vertex-" + std::to_string(params.next_vertex_index++);
  };
  for (auto& ol : params.lines) {
    if (ol.id == entity_id) continue;
    update_shared_pt(ol.start_vertex_id, ol.start_x, ol.start_y);
    update_shared_pt(ol.end_vertex_id, ol.end_x, ol.end_y);
  }
  for (auto& oa : params.arcs) {
    if (oa.id == entity_id) continue;
    update_shared_pt(oa.start_vertex_id, oa.start_x, oa.start_y);
    update_shared_pt(oa.end_vertex_id, oa.end_x, oa.end_y);
  }
}

// Applies the corner to ONE entity. Returns true when the entity
// changed (false = its near end was already at the corner).
// Throws when the corner would collapse the entity or invert an arc.
bool corner_apply_to_entity(SketchFeatureParameters& params,
                            const std::string& entity_id,
                            const CornerResolve& r, bool near_is_start) {
  const SketchLine* l = corner_find_line(params, entity_id);
  const SketchArc* a = corner_find_arc(params, entity_id);

  const double near_x = near_is_start ? (l != nullptr ? l->start_x
                                                       : a->start_x)
                                       : (l != nullptr ? l->end_x
                                                       : a->end_x);
  const double near_y = near_is_start ? (l != nullptr ? l->start_y
                                                       : a->start_y)
                                       : (l != nullptr ? l->end_y
                                                       : a->end_y);
  // Already at the corner — nothing to do (keeps the whole entity).
  if (points_match(near_x, near_y, r.corner_x, r.corner_y)) return false;

  // The corner must not swallow the FAR end (that would collapse the
  // entity to zero length / zero sweep).
  const double far_x = near_is_start ? (l != nullptr ? l->end_x : a->end_x)
                                      : (l != nullptr ? l->start_x
                                                      : a->start_x);
  const double far_y = near_is_start ? (l != nullptr ? l->end_y : a->end_y)
                                      : (l != nullptr ? l->start_y
                                                      : a->start_y);
  if (points_match(far_x, far_y, r.corner_x, r.corner_y)) {
    throw std::runtime_error(
        "Sketch corner trim: the corner collapses an entity");
  }

  const std::string old_id =
      near_is_start ? (l != nullptr ? l->start_vertex_id
                                    : a->start_vertex_id)
                    : (l != nullptr ? l->end_vertex_id
                                    : a->end_vertex_id);

  if (a != nullptr) {
    const double sweep = corner_new_arc_sweep(*a, near_is_start, r.corner_x,
                                              r.corner_y);
    if (sweep <= kCornerAngEps ||
        sweep >= 2.0 * kCornerPi - kCornerAngEps) {
      throw std::runtime_error(
          "Sketch corner trim: the corner would invert or collapse an arc");
    }
  }

  if (l != nullptr) {
    auto it = std::find_if(params.lines.begin(), params.lines.end(),
                           [&](const SketchLine& x) {
                             return x.id == entity_id;
                           });
    if (near_is_start) {
      it->start_x = r.corner_x;
      it->start_y = r.corner_y;
    } else {
      it->end_x = r.corner_x;
      it->end_y = r.corner_y;
    }
    corner_move_endpoint(params, entity_id, r.corner_x, r.corner_y,
                         near_x, near_y, old_id,
                         near_is_start ? it->start_vertex_id
                                       : it->end_vertex_id);
  } else {
    auto it = std::find_if(params.arcs.begin(), params.arcs.end(),
                           [&](const SketchArc& x) {
                             return x.id == entity_id;
                           });
    if (near_is_start) {
      it->start_x = r.corner_x;
      it->start_y = r.corner_y;
    } else {
      it->end_x = r.corner_x;
      it->end_y = r.corner_y;
    }
    corner_move_endpoint(params, entity_id, r.corner_x, r.corner_y,
                         near_x, near_y, old_id,
                         near_is_start ? it->start_vertex_id
                                       : it->end_vertex_id);
    // The sweep changed: arc angle dimensions flip driven (the
    // extend tool uses the same convention).
    for (auto& dim : params.dimensions) {
      if (dim.entity_id == entity_id && dim.kind == "arc_angle") {
        dim.driven = true;
      }
    }
  }
  return true;
}

// Resolves AND validates, so the preview never promises a corner the
// apply step would refuse.
CornerResolve corner_resolve_validated(const SketchFeatureParameters& params,
                                       const std::string& a_id,
                                       const std::string& b_id,
                                       double click_x,
                                       double click_y) {
  CornerResolve r = corner_resolve(params, a_id, b_id, click_x, click_y);
  if (!r.valid) return r;
  const SketchLine* al = corner_find_line(params, a_id);
  const SketchLine* bl = corner_find_line(params, b_id);
  const SketchArc* aa = corner_find_arc(params, a_id);
  const SketchArc* ba = corner_find_arc(params, b_id);

  auto entity_valid = [&](const SketchLine* l, const SketchArc* a,
                          bool near_is_start) {
    const double sx = l != nullptr ? l->start_x : a->start_x;
    const double sy = l != nullptr ? l->start_y : a->start_y;
    const double ex = l != nullptr ? l->end_x : a->end_x;
    const double ey = l != nullptr ? l->end_y : a->end_y;
    const double near_x = near_is_start ? sx : ex;
    const double near_y = near_is_start ? sy : ey;
    if (points_match(near_x, near_y, r.corner_x, r.corner_y)) return true;
    const double far_x = near_is_start ? ex : sx;
    const double far_y = near_is_start ? ey : sy;
    if (points_match(far_x, far_y, r.corner_x, r.corner_y)) return false;
    if (a != nullptr) {
      const double sweep =
          corner_new_arc_sweep(*a, near_is_start, r.corner_x, r.corner_y);
      if (sweep <= kCornerAngEps ||
          sweep >= 2.0 * kCornerPi - kCornerAngEps)
        return false;
    }
    return true;
  };

  if (!entity_valid(al, aa, r.a_near_is_start)) r.valid = false;
  if (!entity_valid(bl, ba, r.b_near_is_start)) r.valid = false;
  return r;
}

}  // namespace

CornerTrimPreview corner_trim_preview(const FeatureEntry& feature,
                                      const std::string& entity_a_id,
                                      const std::string& entity_b_id,
                                      double cursor_x,
                                      double cursor_y) {
  CornerTrimPreview out;
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    return out;
  }
  const auto& params = *feature.sketch_parameters;
  const auto r = corner_resolve_validated(params, entity_a_id, entity_b_id,
                                          cursor_x, cursor_y);
  if (!r.valid) return out;

  out.valid = true;
  out.corner_x = r.corner_x;
  out.corner_y = r.corner_y;

  const SketchLine* al = corner_find_line(params, entity_a_id);
  const SketchArc* aa = corner_find_arc(params, entity_a_id);
  const SketchLine* bl = corner_find_line(params, entity_b_id);
  const SketchArc* ba = corner_find_arc(params, entity_b_id);

  auto fill = [&](CornerTrimPreviewSegment& seg, const SketchLine* l,
                  const SketchArc* a, bool near_is_start) {
    seg.kind = l != nullptr ? "line" : "arc";
    if (near_is_start) {
      seg.sx = r.corner_x;
      seg.sy = r.corner_y;
      seg.ex = l != nullptr ? l->end_x : a->end_x;
      seg.ey = l != nullptr ? l->end_y : a->end_y;
    } else {
      seg.sx = l != nullptr ? l->start_x : a->start_x;
      seg.sy = l != nullptr ? l->start_y : a->start_y;
      seg.ex = r.corner_x;
      seg.ey = r.corner_y;
    }
    if (a != nullptr) {
      seg.center_x = a->center_x;
      seg.center_y = a->center_y;
      seg.radius = a->radius;
      seg.ccw = a->ccw;
    }
  };
  fill(out.a, al, aa, r.a_near_is_start);
  fill(out.b, bl, ba, r.b_near_is_start);
  return out;
}

void corner_trim_sketch_entities(FeatureEntry& feature,
                                 const std::string& entity_a_id,
                                 const std::string& entity_b_id,
                                 double click_x,
                                 double click_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can corner-trim");
  }
  auto& params = *feature.sketch_parameters;
  ensure_user_editable_entity(params, entity_a_id);
  ensure_user_editable_entity(params, entity_b_id);
  if (entity_a_id == entity_b_id) {
    throw std::runtime_error(
        "Sketch corner trim: pick two different entities");
  }

  const SketchLine* al = corner_find_line(params, entity_a_id);
  const SketchLine* bl = corner_find_line(params, entity_b_id);
  const SketchArc* aa = corner_find_arc(params, entity_a_id);
  const SketchArc* ba = corner_find_arc(params, entity_b_id);
  if ((al == nullptr && aa == nullptr) || (bl == nullptr && ba == nullptr)) {
    throw std::runtime_error(
        "Sketch corner trim: only lines and arcs are supported");
  }
  if ((al != nullptr && al->is_construction) ||
      (bl != nullptr && bl->is_construction) ||
      (aa != nullptr && aa->is_construction) ||
      (ba != nullptr && ba->is_construction)) {
    throw std::runtime_error(
        "Sketch corner trim: construction entities cannot be corner-trimmed");
  }

  const auto r = corner_resolve(params, entity_a_id, entity_b_id, click_x,
                                click_y);
  if (!r.valid) {
    throw std::runtime_error(
        "Sketch corner trim: no virtual corner (entities are parallel or "
        "their curves do not intersect)");
  }

  // Apply to A first (so the corner endpoint id it adopts is B's
  // original near-end id when they both land on the corner; B's move
  // then re-finds that same id and the two endpoints share it).
  corner_apply_to_entity(params, entity_a_id, r, r.a_near_is_start);
  corner_apply_to_entity(params, entity_b_id, r, r.b_near_is_start);

  // Safety net: coincident constraints whose merged point died with
  // the moved endpoints or whose lines no longer share it.
  sweep_orphan_coincident_constraints(params);
}
