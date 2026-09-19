#include "core/drawing/drawing_sheet.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

#include "core/document/document_state.h"
#include "core/drawing/drawing_dimension_geometry.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_text.h"
#include "core/drawing/drawing_title_block.h"

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kThickLineMm = 0.5;   // ISO line group 0.5 (A3/A4)
constexpr double kThinLineMm = 0.25;
constexpr double kFrameLineMm = 0.7;   // ISO 5457 frame
constexpr double kLeftMarginMm = 20.0; // ISO 5457: left margin incl. frame
constexpr double kMarginMm = 10.0;     // other margins incl. frame

double quant(double v) { return std::round(v * 1e6) / 1e6; }

// ── Coincidence priority (ISO 128-2) ──────────────────────────────
// When two primitives share geometry, the highest-priority line wins:
// visible > hidden > cutting plane > hatch.  Lower rank wins.

int priority_rank(const SheetPrimitive& p) {
  if (p.purpose != "cutting_plane" && p.purpose != "hatch" &&
      p.line_class == "visible") {
    return 0;
  }
  if (p.line_class == "hidden") {
    return 1;
  }
  if (p.purpose == "cutting_plane") {
    return 2;
  }
  return 3;
}

/// The infinite support of a line primitive (unit direction +
/// signed distance from the origin) — the coincidence-priority check
/// uses supports so a chain trace lying ON a visible line is dropped
/// even when its parameter range differs from the visible line's.
struct LineSupport {
  std::array<double, 2> dir = {1.0, 0.0};
  double offset = 0.0;
};

std::optional<LineSupport> line_support(const SheetPrimitive& p) {
  if (p.kind != "line") {
    return std::nullopt;
  }
  const double dx = p.p1[0] - p.p0[0];
  const double dy = p.p1[1] - p.p0[1];
  const double len = std::hypot(dx, dy);
  if (len < 1e-9) {
    return std::nullopt;
  }
  LineSupport support;
  support.dir = {dx / len, dy / len};
  // cross(dir, p0) = the signed distance from the origin.
  support.offset = support.dir[0] * p.p0[1] - support.dir[1] * p.p0[0];
  return support;
}

/// True when `lower` lies on the same support as `higher` with
/// overlapping parameter ranges (or the same circle).
bool supports_overlap(const SheetPrimitive& lower,
                      const SheetPrimitive& higher) {
  const auto lower_line = line_support(lower);
  const auto higher_line = line_support(higher);
  if (lower_line.has_value() && higher_line.has_value()) {
    const double cross = lower_line->dir[0] * higher_line->dir[1] -
                         lower_line->dir[1] * higher_line->dir[0];
    if (std::abs(cross) > 1e-6) {
      return false;  // not parallel
    }
    if (std::abs(lower_line->offset - higher_line->offset) > 1e-6) {
      return false;  // parallel but not the same line
    }
    const auto range = [](const SheetPrimitive& p, const LineSupport& s) {
      double a = p.p0[0] * s.dir[0] + p.p0[1] * s.dir[1];
      double b = p.p1[0] * s.dir[0] + p.p1[1] * s.dir[1];
      if (a > b) {
        std::swap(a, b);
      }
      return std::pair<double, double>{a, b};
    };
    const auto lower_range = range(lower, lower_line.value());
    const auto higher_range = range(higher, higher_line.value());
    return lower_range.first < higher_range.second - 1e-6 &&
           higher_range.first < lower_range.second - 1e-6;
  }
  // Same circle: identical center + radius (traces are lines in
  // practice; the circle case is defensive).
  if (lower.kind == "circle_arc" && higher.kind == "circle_arc" &&
      lower.center.has_value() && higher.center.has_value() &&
      lower.radius.has_value() && higher.radius.has_value()) {
    return std::hypot(lower.center.value()[0] - higher.center.value()[0],
                      lower.center.value()[1] -
                          higher.center.value()[1]) < 1e-6 &&
           std::abs(lower.radius.value() - higher.radius.value()) < 1e-6;
  }
  return false;
}

