#include "core/drawing/drawing_detail_clip.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

using Pt = std::array<double, 2>;

double dist2(const Pt& a, const Pt& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  return dx * dx + dy * dy;
}

double norm(const Pt& a) { return std::sqrt(a[0] * a[0] + a[1] * a[1]); }

double dot(const Pt& a, const Pt& b) { return a[0] * b[0] + a[1] * b[1]; }

/// A circle-piece record's point at an angular parameter.
Pt point_at(const ProjectedEdgeRecord& rec, double angle) {
  if (rec.curve_kind == "ellipse" && rec.ellipse_center.has_value() &&
      rec.ellipse_major_dir.has_value() && rec.ellipse_major_radius.has_value() &&
      rec.ellipse_minor_radius.has_value()) {
    const auto& c = rec.ellipse_center.value();
    const auto& md = rec.ellipse_major_dir.value();
    const double major = rec.ellipse_major_radius.value();
    const double minor = rec.ellipse_minor_radius.value();
    return {c[0] + md[0] * major * std::cos(angle) -
                md[1] * minor * std::sin(angle),
            c[1] + md[1] * major * std::cos(angle) +
                md[0] * minor * std::sin(angle)};
  }
  const auto& c = rec.circle_center.value();
  const double r = rec.circle_radius.value();
  return {c[0] + r * std::cos(angle), c[1] + r * std::sin(angle)};
}

struct Window {
  Pt center;
  double radius = 0.0;
  double radius2 = 0.0;
};

bool inside(const Window& w, const Pt& p) {
  return dist2(p, w.center) <= w.radius2 + 1e-12;
}

/// The record's angular sweep: |end - start|, with start == end (a
/// full circle) read as 2π — the record_sweep convention.
double sweep_of(const ProjectedEdgeRecord& rec) {
  const double sweep = std::abs(rec.end_angle - rec.start_angle);
  return sweep < 1e-9 ? 2.0 * kPi : sweep;
}

// ── Line clipping (analytic segment ∩ circle) ──────────────────────

/// Keeps the inside span of the segment [a, b].  Returns false when
/// the whole segment lies outside.
bool clip_line(const Window& w, const Pt& a, const Pt& b, Pt* out_a,
               Pt* out_b) {
  const bool a_in = inside(w, a);
  const bool b_in = inside(w, b);
  const Pt d = {b[0] - a[0], b[1] - a[1]};
  // Quadratic |a + t·d − c|² = r² → t²·|d|² + 2t·d·(a−c) + |a−c|² − r².
  const Pt ac = {a[0] - w.center[0], a[1] - w.center[1]};
  const double A = dot(d, d);
  if (A < 1e-18) {
    if (a_in) {
      *out_a = a;
      *out_b = b;
      return true;
    }
    return false;
  }
  const double B = 2.0 * dot(d, ac);
  const double C = dot(ac, ac) - w.radius2;
  double disc = B * B - 4.0 * A * C;
  if (disc < 0.0) {
    disc = 0.0;
  }
  const double sq = std::sqrt(disc);
  const double t0 = (-B - sq) / (2.0 * A);
  const double t1 = (-B + sq) / (2.0 * A);
  if (t1 < 0.0 || t0 > 1.0) {
    return false;  // the infinite line crosses outside the segment
  }
  const double from = std::clamp(t0, 0.0, 1.0);
  const double to = std::clamp(t1, 0.0, 1.0);
  // The span between the roots is inside the circle (the parabola
  // opens upward — negative inside).  Verify with the midpoint.
  const Pt mid = {a[0] + d[0] * (from + to) / 2.0,
                  a[1] + d[1] * (from + to) / 2.0};
  if (!inside(w, mid)) {
    return false;
  }
  *out_a = {a[0] + d[0] * from, a[1] + d[1] * from};
  *out_b = {a[0] + d[0] * to, a[1] + d[1] * to};
  return true;
}

// ── Circle-arc clipping (sampled angular inside-intervals) ─────────

