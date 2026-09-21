#include "core/drawing/drawing_annotation_geometry.h"

#include <algorithm>
#include <cmath>

namespace polysmith::core {

namespace {

// ── Annotation constants (sheet-mm, view-scale free) ───────────────
constexpr double kPi = 3.14159265358979323846;
constexpr double kThinLineMm = 0.25;        // annotation lines
constexpr double kArrowLengthMm = 3.0;      // closed filled 30° arrowhead
constexpr double kArrowHalfWidthMm = 0.75;  // (3:1 length:width)
constexpr double kTextHeightMm = 3.5;       // ISO 3098 annotation text
constexpr double kLeaderDefaultLengthMm = 6.0;  // text-center distance
constexpr double kLeaderLandingGapMm = 1.0;     // leader stops short of text
// GEOMETRY tab:
constexpr double kCenterMarkOverrunMm = 2.5;    // center-mark cross arm
constexpr double kCenterlineOverrunMm = 3.0;    // chain line past the centers
constexpr double kEdgeExtensionMm = 5.0;        // outward from the chosen end
// SYMBOLS tab:
constexpr double kSurfaceFinishLongLegMm = 4.2;   // ISO 1302 check mark
constexpr double kSurfaceFinishShortLegMm = 2.1;
constexpr double kWeldingRefHalfMm = 5.0;      // reference line half-length
constexpr double kToleranceFrameWideMm = 7.0;  // compartment width
constexpr double kToleranceFrameHighMm = 7.0;  // frame height
constexpr double kDatumTriangleHighMm = 3.0;   // filled triangle height
constexpr double kDatumTriangleHalfMm = 2.0;   // base half-width
constexpr double kDatumBoxMm = 3.5;            // letter box side
constexpr double kBalloonRadiusMm = 3.5;

using Pt = std::array<double, 2>;

Pt operator+(const Pt& a, const Pt& b) { return {a[0] + b[0], a[1] + b[1]}; }
Pt operator-(const Pt& a, const Pt& b) { return {a[0] - b[0], a[1] - b[1]}; }
Pt operator*(const Pt& a, double k) { return {a[0] * k, a[1] * k}; }

double dot(const Pt& a, const Pt& b) { return a[0] * b[0] + a[1] * b[1]; }

double norm(const Pt& a) { return std::sqrt(dot(a, a)); }

Pt unit(const Pt& a) {
  const double len = norm(a);
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
  p.purpose = "annotation";
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
  p.purpose = "annotation";
  return p;
}

SheetText annotation_text(const std::string& text, const Pt& position,
                          bool stale) {
  SheetText t;
  t.text = text;
  t.position = position;
  t.height_mm = kTextHeightMm;
  t.purpose = "annotation";
  t.stale = stale;
  return t;
}

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

/// Default leader direction for point-anchored symbols: radial outward
/// for circles, the edge-perpendicular for lines.
Pt default_dir(const ResolvedAttachment& r, const DrawingView& view) {
  if (r.a_kind == "circle" && r.a_center.has_value()) {
    return unit(to_sheet(view, r.attach_point) - to_sheet(view, r.a_center.value()));
  }
  return perp(unit(to_sheet(view, r.a_p1) - to_sheet(view, r.a_p0)));
}

/// ISO 128-2 chain line (type H: 24d dash / 3d gap / d dot / 3d gap at
/// the line width) — the final dash extends to the end point (the
/// Annex-A corner rule).  Mirrors the flatten's dash_spans
/// (drawing_sheet.cpp): annotation primitives bypass the view-geometry
/// dashing pass, so the emitter pre-dashes into continuous pieces.
void emit_chain_line(std::vector<SheetPrimitive>& out, const Pt& a,
                     const Pt& b) {
  const double w = kThinLineMm;
  const double dash = 24.0 * w;
  const double gap = 3.0 * w;
  const double dot_len = w;
  const double cycle = dash + gap + dot_len + gap;
  const double length = norm(b - a);
  if (length < 1e-9) {
    return;
  }
  const Pt u = unit(b - a);
  double cursor = 0.0;
  while (cursor < length - 1e-9) {
    out.push_back(thin_line(a + u * cursor,
                            a + u * std::min(cursor + dash, length)));
    const double dot_from = cursor + dash + gap;
    if (dot_from < length - 1e-9) {
      out.push_back(thin_line(a + u * dot_from,
                              a + u * std::min(dot_from + dot_len, length)));
    }
    cursor += cycle;
  }
}

// ── Case builders ─────────────────────────────────────────────────

/// Leader text (ANNOTATE tab): an arrow at the attachment point on
/// the edge, a straight leader, and the user text.  The text lands at
/// its default anchor plus text_offset; with an offset the leader
/// re-aims through the dragged text (the attachment point stays fixed
/// on the edge) — the same placement-point semantics as the radius
/// dimension.
void emit_leader_text(AnnotationGraphics* out, const ResolvedAttachment& r,
                      const Annotation& annotation, const DrawingView& view,
                      bool stale) {
  const Pt attach = to_sheet(view, r.attach_point);
  const Pt d0 = default_dir(r, view);
  const Pt anchor = attach + d0 * kLeaderDefaultLengthMm;
  const Pt t = apply_text_offset(annotation, anchor);
  const Pt dir = annotation.text_offset.has_value() ? unit(t - attach) : d0;
  // The leader stops just short of the text's near edge (no glyph
  // metrics needed — the text is anchored at its center).
  const Pt stop = t - dir * (kTextHeightMm / 2.0 + kLeaderLandingGapMm);
  out->primitives.push_back(thin_line(attach, stop));
  out->primitives.push_back(arrow(attach, dir));
  out->text = annotation_text(r.text, t, stale);
}

/// Center mark (GEOMETRY): a thin cross centered on the circle,
/// arms extending past the (unmarked) outline.
void emit_center_mark(AnnotationGraphics* out, const ResolvedAttachment& r,
                      const DrawingView& view) {
  if (!r.a_center.has_value()) {
    return;
  }
  const Pt c = to_sheet(view, r.a_center.value());
  out->primitives.push_back(thin_line(
      c + Pt{-kCenterMarkOverrunMm, 0.0}, c + Pt{kCenterMarkOverrunMm, 0.0}));
  out->primitives.push_back(thin_line(
      c + Pt{0.0, -kCenterMarkOverrunMm}, c + Pt{0.0, kCenterMarkOverrunMm}));
}

/// Centerline (GEOMETRY): a chain line through both circle centers,
/// overshooting them by kCenterlineOverrunMm on each side.
void emit_centerline(AnnotationGraphics* out, const ResolvedAttachment& r,
                     const DrawingView& view) {
  if (!r.a_center.has_value() || !r.b_center.has_value()) {
    return;
  }
  const Pt a = to_sheet(view, r.a_center.value());
  const Pt b = to_sheet(view, r.b_center.value());
  if (norm(b - a) < 1e-9) {
    return;  // concentric centers — rejected at validation
  }
  const Pt u = unit(b - a);
  emit_chain_line(out->primitives, a - u * kCenterlineOverrunMm,
                  b + u * kCenterlineOverrunMm);
}

/// Edge extension (GEOMETRY): a thin line kEdgeExtensionMm outward
/// from the chosen end (attach_param snaps to 0/1 at create time).
void emit_edge_extension(AnnotationGraphics* out, const ResolvedAttachment& r,
                         const Annotation& annotation,
                         const DrawingView& view) {
  const double f = annotation.attach_param.value_or(0.0);
  const Pt from = to_sheet(view, r.attach_point);
  // Outward = away from the edge interior (toward the far end is IN).
  const Pt far = to_sheet(view, f < 0.5 ? r.a_p1 : r.a_p0);
  const Pt d = unit(from - far);
  out->primitives.push_back(thin_line(from, from + d * kEdgeExtensionMm));
}

/// Surface finish (SYMBOLS): the ISO 1302 basic check mark (60° legs,
/// the long leg carrying the value text to its right) rooted on the
/// attachment point.
void emit_surface_finish(AnnotationGraphics* out, const ResolvedAttachment& r,
                         const DrawingView& view, bool stale) {
  const Pt root = to_sheet(view, r.attach_point);
  const Pt up = Pt{0.5, 0.866};  // 60° above horizontal
  out->primitives.push_back(
      thin_line(root, root + up * kSurfaceFinishLongLegMm));
  out->primitives.push_back(thin_line(
      root, root + Pt{-up[0], up[1]} * kSurfaceFinishShortLegMm));
  SheetText t = annotation_text(r.text, root + Pt{2.8, 2.2}, stale);
  t.h_align = "left";  // value beside the check mark
  out->text = t;
}

/// Welding (SYMBOLS): an arrow into the attachment point, a short
/// leader, and the ISO 2553 reference line through the elbow with the
/// weld symbol text above its left end.  The whole symbol shifts by
/// text_offset; the leader re-aims through the shifted elbow.
void emit_welding(AnnotationGraphics* out, const ResolvedAttachment& r,
                  const Annotation& annotation, const DrawingView& view,
                  bool stale) {
  const Pt attach = to_sheet(view, r.attach_point);
  const Pt d0 = default_dir(r, view);
  const Pt elbow = apply_text_offset(annotation, attach + d0 * kLeaderDefaultLengthMm);
  const Pt dir = unit(elbow - attach);
  out->primitives.push_back(thin_line(attach, elbow));
  out->primitives.push_back(arrow(attach, dir));
  out->primitives.push_back(thin_line(elbow + Pt{-kWeldingRefHalfMm, 0.0},
                                      elbow + Pt{kWeldingRefHalfMm, 0.0}));
  SheetText t = annotation_text(
      r.text, elbow + Pt{-kWeldingRefHalfMm, kTextHeightMm / 2.0 + 0.5}, stale);
  t.h_align = "left";  // symbol text above the line's left end
  out->text = t;
}

/// Tolerance frame (SYMBOLS): the ISO 1101 frame (two 7 mm
/// compartments) at the end of a short leader, content centered.
void emit_tolerance_frame(AnnotationGraphics* out, const ResolvedAttachment& r,
                          const Annotation& annotation, const DrawingView& view,
                          bool stale) {
  const Pt attach = to_sheet(view, r.attach_point);
  const Pt d0 = default_dir(r, view);
  // The frame's left edge midpoint — the leader lands there.
  const Pt left_mid = apply_text_offset(annotation, attach + d0 * kLeaderDefaultLengthMm);
  out->primitives.push_back(thin_line(attach, left_mid));
  const double w = 2.0 * kToleranceFrameWideMm;
  const double h = kToleranceFrameHighMm;
  const Pt tl = left_mid + Pt{0.0, h / 2.0};
  const Pt tr = left_mid + Pt{w, h / 2.0};
  const Pt bl = left_mid + Pt{0.0, -h / 2.0};
  const Pt br = left_mid + Pt{w, -h / 2.0};
  out->primitives.push_back(thin_line(tl, tr));
  out->primitives.push_back(thin_line(tr, br));
  out->primitives.push_back(thin_line(br, bl));
  out->primitives.push_back(thin_line(bl, tl));
  out->text = annotation_text(r.text, left_mid + Pt{w / 2.0, 0.0}, stale);
}

/// Datum (SYMBOLS): the ISO 5459 filled triangle (apex on the
/// attachment point, base up) with the letter box beside it.
void emit_datum(AnnotationGraphics* out, const ResolvedAttachment& r,
                const DrawingView& view, bool stale) {
  const Pt apex = to_sheet(view, r.attach_point);
  SheetPrimitive tri;
  tri.kind = "filled_poly";
  tri.points = {apex, apex + Pt{-kDatumTriangleHalfMm, kDatumTriangleHighMm},
                apex + Pt{kDatumTriangleHalfMm, kDatumTriangleHighMm}};
  tri.style = {"continuous", kThinLineMm};
  tri.purpose = "annotation";
  out->primitives.push_back(tri);
  // The letter box: center on the triangle's mid-height, right of its
  // base end.
  const Pt bl = apex + Pt{kDatumTriangleHalfMm + 0.5,
                          kDatumTriangleHighMm / 2.0 - kDatumBoxMm / 2.0};
  const Pt tr = bl + Pt{kDatumBoxMm, kDatumBoxMm};
  out->primitives.push_back(thin_line(bl, bl + Pt{kDatumBoxMm, 0.0}));
  out->primitives.push_back(thin_line(bl + Pt{kDatumBoxMm, 0.0}, tr));
  out->primitives.push_back(thin_line(tr, bl + Pt{0.0, kDatumBoxMm}));
  out->primitives.push_back(thin_line(bl + Pt{0.0, kDatumBoxMm}, bl));
  out->text = annotation_text(r.text, bl + Pt{kDatumBoxMm / 2.0,
                                              kDatumBoxMm / 2.0},
                              stale);
}

/// Balloon (SYMBOLS): the reference circle with the number centered,
/// a leader to the attachment point.  text_offset shifts the circle;
/// the leader re-aims through it.
void emit_balloon(AnnotationGraphics* out, const ResolvedAttachment& r,
                  const Annotation& annotation, const DrawingView& view,
                  bool stale) {
  const Pt attach = to_sheet(view, r.attach_point);
  const Pt d0 = default_dir(r, view);
  const Pt anchor = apply_text_offset(annotation, attach + d0 * kLeaderDefaultLengthMm);
  const Pt dir = unit(anchor - attach);
  const Pt rim = anchor - dir * kBalloonRadiusMm;
  out->primitives.push_back(thin_line(attach, rim));
  SheetPrimitive circle;
  circle.kind = "circle_arc";
  circle.p0 = anchor + Pt{kBalloonRadiusMm, 0.0};
  circle.p1 = circle.p0;  // full circle (start == end)
  circle.center = anchor;
  circle.radius = kBalloonRadiusMm;
  circle.start_angle = 0.0;
  circle.end_angle = 2.0 * kPi;
  circle.style = {"continuous", kThinLineMm};
  circle.purpose = "annotation";
  out->primitives.push_back(circle);
  out->text = annotation_text(r.text, anchor, stale);
}

}  // namespace

AnnotationGraphics build_annotation_graphics(const ResolvedAttachment& resolved,
                                             const Annotation& annotation,
                                             const DrawingView& view,
                                             bool stale) {
  AnnotationGraphics out;
  const std::string& kind = resolved.kind;
  if (kind == "leader_text") {
    emit_leader_text(&out, resolved, annotation, view, stale);
  } else if (kind == "center_mark") {
    emit_center_mark(&out, resolved, view);
  } else if (kind == "centerline") {
    emit_centerline(&out, resolved, view);
  } else if (kind == "edge_extension") {
    emit_edge_extension(&out, resolved, annotation, view);
  } else if (kind == "surface_finish") {
    emit_surface_finish(&out, resolved, view, stale);
  } else if (kind == "welding") {
    emit_welding(&out, resolved, annotation, view, stale);
  } else if (kind == "tolerance_frame") {
    emit_tolerance_frame(&out, resolved, annotation, view, stale);
  } else if (kind == "datum") {
    emit_datum(&out, resolved, view, stale);
  } else if (kind == "balloon") {
    emit_balloon(&out, resolved, annotation, view, stale);
  }
  return out;
}

}  // namespace polysmith::core