/// Geometry key: kind + quantized geometry, direction-independent for
/// lines (sorted endpoints).
std::string geometry_key(const SheetPrimitive& p) {
  std::ostringstream os;
  os << std::fixed;
  os.precision(6);
  os << p.kind << "|";
  if (p.kind == "line") {
    double x0 = quant(p.p0[0]);
    double y0 = quant(p.p0[1]);
    double x1 = quant(p.p1[0]);
    double y1 = quant(p.p1[1]);
    if (x0 > x1 || (x0 == x1 && y0 > y1)) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    os << x0 << "," << y0 << "|" << x1 << "," << y1;
  } else {
    os << quant(p.p0[0]) << "," << quant(p.p0[1]) << "|"
       << quant(p.p1[0]) << "," << quant(p.p1[1]);
    if (p.center.has_value()) {
      os << "|" << quant(p.center.value()[0]) << ","
         << quant(p.center.value()[1]);
    }
    if (p.radius.has_value()) {
      os << "|r" << quant(p.radius.value());
    }
  }
  return os.str();
}

// ── Dash pattern application (ISO 128-2, Annex A corner rule) ─────
// Dashed: 12d dash / 3d gap.  Chain: 24d dash / 3d gap / dot d /
// 3d gap.  Every curve piece is dashed independently — corners
// restart the pattern and always start with a dash — and the final
// dash extends to the end point so no piece ever ends with a gap.

struct DashSpan {
  double from;
  double to;
  /// true = the chain line's dot (a short stroke of one line width).
  bool is_dot = false;
};

/// Pattern spans over [0, length] (dot == 0 for dashed lines; chain
/// lines add a dot after each gap).
std::vector<DashSpan> dash_spans(double length, double dash, double gap,
                                 double dot) {
  std::vector<DashSpan> spans;
  if (length < 1e-9) {
    return spans;
  }
  const bool has_dot = dot > 0;
  const double cycle = dash + gap + (has_dot ? dot + gap : 0.0);
  double cursor = 0.0;
  while (cursor < length - 1e-9) {
    // The final dash extends to the end point (the corner rule) —
    // clamping to `length` here is exactly that extension.
    spans.push_back({cursor, std::min(cursor + dash, length), false});
    if (has_dot) {
      const double dot_from = cursor + dash + gap;
      if (dot_from < length - 1e-9) {
        spans.push_back({dot_from, std::min(dot_from + dot, length), true});
      }
    }
    cursor += cycle;
  }
  return spans;
}

/// Dashes one line primitive into continuous sub-segments.
void emit_dashed_line(std::vector<SheetPrimitive>& out,
                      const SheetPrimitive& primitive) {
  const double dx = primitive.p1[0] - primitive.p0[0];
  const double dy = primitive.p1[1] - primitive.p0[1];
  const double length = std::hypot(dx, dy);
  if (length < 1e-9) {
    return;
  }
  const double w = primitive.style.width_mm;
  const double dash = primitive.style.line_type == "dashed" ? 12.0 * w
                                                            : 24.0 * w;
  const double gap = 3.0 * w;
  const double dot = primitive.style.line_type == "dashed" ? 0.0 : w;
  for (const auto& span : dash_spans(length, dash, gap, dot)) {
    SheetPrimitive piece = primitive;
    piece.style.line_type = "continuous";
    piece.p0 = {primitive.p0[0] + dx * span.from / length,
                primitive.p0[1] + dy * span.from / length};
    piece.p1 = {primitive.p0[0] + dx * span.to / length,
                primitive.p0[1] + dy * span.to / length};
    out.push_back(std::move(piece));
  }
}

