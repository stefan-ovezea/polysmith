#include "core/drawing/drawing_dimension_geometry.h"

#include <algorithm>
#include <cmath>

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

// ── ISO 129-1 annotation constants (sheet-mm, view-scale free) ────
constexpr double kThinLineMm = 0.25;       // extension/dimension lines
constexpr double kDimGapMm = 2.0;          // 8 × line width gap
constexpr double kDimOvershootMm = 2.0;    // extension overshoot
constexpr double kDimOffsetMm = 8.0;       // dimension line off the feature
constexpr double kArrowLengthMm = 3.0;     // closed filled 30° arrowhead
constexpr double kArrowHalfWidthMm = 0.75; // (3:1 length:width)
constexpr double kTextHeightMm = 3.5;      // ISO 3098 dimension numerals
constexpr double kTextGapMm = 1.0;         // text above the dimension line
constexpr double kAngularRadiusMm = 12.0;  // angle arc radius

using Pt = std::array<double, 2>;

double dist2(const Pt& a, const Pt& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  return std::sqrt(dx * dx + dy * dy);
}

Pt operator+(const Pt& a, const Pt& b) { return {a[0] + b[0], a[1] + b[1]}; }
Pt operator-(const Pt& a, const Pt& b) { return {a[0] - b[0], a[1] - b[1]}; }
Pt operator*(const Pt& a, double k) { return {a[0] * k, a[1] * k}; }

double dot(const Pt& a, const Pt& b) { return a[0] * b[0] + a[1] * b[1]; }
double cross(const Pt& a, const Pt& b) { return a[0] * b[1] - a[1] * b[0]; }

Pt unit(const Pt& a) {
  const double len = std::sqrt(dot(a, a));
  if (len < 1e-12) {
    return {1.0, 0.0};
  }
  return a * (1.0 / len);
}

Pt perp(const Pt& a) { return {-a[1], a[0]}; }

SheetPrimitive thin_line(const Pt& a, const Pt& b) {
  SheetPrimitive p;
  p.kind = "line";
  p.p0 = a;
  p.p1 = b;
  p.style = {"continuous", kThinLineMm};
  p.purpose = "dimension";
  return p;
}

/// Closed filled arrowhead: tip at `tip`, pointing along `dir_out`
/// (unit).  The base is perpendicular at `len` back from the tip.
SheetPrimitive arrow(const Pt& tip, const Pt& dir_out) {
  SheetPrimitive p;
  p.kind = "filled_poly";
  const Pt back = tip - dir_out * kArrowLengthMm;
  const Pt side = perp(dir_out) * kArrowHalfWidthMm;
  p.points = {tip, back + side, back - side};
  p.style = {"continuous", kThinLineMm};
  p.purpose = "dimension";
  return p;
}

SheetText dimension_text(const std::string& text, const Pt& position,
                         bool stale) {
  SheetText t;
  t.text = text;
  t.position = position;
  t.height_mm = kTextHeightMm;
  t.purpose = "dimension";
  t.stale = stale;
  return t;
}

/// Where the dimension sits relative to the feature: +1 on one side,
/// −1 flipped (arrow_flip).
double outward_side(bool arrow_flip) { return arrow_flip ? -1.0 : 1.0; }

/// Applies the view transform (view-mm → sheet-mm) to a point.
Pt to_sheet(const DrawingView& view, const Pt& p) {
  return {p[0] * view.scale + view.sheet_position[0],
          p[1] * view.scale + view.sheet_position[1]};
}

/// Text position shifted by the annotation's cosmetic offset.
Pt apply_text_offset(const Annotation& annotation, const Pt& position) {
  if (annotation.text_offset.has_value()) {
    return {position[0] + annotation.text_offset.value()[0],
            position[1] + annotation.text_offset.value()[1]};
  }
  return position;
}

/// Semantic dimension record for the annotated-DXF backend (P9):
/// the same sheet-mm points the primitives were built from.
SheetDimension linear_semantic(const std::string& text, const Pt& def_point,
                               const Pt& text_point, const Pt& def1,
                               const Pt& def2) {
  SheetDimension s;
  s.kind = "linear";
  s.def_point = def_point;
  s.text_point = text_point;
  s.def1 = def1;
  s.def2 = def2;
  s.text = text;
  return s;
}

// ── Case builders ─────────────────────────────────────────────────

