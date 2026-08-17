// Exact-curve planar arrangement profile detection.
//
// Replaces the former polyline-approximation pipeline (64-segment circle
// polygons, quantized node keys, split hacks, curve-only face filters,
// circle-face synthesis, edge-loop fallback).  Every solid sketch entity
// (line / circle / arc) is treated as an exact parametric curve:
//   line:   t in [0, 1]
//   circle: angle in [0, 2π)
//   arc:    angle, increasing along the stored ccw sweep
// Split parameters come from the trim engine's exact analytic
// intersection functions plus endpoint-on-curve touch records, so no
// coordinate quantization is ever used for graph identity.
//
// Every bounded closed region of the arrangement is a profile.  Faces
// are walked with the interior on the left using exact tangent
// directions; the exterior face (CW, negative signed area) and
// degenerate zero-area tangent pairings are discarded.  CW cycles
// nested inside a CCW face become its inner loops (holes).

namespace {

constexpr double kExactPi = 3.14159265358979323846;
constexpr double kExactParamEps = 1e-9;

double exact_wrap_angle(double a) {
  while (a < 0.0) a += 2.0 * kExactPi;
  while (a >= 2.0 * kExactPi) a -= 2.0 * kExactPi;
  return a;
}

// ── exact curve model ─────────────────────────────────────────────

struct ExactCurve {
  enum class Kind { kLine, kCircle, kArc };
  Kind kind;
  std::string id;
  // line
  double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
  // circle / arc
  double cx = 0.0, cy = 0.0, r = 0.0;
  bool ccw = true;             // stored sweep direction (circles: ccw)
  double sweep_start = 0.0;    // wrapped angle of the entity's start param
  double sweep_end = 0.0;      // wrapped angle of the entity's end param (arcs)
  // shared vertex ids for endpoint pre-union (lines and arcs only)
  std::string start_vertex_id;
  std::string end_vertex_id;
};

SketchProfilePoint exact_curve_point(const ExactCurve& c, double param) {
  if (c.kind == ExactCurve::Kind::kLine) {
    return SketchProfilePoint{.x = c.x0 + param * (c.x1 - c.x0),
                             .y = c.y0 + param * (c.y1 - c.y0)};
  }
  return SketchProfilePoint{.x = c.cx + c.r * std::cos(param),
                           .y = c.cy + c.r * std::sin(param)};
}

// Unit tangent of INCREASING parameter at `param` (line: start->end;
// circle: ccw; arc: along the stored sweep).
void exact_curve_tangent(const ExactCurve& c, double param, double& tx,
                         double& ty) {
  if (c.kind == ExactCurve::Kind::kLine) {
    const double len = std::sqrt((c.x1 - c.x0) * (c.x1 - c.x0) +
                                 (c.y1 - c.y0) * (c.y1 - c.y0));
    tx = (c.x1 - c.x0) / len;
    ty = (c.y1 - c.y0) / len;
    return;
  }
  const double s = std::sin(param);
  const double cc = std::cos(param);
  const bool inc_is_ccw = (c.kind == ExactCurve::Kind::kCircle) || c.ccw;
  if (inc_is_ccw) {
    tx = -s; ty = cc;
  } else {
    tx = s; ty = -cc;
  }
}

// Lifts a wrapped angle into the sweep frame of a circular entity so
// params can be ordered along the sweep direction.
double exact_lift_to_sweep(const ExactCurve& c, double wrapped) {
  const bool inc_is_ccw = (c.kind == ExactCurve::Kind::kCircle) || c.ccw;
  double a = wrapped;
  if (inc_is_ccw) {
    while (a < c.sweep_start) a += 2.0 * kExactPi;
  } else {
    while (a > c.sweep_start) a -= 2.0 * kExactPi;
  }
  return a;
}

bool exact_angle_in_sweep(double wrapped, const ExactCurve& arc) {
  const double s = arc.sweep_start;
  const double e = arc.sweep_end;
  if (arc.ccw) {
    double a = wrapped;
    if (a < s) a += 2.0 * kExactPi;
    double ee = e;
    if (ee <= s) ee += 2.0 * kExactPi;
    return a >= s - kExactParamEps && a <= ee + kExactParamEps;
  }
  double a = wrapped;
  if (a > s) a -= 2.0 * kExactPi;
  double ee = e;
  if (ee >= s) ee -= 2.0 * kExactPi;
  return a <= s + kExactParamEps && a >= ee - kExactParamEps;
}

// Signed area contribution of one walked half-edge: ½(x dy − y dx)
// integrated exactly.  Lines use the shoelace term of the endpoint
// coordinates; circular pieces integrate over their lifted angle span
// (signed — walking a piece backwards flips the sign).
double exact_half_edge_area(const ExactCurve& c, double ps, double pe,
                            const SketchProfilePoint& p0,
                            const SketchProfilePoint& p1) {
  if (c.kind == ExactCurve::Kind::kLine) {
    return 0.5 * (p0.x * p1.y - p1.x * p0.y);
  }
  const double d_theta = pe - ps;
  return 0.5 * (c.r * c.r * d_theta +
                c.r * c.cx * (std::sin(pe) - std::sin(ps)) -
                c.r * c.cy * (std::cos(pe) - std::cos(ps)));
}

// ── half-edge graph ───────────────────────────────────────────────

struct ExactHalfEdge {
  int node_start = -1;
  int node_end = -1;
  int curve = -1;          // index into the curve list
  bool forward = true;     // false = twin (walks against parameter direction)
  double param_start = 0.0;
  double param_end = 0.0;
  int twin = -1;
  bool used = false;
};

struct ExactNode {
  double x = 0.0;
  double y = 0.0;
};

struct ExactFace {
  std::vector<int> edges;  // half-edge indices in walk order
  double signed_area = 0.0;
  bool ccw = false;
};

struct ExactDSU {
  std::vector<int> parent;
  explicit ExactDSU(size_t n) : parent(n) {
    for (size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);
  }
  int find(int x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  }
  void unite(int a, int b) {
    a = find(a); b = find(b);
    if (a != b) parent[a] = b;
  }
};

// One unification record: two (curve, param) references that denote the
// same geometric point.
struct ExactUnionRecord {
  int curve_a;
  double param_a;
  int curve_b;
  double param_b;
};

double exact_point_segment_dist_sq(double px, double py, double ax, double ay,
                                   double bx, double by) {
  const double abx = bx - ax, aby = by - ay;
  const double len_sq = abx * abx + aby * aby;
  double t = 0.0;
  if (len_sq > 1e-18) {
    t = ((px - ax) * abx + (py - ay) * aby) / len_sq;
    t = std::max(0.0, std::min(1.0, t));
  }
  const double dx = px - (ax + t * abx);
  const double dy = py - (ay + t * aby);
  return dx * dx + dy * dy;
}

bool exact_point_in_polygon(const SketchProfilePoint& pt,
                            const std::vector<SketchProfilePoint>& poly) {
  bool inside = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    const SketchProfilePoint& a = poly[i];
    const SketchProfilePoint& b = poly[j];
    const bool crosses = ((a.y > pt.y) != (b.y > pt.y)) &&
                         (pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y) + a.x);
    if (crosses) inside = !inside;
  }
  return inside;
}