/// Dashes one circle/ellipse arc into continuous sub-arcs (angular
/// parameters map linearly to arc length).
void emit_dashed_arc(std::vector<SheetPrimitive>& out,
                     const SheetPrimitive& primitive) {
  double radius = 0.0;
  if (primitive.kind == "circle_arc" && primitive.radius.has_value()) {
    radius = primitive.radius.value();
  } else if (primitive.kind == "ellipse_arc" &&
             primitive.major_radius.has_value()) {
    radius = primitive.major_radius.value();
  }
  if (radius < 1e-9) {
    return;
  }
  const double sweep = std::abs(primitive.end_angle - primitive.start_angle);
  const double length = sweep * radius;
  if (length < 1e-9) {
    return;
  }
  const double w = primitive.style.width_mm;
  const double dash = primitive.style.line_type == "dashed" ? 12.0 * w
                                                            : 24.0 * w;
  const double gap = 3.0 * w;
  const double dot = primitive.style.line_type == "dashed" ? 0.0 : w;
  const double direction = primitive.end_angle >= primitive.start_angle
                               ? 1.0
                               : -1.0;
  const auto point_at = [&](double angle) {
    if (primitive.kind == "circle_arc" && primitive.center.has_value() &&
        primitive.radius.has_value()) {
      const auto& c = primitive.center.value();
      const double r = primitive.radius.value();
      return std::array<double, 2>{c[0] + r * std::cos(angle),
                                   c[1] + r * std::sin(angle)};
    }
    // Ellipse: center + major·cos(a) + minor_dir·minor·sin(a).
    const auto& c = primitive.center.value();
    const auto& md = primitive.major_dir.value();
    const double major = primitive.major_radius.value();
    const double minor = primitive.minor_radius.value();
    return std::array<double, 2>{
        c[0] + md[0] * major * std::cos(angle) -
            md[1] * minor * std::sin(angle),
        c[1] + md[1] * major * std::cos(angle) +
            md[0] * minor * std::sin(angle)};
  };
  for (const auto& span : dash_spans(length, dash, gap, dot)) {
    SheetPrimitive piece = primitive;
    piece.style.line_type = "continuous";
    piece.start_angle =
        primitive.start_angle + direction * span.from / radius;
    piece.end_angle = primitive.start_angle + direction * span.to / radius;
    piece.p0 = point_at(piece.start_angle);
    piece.p1 = point_at(piece.end_angle);
    out.push_back(std::move(piece));
  }
}

void emit_primitive(std::vector<SheetPrimitive>& out, SheetPrimitive p) {
  if (p.style.line_type == "continuous") {
    out.push_back(std::move(p));
    return;
  }
  if (p.kind == "line") {
    emit_dashed_line(out, p);
  } else {
    emit_dashed_arc(out, p);
  }
}

// ── ISO 5457 furniture ────────────────────────────────────────────

/// The frame: a rectangle at the trimmed margins (left 20 mm incl.
/// frame, the others 10 mm incl. frame), 0.7 mm wide.
void emit_frame(std::vector<SheetPrimitive>& out, double w, double h) {
  const double x0 = kLeftMarginMm;
  const double x1 = w - kMarginMm;
  const double y0 = kMarginMm;
  const double y1 = h - kMarginMm;
  const std::array<std::array<double, 2>, 5> corners = {{
      {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0},
  }};
  for (size_t i = 0; i + 1 < corners.size(); ++i) {
    SheetPrimitive p;
    p.purpose = "frame";
    p.style = {"continuous", kFrameLineMm};
    p.p0 = corners[i];
    p.p1 = corners[i + 1];
    out.push_back(std::move(p));
  }
}

/// Four centring marks at the side midpoints: short ticks extending
/// 5 mm OUTWARD from the frame into the margin (ISO 5457), ≥0.5 mm
/// stroke.
void emit_centring_marks(std::vector<SheetPrimitive>& out, double w,
                         double h) {
  const double x0 = kLeftMarginMm;
  const double x1 = w - kMarginMm;
  const double y0 = kMarginMm;
  const double y1 = h - kMarginMm;
  const double mid_x = (x0 + x1) / 2.0;
  const double mid_y = (y0 + y1) / 2.0;
  const auto mark = [&](std::array<double, 2> a, std::array<double, 2> b) {
    SheetPrimitive p;
    p.purpose = "centring_mark";
    p.style = {"continuous", 0.5};
    p.p0 = a;
    p.p1 = b;
    out.push_back(std::move(p));
  };
  mark({mid_x, y0}, {mid_x, y0 - 5.0});          // top
  mark({mid_x, y1}, {mid_x, y1 + 5.0});          // bottom
  mark({x0, mid_y}, {x0 - 5.0, mid_y});          // left
  mark({x1, mid_y}, {x1 + 5.0, mid_y});          // right
}