void emit_linear_single(DimensionGraphics* out, const ResolvedDimension& r,
                        const Annotation& annotation, const DrawingView& view,
                        bool stale) {
  const Pt a = to_sheet(view, r.a_p0);
  const Pt b = to_sheet(view, r.a_p1);
  const Pt d = unit(b - a);
  const Pt n = perp(d) * outward_side(annotation.arrow_flip);
  const double text_rise = kTextGapMm + kTextHeightMm / 2.0;
  const Pt pa0 = a + n * kDimOffsetMm;
  const Pt pb0 = b + n * kDimOffsetMm;
  const Pt mid = (pa0 + pb0) * 0.5 + n * text_rise;  // default text anchor
  const Pt t = apply_text_offset(annotation, mid);   // dragged text anchor
  // Dragging the text moves the whole dimension: the dimension line
  // follows the text's PERPENDICULAR component (extension lines
  // stretch), the parallel component slides the text along the line.
  // The delta formulation keeps the no-offset layout bit-exact (the
  // default dim line is kDimOffsetMm from the feature).
  const double o = kDimOffsetMm + dot(t - mid, n);
  // Extension lines: gap off the feature, overshoot past the
  // dimension line.
  out->primitives.push_back(
      thin_line(a + n * kDimGapMm, a + n * (o + kDimOvershootMm)));
  out->primitives.push_back(
      thin_line(b + n * kDimGapMm, b + n * (o + kDimOvershootMm)));
  const Pt pa = a + n * o;
  const Pt pb = b + n * o;
  out->primitives.push_back(thin_line(pa, pb));
  out->primitives.push_back(arrow(pa, d * -1.0));  // outward at the left end
  out->primitives.push_back(arrow(pb, d));         // outward at the right end
  out->text = dimension_text(r.text, t, stale);
  out->semantic = linear_semantic(r.text, pa, t, a, b);
}

void emit_linear_two_lines(DimensionGraphics* out, const ResolvedDimension& r,
                           const Annotation& annotation, const DrawingView& view,
                           bool stale) {
  const Pt a0 = to_sheet(view, r.a_p0);
  const Pt a1 = to_sheet(view, r.a_p1);
  const Pt b0 = to_sheet(view, r.b_p0.value());
  const Pt b1 = to_sheet(view, r.b_p1.value());
  const Pt d = unit(a1 - a0);
  const Pt n = perp(d);
  // Perpendicular connector between the two (parallel) lines, clamped
  // to the second line's span (standard CAD behavior).
  Pt p0 = (a0 + a1) * 0.5;
  Pt p1 = p0 + n * cross(d, b0 - p0);
  const Pt bd = b1 - b0;
  const double b_len2 = dot(bd, bd);
  if (b_len2 > 1e-12) {
    const double t = std::clamp(dot(p1 - b0, bd) / b_len2, 0.0, 1.0);
    p1 = b0 + bd * t;
  }
  const Pt dir = unit(p1 - p0);
  out->primitives.push_back(thin_line(p0, p1));
  out->primitives.push_back(arrow(p0, dir * -1.0));
  out->primitives.push_back(arrow(p1, dir));
  const Pt mid = (p0 + p1) * 0.5 +
                 n * outward_side(annotation.arrow_flip) *
                     (kTextGapMm + kTextHeightMm / 2.0);
  out->text = dimension_text(r.text,
                             apply_text_offset(annotation, mid), stale);
  // The connector IS perpendicular to both parallel lines, so an
  // aligned dimension between its ends measures exactly the value.
  out->semantic =
      linear_semantic(r.text, p0, apply_text_offset(annotation, mid), p0, p1);
}

void emit_linear_line_circle(DimensionGraphics* out, const ResolvedDimension& r,
                             const Annotation& annotation, const DrawingView& view,
                             bool stale) {
  // The line is attachment A, the circle B (see measure_records).
  const Pt a0 = to_sheet(view, r.a_p0);
  const Pt a1 = to_sheet(view, r.a_p1);
  const Pt c = to_sheet(view, r.b_center.value());
  const Pt d = unit(a1 - a0);
  const Pt n = perp(d);
  // Foot of the center onto the (infinite) line, clamped to its span.
  Pt p0 = a0 + d * std::clamp(dot(c - a0, d), 0.0, dist2(a0, a1));
  const Pt dir = unit(c - p0);
  out->primitives.push_back(thin_line(p0, c));
  out->primitives.push_back(arrow(p0, dir * -1.0));
  out->primitives.push_back(arrow(c, dir));
  const Pt mid = (p0 + c) * 0.5 +
                 n * outward_side(annotation.arrow_flip) *
                     (kTextGapMm + kTextHeightMm / 2.0);
  out->text = dimension_text(r.text,
                             apply_text_offset(annotation, mid), stale);
  // p0 is the perpendicular foot of the center onto the line, so the
  // aligned dimension measures exactly the value.
  out->semantic =
      linear_semantic(r.text, p0, apply_text_offset(annotation, mid), p0, c);
}