std::vector<SketchProfilePoint> exact_sample_circle_loop(
    const ExactCurve& circle) {
  std::vector<SketchProfilePoint> points;
  points.reserve(kCircleProfileSampleSegments);
  for (int index = 0; index < kCircleProfileSampleSegments; ++index) {
    const double angle = (static_cast<double>(index) /
                          static_cast<double>(kCircleProfileSampleSegments)) *
                         2.0 * kExactPi;
    points.push_back(SketchProfilePoint{
        .x = circle.cx + circle.r * std::cos(angle),
        .y = circle.cy + circle.r * std::sin(angle),
    });
  }
  return points;
}

}  // namespace

// ── main entry: build every bounded region of the exact arrangement ──

std::vector<SketchProfileRegion> build_exact_profiles(
    const SketchFeatureParameters& parameters) {
  std::vector<SketchProfileRegion> regions;

  // 1. Collect solid entities as exact curves.
  std::vector<ExactCurve> curves;
  bool has_solid_lines = false;
  bool has_solid_arcs = false;
  for (const auto& line : parameters.lines) {
    if (line.is_construction) continue;
    if (points_match({line.start_x, line.start_y}, {line.end_x, line.end_y})) {
      continue;
    }
    has_solid_lines = true;
    curves.push_back(ExactCurve{
        .kind = ExactCurve::Kind::kLine,
        .id = line.id,
        .x0 = line.start_x, .y0 = line.start_y,
        .x1 = line.end_x, .y1 = line.end_y,
        .start_vertex_id = line.start_vertex_id,
        .end_vertex_id = line.end_vertex_id,
    });
  }
  for (const auto& circle : parameters.circles) {
    if (circle.is_construction) continue;
    curves.push_back(ExactCurve{
        .kind = ExactCurve::Kind::kCircle,
        .id = circle.id,
        .cx = circle.center_x, .cy = circle.center_y, .r = circle.radius,
        .ccw = true,
    });
  }
  for (const auto& arc : parameters.arcs) {
    if (arc.is_construction) continue;
    has_solid_arcs = true;
    curves.push_back(ExactCurve{
        .kind = ExactCurve::Kind::kArc,
        .id = arc.id,
        .cx = arc.center_x, .cy = arc.center_y, .r = arc.radius,
        .ccw = arc.ccw,
        .sweep_start = exact_wrap_angle(std::atan2(arc.start_y - arc.center_y,
                                                   arc.start_x - arc.center_x)),
        .sweep_end = exact_wrap_angle(std::atan2(arc.end_y - arc.center_y,
                                                 arc.end_x - arc.center_x)),
        .start_vertex_id = arc.start_vertex_id,
        .end_vertex_id = arc.end_vertex_id,
    });
  }
  if (curves.empty()) return regions;

  const int nc = static_cast<int>(curves.size());
  std::vector<std::vector<double>> splits(nc);
  std::vector<ExactUnionRecord> unions;

  auto line_param = [](double t) { return std::max(0.0, std::min(1.0, t)); };
  auto angle_param = [](double a) { return exact_wrap_angle(a); };

  // Sketch entity views for the trim-engine intersection functions.
  auto as_line = [](const ExactCurve& c) {
    return SketchLine{.id = c.id, .start_x = c.x0, .start_y = c.y0,
                      .end_x = c.x1, .end_y = c.y1};
  };
  auto as_circle = [](const ExactCurve& c) {
    return SketchCircle{.id = c.id, .center_x = c.cx, .center_y = c.cy,
                        .radius = c.r};
  };
  auto as_arc = [](const ExactCurve& c) {
    return SketchArc{.id = c.id,
                     .center_x = c.cx,
                     .center_y = c.cy,
                     .radius = c.r,
                     .start_x = c.cx + c.r * std::cos(c.sweep_start),
                     .start_y = c.cy + c.r * std::sin(c.sweep_start),
                     .end_x = c.cx + c.r * std::cos(c.sweep_end),
                     .end_y = c.cy + c.r * std::sin(c.sweep_end),
                     .ccw = c.ccw};
  };

  // All exact intersections between two curves: fills `out` with
  // (param on A, param on B) records using the trim engine's analytic
  // solvers.  Covers every kind pairing.
  auto collect_intersections =
      [&](const ExactCurve& A, const ExactCurve& B,
          std::vector<std::pair<double, double>>& out) {
        using K = ExactCurve::Kind;
        if (A.kind == K::kLine && B.kind == K::kLine) {
          if (const auto is = intersect_line_line(as_line(A), as_line(B));
              is.has_value()) {
            out.push_back({line_param(is->param_on_target),
                           line_param(is->param_on_other)});
          }
        } else if (A.kind == K::kLine && B.kind == K::kCircle) {
          for (const auto& is : intersect_line_circle(as_line(A), as_circle(B))) {
            out.push_back({line_param(is.param_on_target),
                           angle_param(is.param_on_other)});
          }
        } else if (A.kind == K::kCircle && B.kind == K::kLine) {
          for (const auto& is : intersect_line_circle(as_line(B), as_circle(A))) {
            out.push_back({angle_param(is.param_on_other),
                           line_param(is.param_on_target)});
          }
        } else if (A.kind == K::kCircle && B.kind == K::kCircle) {
          for (const auto& is :
               intersect_circle_circle(as_circle(A), as_circle(B))) {
            out.push_back({angle_param(is.param_on_target),
                           angle_param(is.param_on_other)});
          }
        } else if (A.kind == K::kLine && B.kind == K::kArc) {
          for (const auto& is : intersect_arc_line(as_arc(B), as_line(A))) {
            out.push_back({line_param(is.param_on_other),
                           angle_param(is.param_on_target)});
          }
        } else if (A.kind == K::kArc && B.kind == K::kLine) {
          for (const auto& is : intersect_arc_line(as_arc(A), as_line(B))) {
            out.push_back({angle_param(is.param_on_target),
                           line_param(is.param_on_other)});
          }
        } else if (A.kind == K::kCircle && B.kind == K::kArc) {
          for (const auto& is : intersect_arc_circle(as_arc(B), as_circle(A))) {
            out.push_back({angle_param(is.param_on_other),
                           angle_param(is.param_on_target)});
          }
        } else if (A.kind == K::kArc && B.kind == K::kCircle) {
          for (const auto& is : intersect_arc_circle(as_arc(A), as_circle(B))) {
            out.push_back({angle_param(is.param_on_target),
                           angle_param(is.param_on_other)});
          }
        } else {  // arc x arc
          for (const auto& is : intersect_arc_arc(as_arc(A), as_arc(B))) {
            out.push_back({angle_param(is.param_on_target),
                           angle_param(is.param_on_other)});
          }
        }
      };

  // 2. Intersection and touch records.
  for (int a = 0; a < nc; ++a) {
    for (int b = a + 1; b < nc; ++b) {
      const ExactCurve& ca = curves[a];
      const ExactCurve& cb = curves[b];
      std::vector<std::pair<double, double>> recs;
      collect_intersections(ca, cb, recs);
      for (const auto& [pa, pb] : recs) {
        splits[a].push_back(pa);
        splits[b].push_back(pb);
        unions.push_back({a, pa, b, pb});
      }

      // Touch records: endpoints of A lying on B, and vice versa.
      auto endpoint_refs = [&](const ExactCurve& c) {
        struct Ref { double param; std::string vertex_id; double x, y; };
        std::vector<Ref> refs;
        if (c.kind == ExactCurve::Kind::kLine) {
          refs.push_back({0.0, c.start_vertex_id, c.x0, c.y0});
          refs.push_back({1.0, c.end_vertex_id, c.x1, c.y1});
        } else if (c.kind == ExactCurve::Kind::kArc) {
          refs.push_back({c.sweep_start, c.start_vertex_id,
                          c.cx + c.r * std::cos(c.sweep_start),
                          c.cy + c.r * std::sin(c.sweep_start)});
          refs.push_back({c.sweep_end, c.end_vertex_id,
                          c.cx + c.r * std::cos(c.sweep_end),
                          c.cy + c.r * std::sin(c.sweep_end)});
        }
        return refs;
      };
      auto touches = [&](const ExactCurve& src, const ExactCurve& dst,
                         int src_idx, int dst_idx) {
        for (const auto& ref : endpoint_refs(src)) {
          if (dst.kind == ExactCurve::Kind::kCircle) {
            const double dx = ref.x - dst.cx, dy = ref.y - dst.cy;
            if (std::abs(std::sqrt(dx * dx + dy * dy) - dst.r) >
                kProfileTolerance) {
              continue;
            }
            const double ang =
                exact_wrap_angle(std::atan2(dy, dx));
            splits[src_idx].push_back(ref.param);
            splits[dst_idx].push_back(ang);
            unions.push_back({src_idx, ref.param, dst_idx, ang});
          } else if (dst.kind == ExactCurve::Kind::kArc) {
            const double dx = ref.x - dst.cx, dy = ref.y - dst.cy;
            if (std::abs(std::sqrt(dx * dx + dy * dy) - dst.r) >
                kProfileTolerance) {
              continue;
            }
            const double ang = exact_wrap_angle(std::atan2(dy, dx));
            if (!exact_angle_in_sweep(ang, dst)) continue;
            splits[src_idx].push_back(ref.param);
            splits[dst_idx].push_back(ang);
            unions.push_back({src_idx, ref.param, dst_idx, ang});
          } else {  // line
            if (exact_point_segment_dist_sq(ref.x, ref.y, dst.x0, dst.y0,
                                            dst.x1, dst.y1) >
                kProfileTolerance * kProfileTolerance) {
              continue;
            }
            const double abx = dst.x1 - dst.x0, aby = dst.y1 - dst.y0;
            const double len_sq = abx * abx + aby * aby;
            const double t =
                len_sq > 1e-18
                    ? std::max(
                          0.0, std::min(1.0, ((ref.x - dst.x0) * abx +
                                              (ref.y - dst.y0) * aby) /
                                                 len_sq))
                    : 0.0;
            splits[src_idx].push_back(ref.param);
            splits[dst_idx].push_back(t);
            unions.push_back({src_idx, ref.param, dst_idx, t});
          }
        }
      };
      touches(ca, cb, a, b);
      touches(cb, ca, b, a);
    }
  }

  // Shared-vertex-id pre-union between line/arc endpoints.
  for (int a = 0; a < nc; ++a) {
    if (curves[a].kind == ExactCurve::Kind::kCircle) continue;
    for (int b = a + 1; b < nc; ++b) {
      if (curves[b].kind == ExactCurve::Kind::kCircle) continue;
      const ExactCurve& ca = curves[a];
      const ExactCurve& cb = curves[b];
      const double pa_start = (ca.kind == ExactCurve::Kind::kLine)
                                  ? 0.0 : ca.sweep_start;
      const double pa_end = (ca.kind == ExactCurve::Kind::kLine)
                                ? 1.0 : ca.sweep_end;
      const double pb_start = (cb.kind == ExactCurve::Kind::kLine)
                                  ? 0.0 : cb.sweep_start;
      const double pb_end = (cb.kind == ExactCurve::Kind::kLine)
                                ? 1.0 : cb.sweep_end;
      if (!ca.start_vertex_id.empty() &&
          ca.start_vertex_id == cb.start_vertex_id) {
        unions.push_back({a, pa_start, b, pb_start});
      }
      if (!ca.start_vertex_id.empty() &&
          ca.start_vertex_id == cb.end_vertex_id) {
        unions.push_back({a, pa_start, b, pb_end});
      }
      if (!ca.end_vertex_id.empty() && ca.end_vertex_id == cb.start_vertex_id) {
        unions.push_back({a, pa_end, b, pb_start});
      }
      if (!ca.end_vertex_id.empty() && ca.end_vertex_id == cb.end_vertex_id) {
        unions.push_back({a, pa_end, b, pb_end});
      }
    }
  }

  // 3. Node creation: nodes keyed by (curve, canonical param); endpoint
  // nodes prefer the canonical SketchVertex coordinates (mirrors the old
  // solid_line_segments behavior for shared points).
  std::vector<ExactNode> nodes;
  std::vector<std::vector<std::pair<double, int>>> node_lookup(nc);

  auto canonical_param = [&](const ExactCurve& c, double p) {
    return (c.kind == ExactCurve::Kind::kLine) ? line_param(p)
                                               : angle_param(p);
  };

  auto vertex_coords = [&](const std::string& vid, double fx, double fy) {
    for (const auto& v : parameters.vertices) {
      if (v.id == vid) return SketchProfilePoint{.x = v.x, .y = v.y};
    }
    return SketchProfilePoint{.x = fx, .y = fy};
  };

  auto ensure_node = [&](int curve, double param) -> int {
    const ExactCurve& c = curves[curve];
    const double p = canonical_param(c, param);
    auto& list = node_lookup[curve];
    const SketchProfilePoint pp = exact_curve_point(c, p);
    for (const auto& [known, n] : list) {
      // Geometric identity: tangent double-roots from the quadratic
      // solver can land ~1e-8 in param apart (far above
      // kExactParamEps) — sliver pieces and duplicate nodes at the
      // same point break the face walk.  Treat points within profile
      // tolerance as one node.
      const SketchProfilePoint kp = exact_curve_point(c, known);
      if (std::abs(kp.x - pp.x) <= kProfileTolerance &&
          std::abs(kp.y - pp.y) <= kProfileTolerance) {
        return n;
      }
    }
    const SketchProfilePoint raw = exact_curve_point(c, p);
    const int index = static_cast<int>(nodes.size());
    nodes.push_back(ExactNode{.x = raw.x, .y = raw.y});
    // Endpoint vertex override.
    if (c.kind != ExactCurve::Kind::kCircle) {
      const double p_start = (c.kind == ExactCurve::Kind::kLine)
                                 ? 0.0 : c.sweep_start;
      const double p_end = (c.kind == ExactCurve::Kind::kLine)
                               ? 1.0 : c.sweep_end;
      if (std::abs(p - p_start) <= kExactParamEps &&
          !c.start_vertex_id.empty()) {
        const SketchProfilePoint vp =
            vertex_coords(c.start_vertex_id, raw.x, raw.y);
        nodes[index].x = vp.x; nodes[index].y = vp.y;
      } else if (std::abs(p - p_end) <= kExactParamEps &&
                 !c.end_vertex_id.empty()) {
        const SketchProfilePoint vp =
            vertex_coords(c.end_vertex_id, raw.x, raw.y);
        nodes[index].x = vp.x; nodes[index].y = vp.y;
      }
    }
    list.push_back({p, index});
    return index;
  };

  // 4. Half-edge graph.
  std::vector<ExactHalfEdge> half_edges;
  for (int i = 0; i < nc; ++i) {
    const ExactCurve& c = curves[i];

    std::vector<double> ps;
    for (double s : splits[i]) ps.push_back(canonical_param(c, s));
    if (c.kind == ExactCurve::Kind::kLine) {
      ps.push_back(0.0);
      ps.push_back(1.0);
    } else if (c.kind == ExactCurve::Kind::kArc) {
      ps.push_back(c.sweep_start);
      ps.push_back(c.sweep_end);
    }
    std::sort(ps.begin(), ps.end());
    ps.erase(std::unique(ps.begin(), ps.end(), [](double x, double y) {
               return std::abs(x - y) <= kExactParamEps;
             }), ps.end());
    if (c.kind == ExactCurve::Kind::kCircle && ps.empty()) {
      ps.push_back(0.0);  // full-circle anchor
    }

    // Ordered param sequence along the sweep direction.
    std::vector<double> seq;
    if (c.kind == ExactCurve::Kind::kLine) {
      seq = ps;
    } else {
      for (double p : ps) seq.push_back(exact_lift_to_sweep(c, p));
      if (c.kind == ExactCurve::Kind::kCircle) {
        std::sort(seq.begin(), seq.end());
        seq.push_back(seq.front() + 2.0 * kExactPi);  // closing entry
      } else {
        std::sort(seq.begin(), seq.end(),
                  [&](double x, double y) { return c.ccw ? x < y : x > y; });
      }
    }

    const int piece_count = static_cast<int>(seq.size()) - 1;
    for (int k = 0; k < piece_count; ++k) {
      const double p0 = seq[k];
      const double p1 = seq[k + 1];
      if (std::abs(p1 - p0) <= kExactParamEps) continue;

      const int n0 = ensure_node(i, p0);
      const int n1 = ensure_node(i, p1);
      // Full circles legitimately close on themselves (n0 == n1); every
      // other degenerate piece is dropped.
      const bool full_circle_piece =
          c.kind == ExactCurve::Kind::kCircle &&
          std::abs(std::abs(p1 - p0) - 2.0 * kExactPi) <= kExactParamEps;
      if (n0 == n1 && !full_circle_piece) continue;

      const int fwd = static_cast<int>(half_edges.size());
      const int bwd = fwd + 1;
      half_edges.push_back(ExactHalfEdge{
          .node_start = n0, .node_end = n1, .curve = i, .forward = true,
          .param_start = p0, .param_end = p1, .twin = bwd, .used = false});
      half_edges.push_back(ExactHalfEdge{
          .node_start = n1, .node_end = n0, .curve = i, .forward = false,
          .param_start = p1, .param_end = p0, .twin = fwd, .used = false});
    }
  }

  // Union pass: connect nodes via the records.
  ExactDSU dsu(nodes.size());
  for (const auto& u : unions) {
    const ExactCurve& ca = curves[u.curve_a];
    const ExactCurve& cb = curves[u.curve_b];
    // Geometric node lookup (matches ensure_node): union params carry
    // the same tangent-double-root noise, so resolve by point.
    const SketchProfilePoint upa =
        exact_curve_point(ca, canonical_param(ca, u.param_a));
    const SketchProfilePoint upb =
        exact_curve_point(cb, canonical_param(cb, u.param_b));
    int na = -1;
    for (const auto& [p, n] : node_lookup[u.curve_a]) {
      const SketchProfilePoint pp = exact_curve_point(ca, p);
      if (std::abs(pp.x - upa.x) <= kProfileTolerance &&
          std::abs(pp.y - upa.y) <= kProfileTolerance) {
        na = n; break;
      }
    }
    int nb = -1;
    for (const auto& [p, n] : node_lookup[u.curve_b]) {
      const SketchProfilePoint pp = exact_curve_point(cb, p);
      if (std::abs(pp.x - upb.x) <= kProfileTolerance &&
          std::abs(pp.y - upb.y) <= kProfileTolerance) {
        nb = n; break;
      }
    }
    if (na >= 0 && nb >= 0) dsu.unite(na, nb);
  }
  std::vector<int> rep(nodes.size());
  std::vector<ExactNode> merged;
  for (size_t i = 0; i < nodes.size(); ++i) {
    const int r = dsu.find(static_cast<int>(i));
    if (r == static_cast<int>(i)) {
      rep[i] = static_cast<int>(merged.size());
      merged.push_back(nodes[i]);
    }
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    rep[i] = rep[dsu.find(static_cast<int>(i))];
  }
  nodes = std::move(merged);
  for (auto& h : half_edges) {
    h.node_start = rep[h.node_start];
    h.node_end = rep[h.node_end];
  }

  // Drop dangling curves before the face walk.  A line or arc whose
  // endpoint node is touched by no other curve cannot bound any closed
  // region, and its half-edges steal face walks at T-junctions: the
  // leftmost-turn rule diverts onto the dead end and the real face
  // never closes (user-reported: after trimming arc pieces away, a
  // leftover line dangling into the remaining region kept the whole
  // region from becoming a profile).  Curves whose end was split away
  // by a later trim become dangling exactly this way.
  {
    std::vector<std::set<int>> node_curves(nodes.size());
    for (const auto& h : half_edges) {
      node_curves[h.node_start].insert(h.curve);
      node_curves[h.node_end].insert(h.curve);
    }
    std::vector<bool> curve_dropped(nc, false);
    for (int i = 0; i < nc; ++i) {
      const ExactCurve& c = curves[i];
      if (c.kind == ExactCurve::Kind::kCircle) continue;
      const double p_start =
          (c.kind == ExactCurve::Kind::kLine) ? 0.0 : c.sweep_start;
      const double p_end =
          (c.kind == ExactCurve::Kind::kLine) ? 1.0 : c.sweep_end;
      int start_node = -1;
      int end_node = -1;
      for (const auto& h : half_edges) {
        if (h.curve != i || !h.forward) continue;
        if (std::abs(h.param_start - p_start) <= kExactParamEps) {
          start_node = h.node_start;
        }
        if (std::abs(h.param_end - p_end) <= kExactParamEps) {
          end_node = h.node_end;
        }
      }
      if (start_node >= 0 && node_curves[start_node].size() <= 1) {
        curve_dropped[i] = true;
      }
      if (end_node >= 0 && node_curves[end_node].size() <= 1) {
        curve_dropped[i] = true;
      }
    }
    for (auto& h : half_edges) {
      if (curve_dropped[h.curve]) h.used = true;
    }
  }

  // 5. Face walk: interior on the left.  At each node the next edge is
  // the one immediately clockwise from the reverse of the incoming edge
  // (equivalently the largest CCW turn measured from the reverse tangent)
  // — the standard DCEL face-traversal rule.
  const int nh = static_cast<int>(half_edges.size());
  std::vector<std::vector<int>> out_edges(nodes.size());
  for (int h = 0; h < nh; ++h) {
    if (half_edges[h].used) continue;  // dangling-curve pieces
    out_edges[half_edges[h].node_start].push_back(h);
  }
  // Permanent diagnostic: PS_TRACE_FACES=1 dumps every face-walk step
  // (incoming edge, each candidate's turn, the winner) to the
  // structured log under tag "exact_profiles".  The arrangement walk
  // is the most regression-prone code in the profile detector — this
  // trace turns a face-walk bisection from hours into minutes, so it
  // stays in the source instead of being re-added per debugging
  // session.
  const bool kTraceFaces = std::getenv("PS_TRACE_FACES") != nullptr;

  std::vector<ExactFace> faces;
  for (int start = 0; start < nh; ++start) {
    if (half_edges[start].used) continue;

    std::vector<int> walk;
    std::set<int> walk_seen;
    int cur = start;
    bool closed = false;
    if (kTraceFaces) {
      log_debug("exact_profiles",
                "face walk start=" + std::to_string(start));
    }
    const int guard = 2 * nh + 4;
    for (int step = 0; step < guard; ++step) {
      if (half_edges[cur].used || walk_seen.count(cur)) break;
      walk_seen.insert(cur);
      walk.push_back(cur);

      const ExactHalfEdge& h = half_edges[cur];
      const int at = h.node_end;
      const ExactCurve& ec = curves[h.curve];
      double d_in_x, d_in_y;
      exact_curve_tangent(ec, h.param_end, d_in_x, d_in_y);
      if (!h.forward) { d_in_x = -d_in_x; d_in_y = -d_in_y; }

      int best = -1;
      double best_cw = -1.0;
      bool best_is_same_id = false;
      // CCW turn of a candidate measured from the reverse of the
      // incoming tangent — raw atan2 value in (-π, π]; normalized into
      // [0, 2π) in the candidate loop below.
      const double rx0 = -d_in_x, ry0 = -d_in_y;
      auto cw_of = [&](double dcx, double dcy) {
        return std::atan2(rx0 * dcy - ry0 * dcx, rx0 * dcx + ry0 * dcy);
      };
      constexpr double kUturnWrapEps = 2.0 * kExactPi / 180.0;
      constexpr double kUturnEps = 2.0 * kExactPi / 180.0;
      constexpr double kTangentTieBreak = 5.0 * kExactPi / 180.0;

      // Face traversal: pick the LARGEST CCW turn from the reverse of
      // the incoming tangent (≡ the first edge clockwise from the
      // reverse — the standard DCEL rule).  Two tangent-junction
      // refinements, both needed by the trim-tool corner geometry:
      //  1. A LINE arriving at a tangent point must switch onto the
      //     tangent curve: the candidate anti-parallel to the incoming
      //     (cw ≈ 0/2π) is the wedge's U-turn back through the tangent
      //     digon and wins outright — otherwise the wedged face on the
      //     tangent's far side can never close.
      //  2. A candidate continuing the incoming edge's own curve wins
      //     when its turn is within a few degrees of the best — exact
      //     tangent directions differ by a fraction of a degree at
      //     line-circle junctions, and the face on the curve's inside
      //     must stay on the curve.
      const bool incoming_is_line =
          curves[h.curve].kind == ExactCurve::Kind::kLine;
      // Which side of the incoming curve the walk's interior lies on.
      // For a curve arrival, the curve center is LEFT of the incoming
      // direction exactly when this walk is the curve's inside face
      // (holds for CCW and CW entities alike).  Meaningless for line
      // arrivals.
      const bool center_left =
          incoming_is_line
              ? false
              : d_in_x * (curves[h.curve].cy - nodes[at].y) -
                        d_in_y * (curves[h.curve].cx - nodes[at].x) > 0.0;
      std::vector<double> cand_cw;
      std::vector<int> cand_idx;
      if (kTraceFaces) {
        log_debug("exact_profiles",
                  "face walk step incoming=" + curves[h.curve].id +
                      " fwd=" + std::to_string(h.forward) +
                      " line=" + std::to_string(incoming_is_line) +
                      " node=" + std::to_string(at));
      }
      for (const int cand : out_edges[at]) {
        if (cand == h.twin) continue;
        const ExactHalfEdge& ch = half_edges[cand];
        const ExactCurve& cc = curves[ch.curve];
        double d_c_x, d_c_y;
        exact_curve_tangent(cc, ch.param_start, d_c_x, d_c_y);
        if (!ch.forward) { d_c_x = -d_c_x; d_c_y = -d_c_y; }
        // Normalize the turn into [0, 2π) — the largest-turn rule is
        // the standard first-edge-clockwise-from-the-reverse DCEL rule,
        // and atan2 reports clockwise turns as negative values.  Same-
        // curve continuations also report ±π from an exact zero cross
        // product; normalizing makes the straight continuation beat
        // competing turns consistently.
        // Exception: a LINE whose turn is a tiny epsilon of either
        // sign while arriving FORWARD along a curve's INSIDE face is
        // the tangent U-turn back through the node (exact tangencies
        // produce cw about +/-1e-6, hand-drawn ones up to a degree
        // off) — it must stay small so the same-curve continuation
        // (cw = π) wins and the lens face stays closed.  The curve's
        // OUTSIDE face turns onto that tangent line (the tangent
        // wedge) and keeps the U-turn normalization; its U-turn may
        // also be met arriving backward (wedge) — neither applies the
        // exception, so both normalize to 2π.
        const double raw_cw = cw_of(d_c_x, d_c_y);
        double cw = raw_cw;
        if (cw <= kUturnWrapEps) {
          const bool tangent_uturn_line =
              cc.kind == ExactCurve::Kind::kLine && !incoming_is_line &&
              h.forward && center_left && cw >= -kUturnWrapEps;
          if (!tangent_uturn_line) {
            cw += 2.0 * kExactPi;
          }
        }
        cand_cw.push_back(cw);
        cand_idx.push_back(cand);
        if (kTraceFaces) {
          log_debug("exact_profiles",
                    "face walk cand id=" + cc.id +
                        " kind=" + std::to_string(static_cast<int>(cc.kind)) +
                        " fwd=" + std::to_string(ch.forward) +
                        " raw=" + std::to_string(raw_cw) +
                        " cw=" + std::to_string(cw));
        }
        // A line arriving at the node switches onto the tangent curve.
        if (incoming_is_line &&
            cc.kind != ExactCurve::Kind::kLine &&
            std::min(cw, 2.0 * kExactPi - cw) <= kUturnEps) {
          best = cand;
          best_cw = cw;
          best_is_same_id = false;
          break;
        }
      }

      if (best < 0 && !incoming_is_line) {
        // 3. A curve's INSIDE face arriving at a tangent junction
        // stays on its curve: the anti-parallel candidate (cw ≈ 0/2π)
        // is the tangent LINE doubling back, and taking it strands the
        // walk on the wedge digon while the inside boundary continues
        // straight along the curve (cw ≈ π).  The OUTSIDE face turns
        // onto the tangent line — the largest-cw rule already picks
        // that U-turn, so only the inside face (curve center on the
        // left of the incoming direction) is forced through here.
        // (Line arrivals are handled by rule 1.)
        if (center_left) {
          bool has_line_uturn = false;
          for (size_t ci = 0; ci < cand_idx.size(); ++ci) {
            if (curves[half_edges[cand_idx[ci]].curve].kind !=
                ExactCurve::Kind::kLine) {
              continue;
            }
            const double cw = cand_cw[ci];
            if (std::min(cw, 2.0 * kExactPi - cw) <= kUturnEps) {
              has_line_uturn = true;
              break;
            }
          }
          if (has_line_uturn) {
            const std::string incoming_id = curves[h.curve].id;
            for (size_t ci = 0; ci < cand_idx.size(); ++ci) {
              const double cw = cand_cw[ci];
              const ExactCurve& cc = curves[half_edges[cand_idx[ci]].curve];
              if (cc.id == incoming_id &&
                  std::abs(cw - kExactPi) <= kTangentTieBreak) {
                best = cand_idx[ci];
                best_cw = cw;
                best_is_same_id = true;
                break;
              }
            }
          }
        }
      }

      if (best < 0) {
        const std::string incoming_id = curves[h.curve].id;
        for (size_t ci = 0; ci < cand_idx.size(); ++ci) {
          const int cand = cand_idx[ci];
          const double cw = cand_cw[ci];
          const ExactCurve& cc = curves[half_edges[cand].curve];
          const bool same_id = cc.id == incoming_id;
          if (best < 0 || cw > best_cw + 1e-12 ||
              (same_id && !best_is_same_id &&
               best_cw - cw <= kTangentTieBreak) ||
              (std::abs(cw - best_cw) <= 1e-12 && cand < best)) {
            best_cw = cw;
            best = cand;
            best_is_same_id = same_id;
          }
        }
        // Same-entity preference, order-independent: when the winner
        // belongs to another entity and a same-entity candidate turns
        // nearly as sharply, stay on the entity.  The competing turns
        // at a tangent junction differ by ~1e-3 rad whose sign depends
        // on how the sketch was drawn, so this must not depend on
        // half-edge evaluation order.
        if (!best_is_same_id) {
          int rescued = -1;
          double rescued_cw = 0.0;
          for (size_t ci = 0; ci < cand_idx.size(); ++ci) {
            const ExactCurve& cc = curves[half_edges[cand_idx[ci]].curve];
            if (cc.id != incoming_id) continue;
            if (best_cw - cand_cw[ci] <= kTangentTieBreak &&
                (rescued < 0 || cand_cw[ci] > rescued_cw)) {
              rescued = cand_idx[ci];
              rescued_cw = cand_cw[ci];
            }
          }
          if (rescued >= 0) {
            best = rescued;
            best_cw = rescued_cw;
          }
        }
      }
      if (best < 0) break;
      if (kTraceFaces) {
        log_debug("exact_profiles",
                  "face walk pick=" + curves[half_edges[best].curve].id);
      }
      cur = best;
      if (cur == start) { closed = true; break; }
    }
    if (kTraceFaces) {
      log_debug("exact_profiles",
                "face walk end closed=" + std::to_string(closed) +
                    " edges=" + std::to_string(walk.size()));
    }
    if (!closed) continue;

    ExactFace face{.edges = walk, .signed_area = 0.0, .ccw = false};
    for (const int e : walk) {
      const ExactHalfEdge& h = half_edges[e];
      const ExactCurve& c = curves[h.curve];
      face.signed_area +=
          exact_half_edge_area(c, h.param_start, h.param_end,
                               {nodes[h.node_start].x, nodes[h.node_start].y},
                               {nodes[h.node_end].x, nodes[h.node_end].y});
    }
    if (std::abs(face.signed_area) <= kProfileTolerance) continue;
    for (const int w : walk) half_edges[w].used = true;
    face.ccw = face.signed_area > 0.0;
    faces.push_back(std::move(face));
  }

  // 6. Regions from faces.
  struct FaceGeo {
    std::vector<SketchProfilePoint> points;
    std::vector<std::string> ids;
    std::vector<std::string> vertex_keys;
    double abs_area = 0.0;
  };
  std::vector<FaceGeo> face_geo(faces.size());
  for (size_t f = 0; f < faces.size(); ++f) {
    FaceGeo& geo = face_geo[f];
    for (const int e : faces[f].edges) {
      const ExactHalfEdge& h = half_edges[e];
      const ExactCurve& c = curves[h.curve];
      geo.points.push_back(
          SketchProfilePoint{.x = nodes[h.node_start].x,
                             .y = nodes[h.node_start].y});
      geo.ids.push_back(c.id);
      geo.vertex_keys.push_back(
          make_node_key({nodes[h.node_start].x, nodes[h.node_start].y}));
      if (c.kind != ExactCurve::Kind::kLine) {
        const double span = h.param_end - h.param_start;
        for (int i = 1; i < kArcSampleSegments; ++i) {
          const double a = h.param_start +
                           span * static_cast<double>(i) / kArcSampleSegments;
          const SketchProfilePoint p = exact_curve_point(c, a);
          geo.points.push_back(p);
          geo.ids.push_back(c.id);
          geo.vertex_keys.push_back(make_node_key({p.x, p.y}));
        }
      }
    }
    geo.abs_area = std::abs(faces[f].signed_area);
  }

  // CCW faces sorted by area ascending (smallest first) for hole pairing.
  std::vector<size_t> ccw_idx;
  for (size_t f = 0; f < faces.size(); ++f) {
    if (faces[f].ccw) ccw_idx.push_back(f);
  }
  std::sort(ccw_idx.begin(), ccw_idx.end(), [&](size_t a, size_t b) {
    return face_geo[a].abs_area < face_geo[b].abs_area;
  });

  struct HoleLoop {
    std::vector<SketchProfilePoint> points;
    std::string first_entity_id;
  };
  struct RegionBuild {
    size_t face_idx;
    std::vector<HoleLoop> holes;
  };
  std::vector<RegionBuild> builds;
  for (const size_t f : ccw_idx) {
    builds.push_back(RegionBuild{.face_idx = f});
  }

  // Entity-id set of a face (sorted, unique) — used to recognise the
  // exterior twin of a boundary (same entities traversed the other way),
  // which must never become a hole of its own region.
  auto face_entity_set = [&](size_t f) {
    std::vector<std::string> ids;
    for (const int e : faces[f].edges) {
      ids.push_back(curves[half_edges[e].curve].id);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
  };

  for (size_t f = 0; f < faces.size(); ++f) {
    if (faces[f].ccw) continue;
    // Probe point on the hole cycle: midpoint of the first edge's
    // parameter span (strictly inside any enclosing region — hole
    // boundaries do not cross the outer boundary).
    const ExactHalfEdge& h0 = half_edges[faces[f].edges.front()];
    const ExactCurve& c0 = curves[h0.curve];
    const double mid = 0.5 * (h0.param_start + h0.param_end);
    const SketchProfilePoint probe = exact_curve_point(c0, mid);
    const auto cycle_ids = face_entity_set(f);

    size_t best_f = 0;
    bool found = false;
    double best_area = std::numeric_limits<double>::max();
    for (const size_t cf : ccw_idx) {
      // The reverse traversal of the same boundary is the exterior of
      // an isolated component — not a hole.
      if (face_entity_set(cf) == cycle_ids) continue;
      const FaceGeo& geo = face_geo[cf];
      if (!exact_point_in_polygon(probe, geo.points)) continue;
      if (geo.abs_area < best_area) {
        best_area = geo.abs_area;
        best_f = cf;
        found = true;
      }
    }
    if (!found) continue;  // exterior cycle of the arrangement
    for (auto& b : builds) {
      if (b.face_idx == best_f) {
        b.holes.push_back(HoleLoop{
            .points = face_geo[f].points,
            .first_entity_id = face_geo[f].ids.empty()
                                   ? std::string()
                                   : face_geo[f].ids.front(),
        });
        break;
      }
    }
  }

  const bool circles_only = !has_solid_lines && !has_solid_arcs;

  for (const auto& b : builds) {
    const ExactFace& face = faces[b.face_idx];
    const FaceGeo& geo = face_geo[b.face_idx];

    // Full-circle face: every boundary edge belongs to the same circle
    // entity, all walked in the same direction, and the total angular
    // span covers 2π.  An unsplit circle yields one self-closing edge;
    // a circle touched by lines yields several pieces.
    bool full_circle = false;
    const ExactCurve* circle_curve = nullptr;
    if (!face.edges.empty()) {
      const ExactCurve& c0 = curves[half_edges[face.edges[0]].curve];
      if (c0.kind == ExactCurve::Kind::kCircle) {
        bool all_same = true;
        bool all_forward_same = true;
        double total_span = 0.0;
        for (const int e : face.edges) {
          const ExactHalfEdge& h = half_edges[e];
          if (curves[h.curve].id != c0.id) { all_same = false; break; }
          total_span += std::abs(h.param_end - h.param_start);
          if (h.forward != half_edges[face.edges[0]].forward) {
            all_forward_same = false;
          }
        }
        if (all_same && all_forward_same &&
            std::abs(total_span - 2.0 * kExactPi) < 1e-9) {
          full_circle = true;
          circle_curve = &c0;
        }
      }
    }

    std::vector<std::string> unique_ids = geo.ids;
    std::sort(unique_ids.begin(), unique_ids.end());
    unique_ids.erase(std::unique(unique_ids.begin(), unique_ids.end()),
                     unique_ids.end());

    SketchProfileRegion region;
    region.vertex_ids = geo.vertex_keys;
    region.line_ids = unique_ids;
    region.ordered_edge_ids = geo.ids;
    region.points = geo.points;
    for (const auto& hole : b.holes) {
      region.inner_loops.push_back(hole.points);
    }
    region.center_x = 0.0;
    region.center_y = 0.0;
    region.radius = 0.0;

    // Arc descriptors for circular pieces of circle entities (kept for
    // legacy consumers).
    for (const int e : face.edges) {
      const ExactHalfEdge& h = half_edges[e];
      const ExactCurve& c = curves[h.curve];
      if (c.kind != ExactCurve::Kind::kCircle) continue;
      region.arc_descriptors.push_back(SketchArcDescriptor{
          .circle_id = c.id,
          .center_x = c.cx,
          .center_y = c.cy,
          .radius = c.r,
          .start_angle_rad = exact_wrap_angle(h.param_start),
          .end_angle_rad = exact_wrap_angle(h.param_end),
          .ccw = h.forward,
      });
    }

    // Exact boundary edges (walk order) — the wire builder's primary
    // input.
    for (const int e : face.edges) {
      const ExactHalfEdge& h = half_edges[e];
      const ExactCurve& c = curves[h.curve];
      ProfileBoundaryEdge be;
      be.entity_id = c.id;
      be.entity_kind = c.kind == ExactCurve::Kind::kLine
                           ? "line"
                           : (c.kind == ExactCurve::Kind::kCircle ? "circle"
                                                                  : "arc");
      be.param_start = h.param_start;
      be.param_end = h.param_end;
      be.start_x = nodes[h.node_start].x;
      be.start_y = nodes[h.node_start].y;
      be.end_x = nodes[h.node_end].x;
      be.end_y = nodes[h.node_end].y;
      if (c.kind != ExactCurve::Kind::kLine) {
        be.center_x = c.cx;
        be.center_y = c.cy;
        be.radius = c.r;
        const bool inc_is_ccw =
            (c.kind == ExactCurve::Kind::kCircle) || c.ccw;
        be.ccw = h.forward ? inc_is_ccw : !inc_is_ccw;
      }
      region.boundary_edges.push_back(std::move(be));
    }

    if (full_circle) {
      if (circles_only && b.holes.empty()) {
        region.kind = "circle";
        region.id = "profile-circle-" + circle_curve->id;
        region.points = {};
        region.vertex_ids = {circle_center_vertex_id_for(
            parameters, circle_curve->id)};
        region.center_x = circle_curve->cx;
        region.center_y = circle_curve->cy;
        region.radius = circle_curve->r;
      } else {
        region.kind = "polygon";
        region.points = exact_sample_circle_loop(*circle_curve);
        region.source_circle_id = circle_curve->id;
        region.id = make_polygon_profile_id(unique_ids) + "-" +
                    make_polygon_profile_id(geo.vertex_keys);
      }
    } else {
      region.kind = "polygon";
      region.id = make_polygon_profile_id(unique_ids) + "-" +
                  make_polygon_profile_id(geo.vertex_keys);
    }
    for (const auto& hole : b.holes) {
      region.id += "-hole-" + hole.first_entity_id;
    }
    regions.push_back(std::move(region));
  }

  return regions;
}