/// Grid-reference ticks: short strokes on the 5 mm strip INSIDE the
/// frame at each division boundary.  Division counts (ISO 5457, long
/// side first): A0 24/16, A1 16/12, A2 12/8, A3 8/6, A4 6/4.  The
/// zone LETTERS/NUMBERS are text — they land with the P7 text engine.
void emit_grid_reference_ticks(std::vector<SheetPrimitive>& out, double w,
                               double h, const std::string& paper_size) {
  int long_count = 6;   // A4 default
  int short_count = 4;
  if (paper_size == "A0") { long_count = 24; short_count = 16; }
  if (paper_size == "A1") { long_count = 16; short_count = 12; }
  if (paper_size == "A2") { long_count = 12; short_count = 8; }
  if (paper_size == "A3") { long_count = 8; short_count = 6; }

  const double x0 = kLeftMarginMm;
  const double x1 = w - kMarginMm;
  const double y0 = kMarginMm;
  const double y1 = h - kMarginMm;
  const double inner_w = x1 - x0;
  const double inner_h = y1 - y0;
  // The longer SIDE of the trimmed sheet carries the long count.
  const int div_w = w >= h ? long_count : short_count;
  const int div_h = w >= h ? short_count : long_count;

  const auto tick = [&](std::array<double, 2> a, std::array<double, 2> b) {
    SheetPrimitive p;
    p.purpose = "grid_ref";
    p.style = {"continuous", kThinLineMm};
    p.p0 = a;
    p.p1 = b;
    out.push_back(std::move(p));
  };
  for (int i = 1; i < div_w; ++i) {
    const double x = x0 + inner_w * i / div_w;
    tick({x, y0}, {x, y0 + 5.0});   // top strip
    tick({x, y1}, {x, y1 - 5.0});   // bottom strip
  }
  for (int j = 1; j < div_h; ++j) {
    const double y = y0 + inner_h * j / div_h;
    tick({x0, y}, {x0 + 5.0, y});   // left strip
    tick({x1, y}, {x1 - 5.0, y});   // right strip
  }
}

// ── View flattening ───────────────────────────────────────────────

SheetLineStyle style_for(const ProjectedEdgeRecord& rec) {
  if (rec.line_class == "hidden") {
    return {"dashed", kThinLineMm};               // ISO type E/F
  }
  if (rec.curve_class == "cutting_plane") {
    return {"chain", kThinLineMm};                // ISO type H
  }
  if (rec.curve_class == "seam") {
    return {"continuous", kThinLineMm};           // ISO 128-24 seams
  }
  return {"continuous", kThickLineMm};            // ISO type A
}