void emit_linear_two_circles(DimensionGraphics* out, const ResolvedDimension& r,
                             const Annotation& annotation, const DrawingView& view,
                             bool stale) {
  const Pt c0 = to_sheet(view, r.a_p0);              // a center
  const Pt c1 = to_sheet(view, r.b_center.value());  // b center
  const Pt dir = unit(c1 - c0);
  out->primitives.push_back(thin_line(c0, c1));
  out->primitives.push_back(arrow(c0, dir * -1.0));
  out->primitives.push_back(arrow(c1, dir));
  const Pt n = perp(dir) * outward_side(annotation.arrow_flip);
  const Pt mid = (c0 + c1) * 0.5 + n * (kTextGapMm + kTextHeightMm / 2.0);
  out->text = dimension_text(r.text,
                             apply_text_offset(annotation, mid), stale);
  out->semantic =
      linear_semantic(r.text, c0, apply_text_offset(annotation, mid), c0, c1);
}

void emit_radius(DimensionGraphics* out, const ResolvedDimension& r,
                 const Annotation& annotation, const DrawingView& view,
                 bool stale) {
  const Pt c = to_sheet(view, r.a_p0);  // center
  const Pt p = to_sheet(view, r.a_p1);  // arc point nearest the pick
  const Pt dir = unit(p - c);
  const Pt n = perp(dir) * outward_side(annotation.arrow_flip);
  const Pt mid = (c + p) * 0.5 + n * (kTextGapMm + kTextHeightMm / 2.0);
  const Pt t = apply_text_offset(annotation, mid);
  // With a placement offset the leader re-aims through the dragged
  // text (the center stays fixed); without one it keeps pointing at
  // the picked arc point.
  const double r_len = dist2(c, p);
  const Pt leader_dir = annotation.text_offset.has_value()
                            ? unit(t - c)
                            : dir;
  const Pt arc_pt = annotation.text_offset.has_value()
                        ? c + leader_dir * r_len
                        : p;
  out->primitives.push_back(thin_line(c, arc_pt));
  out->primitives.push_back(arrow(arc_pt, leader_dir));
  out->text = dimension_text(r.text, t, stale);
  SheetDimension s;
  s.kind = "radius";
  s.def_point = c;  // center
  s.text_point = t;
  s.arc_point = arc_pt;  // point on the arc
  s.leader_length = r_len;
  s.text = r.text;
  out->semantic = std::move(s);
}

void emit_diameter(DimensionGraphics* out, const ResolvedDimension& r,
                   const Annotation& annotation, const DrawingView& view,
                   bool stale) {
  const Pt c = to_sheet(view, r.a_p0);
  const Pt p = to_sheet(view, r.a_p1);
  const Pt q = c - (p - c);  // opposite arc point
  const Pt dir = unit(p - q);
  const Pt n = perp(dir) * outward_side(annotation.arrow_flip);
  const Pt mid = c + n * (kTextGapMm + kTextHeightMm / 2.0);
  const Pt t = apply_text_offset(annotation, mid);
  // With a placement offset the dimension line stays through the
  // center but re-aims toward the dragged text; without one it keeps
  // the picked direction.
  const double r_len = dist2(c, p);
  const Pt axis_dir = annotation.text_offset.has_value() ? unit(t - c) : dir;
  const Pt p2 = annotation.text_offset.has_value() ? c + axis_dir * r_len : p;
  const Pt q2 = annotation.text_offset.has_value() ? c - axis_dir * r_len : q;
  out->primitives.push_back(thin_line(q2, p2));
  out->primitives.push_back(arrow(q2, axis_dir * -1.0));
  out->primitives.push_back(arrow(p2, axis_dir));
  out->text = dimension_text(r.text, t, stale);
  SheetDimension s;
  s.kind = "diameter";
  s.def_point = q2;  // opposite arc point
  s.text_point = t;
  s.arc_point = p2;  // first arc point
  s.leader_length = r_len;
  s.text = r.text;
  out->semantic = std::move(s);
}