/// Bisects an inside/outside transition on the record circle between
/// angles a0 (inside) and a1 (outside) to 1e-7 rad.
double bisect_crossing(const ProjectedEdgeRecord& rec, const Window& w,
                       double a_inside, double a_outside) {
  double lo = a_inside;
  double hi = a_outside;
  for (int i = 0; i < 60; ++i) {
    const double mid = (lo + hi) / 2.0;
    if (inside(w, point_at(rec, mid))) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return (lo + hi) / 2.0;
}

/// The kept angular sub-intervals [from, to] of the record's arc —
/// the arc parametrization is CCW from start_angle over sweep_of().
/// An inside region wrapping the 0/2π seam is split into two spans;
/// when the arc's own start lies inside, the trailing span merges
/// back with the leading one (the full-circle case).
std::vector<std::array<double, 2>> arc_inside_spans(
    const ProjectedEdgeRecord& rec, const Window& w) {
  const double start = rec.start_angle;
  const double sweep = sweep_of(rec);
  const int samples = 128;
  std::vector<std::array<double, 2>> spans;
  const bool first_inside = inside(w, point_at(rec, start));
  double span_start = first_inside ? start : 0.0;
  bool in_span = first_inside;
  // Sample the arc; every inside→outside / outside→inside transition
  // is bisected into an exact boundary angle.
  double prev_t = start;
  for (int i = 1; i <= samples; ++i) {
    const double t = start + sweep * i / samples;
    const bool in = inside(w, point_at(rec, t));
    if (in && !in_span) {
      span_start = bisect_crossing(rec, w, t, prev_t);
      in_span = true;
    } else if (!in && in_span) {
      spans.push_back({span_start, bisect_crossing(rec, w, prev_t, t)});
      in_span = false;
    }
    prev_t = t;
  }
  if (in_span) {
    // The arc ends inside.  On a FULL circle that also STARTS inside,
    // the trailing span and the leading span are one region — merge
    // them across the seam, unwrapping the end past 2π so the piece
    // keeps the end ≥ start sweep convention (record_sweep reads
    // |end − start|).
    const bool full = sweep >= 2.0 * kPi - 1e-9;
    if (full && first_inside && !spans.empty()) {
      spans[0][0] = span_start;
      spans[0][1] += 2.0 * kPi;
    } else {
      spans.push_back({span_start, start + sweep});
    }
  }
  return spans;
}

/// Clips one circle-piece record; appends the kept sub-records.
void clip_circle_record(const ProjectedEdgeRecord& rec, const Window& w,
                        std::vector<ProjectedEdgeRecord>* out) {
  const auto& c1 = rec.circle_center.value();
  const double r1 = rec.circle_radius.value();
  const double d = std::sqrt(dist2(c1, w.center));
  if (d < 1e-12) {
    // Concentric: the whole record is inside iff r1 ≤ radius.
    if (r1 <= w.radius + 1e-9) {
      out->push_back(rec);
    }
    return;
  }
  // |c1 + r1·u(θ) − c2| ≤ r2  ⇔  cos(θ − φ) ≥ m with
  // m = (d² + r1² − r2²) / (2·d·r1).  m ≤ −1: every angle is inside
  // (keep whole); m ≥ 1: none (drop).  In between, the sub-arcs are
  // found by sampling (arc_inside_spans).
  const double m = (d * d + r1 * r1 - w.radius2) / (2.0 * d * r1);
  if (m <= -1.0) {
    out->push_back(rec);
    return;
  }
  if (m >= 1.0) {
    return;
  }
  // The inside set = the arc (phi − gamma, phi + gamma).  The
  // sub-arcs are found by sampling the arc for inside/outside
  // transitions and bisecting the crossings (arc_inside_spans) —
  // exact within the bisection tolerance, and immune to
  // seam-wrapping bookkeeping.
  for (const auto& span : arc_inside_spans(rec, w)) {
    ProjectedEdgeRecord piece = rec;
    piece.start_angle = span[0];
    piece.end_angle = span[1];
    piece.p_start = point_at(rec, span[0]);
    piece.p_end = point_at(rec, span[1]);
    out->push_back(std::move(piece));
  }
}

// ── Heuristic clips (non-analytic curves) ──────────────────────────

/// Ellipses / bsplines: kept only when fully inside (endpoints + the
/// parametric midpoint) — a crossing window drops the record (the V1
/// limit, documented in the header).
bool fully_inside(const ProjectedEdgeRecord& rec, const Window& w) {
  if (!inside(w, rec.p_start) || !inside(w, rec.p_end)) {
    return false;
  }
  Pt mid = rec.p_start;
  if (rec.curve_kind == "ellipse" && rec.ellipse_center.has_value()) {
    mid = point_at(rec, (rec.start_angle + rec.end_angle) / 2.0);
  } else if (rec.curve_kind == "circle" && rec.circle_center.has_value()) {
    mid = point_at(rec, (rec.start_angle + rec.end_angle) / 2.0);
  } else {
    mid = {(rec.p_start[0] + rec.p_end[0]) / 2.0,
           (rec.p_start[1] + rec.p_end[1]) / 2.0};
  }
  return inside(w, mid);
}

}  // namespace

ProjectionResult clip_projection_to_circle(const ProjectionResult& source,
                                           const std::array<double, 2>& center,
                                           double radius) {
  ProjectionResult out;
  out.source_revision = source.source_revision;
  out.stale = source.stale;
  if (!(radius > 0.0)) {
    return out;  // degenerate window: nothing is inside
  }
  const Window window{center, radius, radius * radius};
  for (const auto& rec : source.edges) {
    if (rec.curve_kind == "line") {
      ProjectedEdgeRecord piece = rec;
      if (clip_line(window, rec.p_start, rec.p_end, &piece.p_start,
                    &piece.p_end)) {
        out.edges.push_back(std::move(piece));
      }
      continue;
    }
    if (rec.curve_kind == "circle" && rec.circle_center.has_value() &&
        rec.circle_radius.has_value()) {
      clip_circle_record(rec, window, &out.edges);
      continue;
    }
    if (fully_inside(rec, window)) {
      out.edges.push_back(rec);
    }
  }
  // Hatch regions: detail parents are projection views (validation) —
  // nothing to inherit.
  return out;
}

}  // namespace polysmith::core