void flatten_view(std::vector<SheetPrimitive>& view_primitives,
                  std::vector<SheetPrimitive>& hatch_primitives,
                  const DrawingView& view,
                  const ProjectionResult& projection) {
  const double s = view.scale;
  const double ox = view.sheet_position[0];
  const double oy = view.sheet_position[1];
  for (const auto& rec : projection.edges) {
    // ISO 128-24: smooth transitions draw no line.
    if (rec.curve_class == "smooth") {
      continue;
    }
    SheetPrimitive p;
    p.kind = rec.curve_kind;
    p.line_class = rec.line_class;
    p.purpose = rec.curve_class == "cutting_plane" ? "cutting_plane"
                                                   : "view_geometry";
    p.p0 = {rec.p_start[0] * s + ox, rec.p_start[1] * s + oy};
    p.p1 = {rec.p_end[0] * s + ox, rec.p_end[1] * s + oy};
    if (rec.circle_center.has_value()) {
      p.center = {{rec.circle_center.value()[0] * s + ox,
                   rec.circle_center.value()[1] * s + oy}};
      p.radius = rec.circle_radius.value() * s;
    }
    if (rec.ellipse_center.has_value()) {
      p.center = {{rec.ellipse_center.value()[0] * s + ox,
                   rec.ellipse_center.value()[1] * s + oy}};
      p.major_dir = rec.ellipse_major_dir;
      p.major_radius = rec.ellipse_major_radius.value() * s;
      p.minor_radius = rec.ellipse_minor_radius.value() * s;
    }
    p.start_angle = rec.start_angle;
    p.end_angle = rec.end_angle;
    p.style = style_for(rec);
    p.section_label = rec.section_label;
    p.trace_sight_dir = rec.trace_sight_dir;
    // UNDASHED here — the coincidence-priority pass needs the full
    // geometry (a dashed hidden edge and a solid visible edge
    // coincide only before dashing).  Dashing happens after the
    // priority pass in flatten_sheet.
    view_primitives.push_back(std::move(p));
  }
  // Section hatching (ISO 128-3 §7): thin continuous scanlines —
  // emitted directly (hatch lines never coincide with edges).
  if (view.section.has_value()) {
    const auto& section = view.section.value();
    for (const auto& region : projection.hatch_regions) {
      const auto segments = compute_hatch_segments(
          region, section.hatch_angle_deg, section.hatch_spacing_mm);
      for (const auto& segment : segments) {
        SheetPrimitive p;
        p.purpose = "hatch";
        p.style = {"continuous", kThinLineMm};
        p.p0 = {segment[0][0] * s + ox, segment[0][1] * s + oy};
        p.p1 = {segment[1][0] * s + ox, segment[1][1] * s + oy};
        hatch_primitives.push_back(std::move(p));
      }
    }
  }
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────

std::array<double, 2> paper_size_mm(const std::string& paper_size) {
  if (paper_size == "A0") return {841.0, 1189.0};
  if (paper_size == "A1") return {594.0, 841.0};
  if (paper_size == "A2") return {420.0, 594.0};
  if (paper_size == "A3") return {297.0, 420.0};
  return {210.0, 297.0};  // A4
}

std::optional<SheetPrimitiveStream> flatten_sheet(
    const DocumentState& document, const std::string& drawing_id,
    const std::string& sheet_id) {
  const Drawing* drawing = nullptr;
  for (const auto& d : document.drawing.drawings) {
    if (d.drawing_id == drawing_id) {
      drawing = &d;
      break;
    }
  }
  if (drawing == nullptr) {
    return std::nullopt;
  }
  const DrawingSheet* sheet = nullptr;
  for (const auto& s : drawing->sheets) {
    if (s.sheet_id == sheet_id) {
      sheet = &s;
      break;
    }
  }
  if (sheet == nullptr) {
    return std::nullopt;
  }

  SheetPrimitiveStream stream;
  stream.sheet_id = sheet_id;
  stream.drawing_id = drawing_id;
  const auto size = paper_size_mm(sheet->paper_size);
  stream.width_mm = sheet->orientation == "landscape" ? size[1] : size[0];
  stream.height_mm = sheet->orientation == "landscape" ? size[0] : size[1];
  const double w = stream.width_mm;
  const double h = stream.height_mm;

  // Furniture first (deterministic stream order — the golden pins it).
  emit_frame(stream.primitives, w, h);
  emit_centring_marks(stream.primitives, w, h);
  emit_grid_reference_ticks(stream.primitives, w, h, sheet->paper_size);
  // The ISO 7200 title block (+ projection symbol inside it) needs
  // the sheet index for the "Sheet x/y" auto-fill.
  size_t sheet_index = 0;
  for (size_t i = 0; i < drawing->sheets.size(); ++i) {
    if (drawing->sheets[i].sheet_id == sheet_id) {
      sheet_index = i;
      break;
    }
  }
  flatten_title_block(document, *drawing, *sheet, sheet_index,
                      drawing->sheets.size(), stream);

  // Views in sheet order: collect UNDASHED primitives first — the
  // coincidence-priority pass needs the full geometry.
  std::vector<SheetPrimitive> view_primitives;
  std::vector<SheetPrimitive> hatch_primitives;
  std::vector<SheetPrimitive> dimension_primitives;
  int view_index = 0;
  for (const auto& view_id : sheet->view_ids) {
    ++view_index;
    const DrawingView* view = nullptr;
    for (const auto& v : drawing->views) {
      if (v.view_id == view_id) {
        view = &v;
        break;
      }
    }
    if (view == nullptr) {
      continue;  // dangling reference — the mutators prevent it
    }
    SheetViewBounds bounds;
    bounds.view_id = view->view_id;
    bounds.label = !view->standard_view.empty()
                       ? view->standard_view
                       : "View " + std::to_string(view_index);
    bounds.scale = view->scale;
    bounds.origin = view->sheet_position;

    const ProjectionResult* projection =
        drawing_runtime::cached_projection(document, view->view_id);
    if (projection != nullptr) {
      bounds.stale = projection->stale;
      bounds.warning = view->warning;
      // Content bounds from the view geometry (hatch lies inside the
      // section outline — excluded, like the P3 emission).
      const size_t before = view_primitives.size();
      flatten_view(view_primitives, hatch_primitives, *view, *projection);
      bool have_bounds = false;
      for (size_t i = before; i < view_primitives.size(); ++i) {
        const auto& p = view_primitives[i];
        if (p.purpose != "view_geometry") {
          continue;
        }
        if (!have_bounds) {
          bounds.min = {p.p0[0], p.p0[1]};
          bounds.max = {p.p0[0], p.p0[1]};
          have_bounds = true;
        }
        bounds.min[0] = std::min({bounds.min[0], p.p0[0], p.p1[0]});
        bounds.min[1] = std::min({bounds.min[1], p.p0[1], p.p1[1]});
        bounds.max[0] = std::max({bounds.max[0], p.p0[0], p.p1[0]});
        bounds.max[1] = std::max({bounds.max[1], p.p0[1], p.p1[1]});
      }
      // ── Dimensions (P6) ──────────────────────────────────────
      // The refresh pass resolved this view's annotations against
      // the fresh projection; the flatten consumes the resolved
      // attachment geometry (memory-only, like the projections).
      // Annotation primitives never join the view-geometry dedup —
      // they draw on top, after hatching.
      for (const auto& annotation : drawing->annotations) {
        if (annotation.view_id != view->view_id) {
          continue;
        }
        const ResolvedDimension* resolved = drawing_runtime::cached_dimension(
            document, annotation.annotation_id);
        if (resolved == nullptr || resolved->broken) {
          continue;  // no geometry (the panel shows the warning)
        }
        DimensionGraphics graphics =
            build_dimension_graphics(*resolved, annotation, *view,
                                     resolved->stale);
        dimension_primitives.insert(dimension_primitives.end(),
                                    graphics.primitives.begin(),
                                    graphics.primitives.end());
        if (graphics.text.has_value()) {
          stream.texts.push_back(graphics.text.value());
        }
      }
    }
    stream.views.push_back(std::move(bounds));
  }

  // ── Coincidence priority (ISO 128-2) ────────────────────────────
  // (1) Primitives sharing exact geometry collapse to the highest-
  // priority line.  (2) A cutting-plane trace lying ON a visible or
  // hidden line (same support, overlapping range) is dropped
  // entirely — the trace never draws over a real edge.
  std::vector<SheetPrimitive> deduped;
  std::unordered_map<std::string, size_t> by_key;
  for (auto& p : view_primitives) {
    const std::string key = geometry_key(p);
    const auto found = by_key.find(key);
    if (found == by_key.end()) {
      by_key.emplace(key, deduped.size());
      deduped.push_back(std::move(p));
    } else if (priority_rank(p) < priority_rank(deduped[found->second])) {
      deduped[found->second] = std::move(p);
    }
  }
  std::vector<SheetPrimitive> survivors;
  for (auto& p : deduped) {
    if (p.purpose == "cutting_plane") {
      const bool covered = std::any_of(
          deduped.begin(), deduped.end(), [&](const SheetPrimitive& other) {
            return other.purpose == "view_geometry" &&
                   supports_overlap(p, other);
          });
      if (covered) {
        continue;
      }
    }
    survivors.push_back(std::move(p));
  }

  // ── Section labels (P7: A–A labels + arrows on cutting planes) ─
  // Collected BEFORE the dashing pass moves the survivors out.  Only
  // traces that survived the coincidence pass get labels — a trace
  // lying on a real edge is dropped and so is its label.
  std::vector<SheetPrimitive> section_label_primitives;
  std::vector<SheetText> section_label_texts;
  for (const auto& p : survivors) {
    if (p.purpose != "cutting_plane" || p.section_label.empty()) {
      continue;
    }
    const double mx = 0.5 * (p.p0[0] + p.p1[0]);
    const double my = 0.5 * (p.p0[1] + p.p1[1]);
    for (const auto& end : {p.p0, p.p1}) {
      // The section's sight direction from the trace pass; outward
      // from the trace midpoint is the fallback.
      std::array<double, 2> d = {end[0] - mx, end[1] - my};
      if (p.trace_sight_dir.has_value()) {
        d = p.trace_sight_dir.value();
      }
      const double len = std::hypot(d[0], d[1]);
      if (len < 1e-9) {
        continue;
      }
      d[0] /= len;
      d[1] /= len;
      // Filled arrow: tip at the trace end, 3 mm long, 1 mm
      // half-width (the dimension arrows' proportions).
      const double tail_x = end[0] - 3.0 * d[0];
      const double tail_y = end[1] - 3.0 * d[1];
      SheetPrimitive arrow;
      arrow.kind = "filled_poly";
      arrow.purpose = "section_label";
      arrow.style = {"continuous", kThinLineMm};
      arrow.points = {{end[0], end[1]},
                      {tail_x - d[1], tail_y + d[0]},
                      {tail_x + d[1], tail_y - d[0]}};
      section_label_primitives.push_back(std::move(arrow));
      // The label letter beyond the arrow tail.
      SheetText label;
      label.text = p.section_label;
      label.position = {end[0] - 5.5 * d[0], end[1] - 5.5 * d[1]};
      label.height_mm = 5.0;
      label.h_align = "center";
      label.purpose = "section_label";
      section_label_texts.push_back(std::move(label));
    }
  }

  // Dashing happens AFTER the priority pass, then the primitives
  // land in the stream (furniture is already in).
  for (auto& p : survivors) {
    emit_primitive(stream.primitives, std::move(p));
  }
  for (auto& p : hatch_primitives) {
    stream.primitives.push_back(std::move(p));
  }
  // Dimensions last — annotations draw on top of the sheet content.
  for (auto& p : dimension_primitives) {
    stream.primitives.push_back(std::move(p));
  }
  // Section labels on top — the filled arrow terminates the chain
  // line's end dash (ISO 128-3).
  for (auto& p : section_label_primitives) {
    stream.primitives.push_back(std::move(p));
  }
  stream.texts.insert(stream.texts.end(), section_label_texts.begin(),
                      section_label_texts.end());

  // ── Vector glyphs (P7) ─────────────────────────────────────────
  // SheetText records stay as DATA (the DXF backend emits a real
  // DRW_Text from them); the viewport and the PDF/SVG backends draw
  // these single-stroke glyph primitives instead.
  for (const auto& text : stream.texts) {
    auto glyphs = drawing_text_glyphs(text);
    stream.primitives.insert(stream.primitives.end(), glyphs.begin(),
                             glyphs.end());
  }
  return stream;
}

}  // namespace polysmith::core