void emit_angular(DimensionGraphics* out, const ResolvedDimension& r,
                  const Annotation& annotation, const DrawingView& view,
                  bool stale) {
  // Intersection of the two lines as the apex; the sector contains
  // both span midpoints (the shorter way around).
  const Pt a0 = to_sheet(view, r.a_p0);
  const Pt a1 = to_sheet(view, r.a_p1);
  const Pt b0 = to_sheet(view, r.b_p0.value());
  const Pt b1 = to_sheet(view, r.b_p1.value());
  Pt u1 = unit(a1 - a0);
  Pt u2 = unit(b1 - b0);
  // Line-line intersection (lines are known non-parallel — the
  // resolver rejected parallel pairs).
  const Pt da = a1 - a0;
  const Pt db = b1 - b0;
  const double denom = cross(da, db);
  if (std::abs(denom) < 1e-12) {
    return;  // defensive — the resolver already rejected parallel
  }
  const double t = cross(b0 - a0, db) / denom;
  const Pt apex = a0 + da * t;
  // Orient both rays away from the apex.
  if (dot(u1, (a0 + a1) * 0.5 - apex) < 0.0) {
    u1 = u1 * -1.0;
  }
  if (dot(u2, (b0 + b1) * 0.5 - apex) < 0.0) {
    u2 = u2 * -1.0;
  }
  const double a1_ang = std::atan2(u1[1], u1[0]);
  const double a2_ang = std::atan2(u2[1], u2[0]);
  // The shorter sweep between the rays.
  double sweep = a2_ang - a1_ang;
  while (sweep > kPi) {
    sweep -= 2.0 * kPi;
  }
  while (sweep < -kPi) {
    sweep += 2.0 * kPi;
  }
  const double mid_ang = a1_ang + sweep / 2.0;
  Pt text_pos = apex + Pt{std::cos(mid_ang), std::sin(mid_ang)} *
                            (kAngularRadiusMm + 6.0);
  const Pt text_anchor = apply_text_offset(annotation, text_pos);
  // With a placement offset the arc radius follows the dragged text
  // (the default text sits 6 mm beyond the arc, clamped to the
  // default minimum radius); without one the arc keeps the default
  // radius bit-exactly.
  const double radius = annotation.text_offset.has_value()
                            ? std::max(dist2(text_anchor, apex) - 6.0,
                                       kAngularRadiusMm)
                            : kAngularRadiusMm;
  const Pt r1 = apex + u1 * radius;
  const Pt r2 = apex + u2 * radius;
  SheetPrimitive arc;
  arc.kind = "circle_arc";
  arc.p0 = r1;
  arc.p1 = r2;
  arc.center = apex;
  arc.radius = radius;
  arc.start_angle = a1_ang;
  arc.end_angle = a1_ang + sweep;
  arc.style = {"continuous", kThinLineMm};
  arc.purpose = "dimension";
  out->primitives.push_back(arc);
  // Arrows tangential to the arc ends, pointing along the sweep.
  const Pt tang1 = perp(u1) * (sweep >= 0.0 ? 1.0 : -1.0);
  out->primitives.push_back(arrow(r1, tang1));
  out->primitives.push_back(arrow(r2, tang1 * -1.0));
  out->text = dimension_text(r.text, text_anchor, stale);
  SheetDimension s;
  s.kind = "angular";
  s.def1 = apex;               // line 1-1
  s.def2 = r1;                 // line 1-2
  s.arc_point = apex;          // line 2-1 (shared apex)
  s.def_point = r2;            // line 2-2
  s.dim_point = apex + Pt{std::cos(mid_ang), std::sin(mid_ang)} *
                           radius;  // the arc location point
  s.text_point = text_anchor;
  s.text = r.text;
  out->semantic = std::move(s);
}

}  // namespace

DimensionGraphics build_dimension_graphics(const ResolvedDimension& resolved,
                                           const Annotation& annotation,
                                           const DrawingView& view,
                                           bool stale) {
  DimensionGraphics out;
  if (resolved.kind == "linear") {
    if (resolved.b_center.has_value()) {
      if (resolved.a_kind == "circle") {
        emit_linear_two_circles(&out, resolved, annotation, view, stale);
      } else {
        emit_linear_line_circle(&out, resolved, annotation, view, stale);
      }
    } else if (resolved.b_p0.has_value()) {
      emit_linear_two_lines(&out, resolved, annotation, view, stale);
    } else {
      emit_linear_single(&out, resolved, annotation, view, stale);
    }
  } else if (resolved.kind == "radius") {
    emit_radius(&out, resolved, annotation, view, stale);
  } else if (resolved.kind == "diameter") {
    emit_diameter(&out, resolved, annotation, view, stale);
  } else if (resolved.kind == "angular") {
    emit_angular(&out, resolved, annotation, view, stale);
  }
  return out;
}

}  // namespace polysmith::core
