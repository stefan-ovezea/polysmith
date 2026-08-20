#include "core/text_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>

#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <Font_BRepFont.hxx>
#include <Font_FontAspect.hxx>
#include <Font_FontMgr.hxx>
#include <Font_StrictLevel.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <NCollection_Buffer.hxx>
#include <NCollection_String.hxx>
#include <Standard_Type.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

#include <fstream>

#include "core/diagnostics/logger.h"

namespace polysmith::core::text {

namespace {

constexpr double kPi = 3.14159265358979323846;
// The sketch's zero-length cleanup deletes lines whose endpoints match
// within 0.01 mm — the engine must never emit segments at or below that
// scale, or the contour opens up and the profile disappears.
constexpr double kMinSegmentLength = 0.01;
constexpr int kMaxSubdivideDepth = 12;
constexpr int kMaxCircleSamples = 256;

// Minimal UTF-8 decoder — text input only, no new dependencies.
bool decode_utf8(const std::string& input,
                 std::vector<char32_t>* out,
                 std::string* error) {
  out->clear();
  size_t i = 0;
  while (i < input.size()) {
    const unsigned char c = static_cast<unsigned char>(input[i]);
    char32_t cp = 0;
    size_t extra = 0;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      extra = 1;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      extra = 2;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      extra = 3;
    } else {
      *error = "invalid UTF-8 byte in text";
      return false;
    }
    if (i + extra >= input.size()) {
      *error = "truncated UTF-8 sequence in text";
      return false;
    }
    for (size_t k = 1; k <= extra; ++k) {
      const unsigned char cont = static_cast<unsigned char>(input[i + k]);
      if ((cont & 0xC0) != 0x80) {
        *error = "invalid UTF-8 continuation byte in text";
        return false;
      }
      cp = (cp << 6) | (cont & 0x3F);
    }
    i += extra + 1;
    out->push_back(cp);
  }
  return true;
}

bool valid_h_align(const std::string& value) {
  return value == "left" || value == "center" || value == "right";
}

bool valid_v_align(const std::string& value) {
  return value == "top" || value == "middle" || value == "bottom";
}

TextPoint make_point(const gp_Pnt& pnt) {
  return TextPoint{pnt.X(), pnt.Y()};
}

struct PathSample {
  double x;
  double y;
  double tangent;  // radians, direction of travel
};

double path_length(const TextPath& path) {
  if (!path.is_arc) {
    const double dx = path.end_x - path.start_x;
    const double dy = path.end_y - path.start_y;
    return std::sqrt(dx * dx + dy * dy);
  }
  return path.radius * path.sweep_angle;
}

PathSample sample_path(const TextPath& path, double distance) {
  if (!path.is_arc) {
    const double dx = path.end_x - path.start_x;
    const double dy = path.end_y - path.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double t = distance / length;
    return PathSample{path.start_x + dx * t, path.start_y + dy * t,
                      std::atan2(dy, dx)};
  }
  const double theta = path.start_angle +
                       static_cast<double>(path.direction) *
                           (distance / path.radius);
  return PathSample{
      path.center_x + path.radius * std::cos(theta),
      path.center_y + path.radius * std::sin(theta),
      theta + static_cast<double>(path.direction) * (kPi / 2.0)};
}

// The default font: OCCT's embedded DejaVu Sans (Latin subset), dumped
// from the TKService resource once per process. Using the embedded font
// directly makes text deterministic on every machine — Font_FontMgr's
// name search would otherwise resolve "DejaVu Sans" to whatever system
// font it pleases (e.g. Arial on Windows) and glyph tessellation would
// differ per machine. A bundled TTF (POLYSMITH_TEXT_FONT_PATH) takes
// precedence when present.
std::string ensure_embedded_font_file() {
  static std::string embedded_path;
  if (!embedded_path.empty()) {
    return embedded_path;
  }
  const occ::handle<NCollection_Buffer> buffer =
      Font_FontMgr::EmbedFallbackFont();
  if (buffer.IsNull() || buffer->Size() == 0) {
    return "";
  }
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      "polysmith-embedded-dejavu.woff";
  try {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(buffer->Data()),
                 static_cast<std::streamsize>(buffer->Size()));
  } catch (const std::exception&) {
    return "";
  }
  embedded_path = path.string();
  return embedded_path;
}

}  // namespace

struct TextEngine::Impl {
  // Loaded fonts keyed by "<font-path-or-default>|<height_mm>".
  std::map<std::string, occ::handle<Font_BRepFont>> fonts;
  // Tessellated glyph contours keyed by "<font-key>|<codepoint>".
  // Relative to the glyph origin; the layout loop places them.
  std::map<std::string, std::vector<TextContour>> glyph_cache;

  occ::handle<Font_BRepFont> get_font(const TextStyle& style,
                                      double height_mm,
                                      std::string* error) {
    const std::string key =
        style.font_path + "|" + std::to_string(height_mm);
    const auto it = fonts.find(key);
    if (it != fonts.end()) {
      return it->second;
    }

    occ::handle<Font_BRepFont> font;
    std::string resolved_path = style.font_path;
    if (resolved_path.empty()) {
      resolved_path = bundled_font_path();
    }
    if (!resolved_path.empty()) {
      font = new Font_BRepFont(NCollection_String(resolved_path.c_str()),
                               height_mm);
      // The constructor swallows load failures; a null or invalid
      // FreeType face is the observable symptom (StdPrs_BRepFont only
      // ever checks FTFont().IsNull(), which misses a failed face).
      if (!font.IsNull() && font->FTFont().IsNull()) {
        font.Nullify();
      }
      if (!font.IsNull() && !font->FTFont()->IsValid()) {
        font.Nullify();
      }
      if (!font.IsNull()) {
        fonts.emplace(key, font);
        return font;
      }
      // Fall through to the system-font search below.
    }
    if (style.font_path.empty()) {
      // Default font, no bundled file: resolve through the font manager
      // (DejaVu Sans when present, otherwise any system font — the
      // embedded fallback needs a zlib-enabled FreeType build, which
      // the vendored one is not). Machine-dependent glyph shapes until
      // a bundled font ships; deterministic per machine.
      font = Font_BRepFont::FindAndCreate("DejaVu Sans",
                                          Font_FontAspect_Regular,
                                          height_mm,
                                          Font_StrictLevel_Any);
      if (!font.IsNull() && font->FTFont().IsNull()) {
        font.Nullify();
      }
      if (!font.IsNull() && !font->FTFont()->IsValid()) {
        font.Nullify();
      }
      if (!font.IsNull()) {
        fonts.emplace(key, font);
        return font;
      }
      // Last resort: the embedded WOFF (works only when FreeType was
      // built with zlib — kept for builds that have it).
      resolved_path = ensure_embedded_font_file();
      if (!resolved_path.empty()) {
        font = new Font_BRepFont(NCollection_String(resolved_path.c_str()),
                                 height_mm);
        if (!font.IsNull() && font->FTFont().IsNull()) {
          font.Nullify();
        }
        if (!font.IsNull() && !font->FTFont()->IsValid()) {
          font.Nullify();
        }
        if (!font.IsNull()) {
          fonts.emplace(key, font);
          return font;
        }
      }
      *error = "default font unavailable";
      return {};
    }
    *error = "failed to load font: " + style.font_path;
    return {};
    fonts.emplace(key, font);
    return font;
  }

  const std::vector<TextContour>& glyph_contours(
      const std::string& font_key,
      const occ::handle<Font_BRepFont>& font,
      char32_t ch,
      double tolerance) {
    const std::string key = font_key + "|" +
                            std::to_string(static_cast<uint32_t>(ch));
    const auto it = glyph_cache.find(key);
    if (it != glyph_cache.end()) {
      return it->second;
    }

    std::vector<TextContour> contours;
    const TopoDS_Shape glyph = font->RenderGlyph(ch);
    if (!glyph.IsNull()) {
      extract_glyph_contours(glyph, tolerance, &contours);
    }
    return glyph_cache.emplace(key, std::move(contours)).first->second;
  }

  // Walks the faces of a rendered glyph and samples every wire into a
  // closed polygonal contour. Faces carry the outer outline and each
  // hole as oriented wires; a glyph with several disconnected parts
  // (e.g. the dot of an "i") is several faces.
  void extract_glyph_contours(const TopoDS_Shape& glyph,
                              double tolerance,
                              std::vector<TextContour>* out) {
    for (TopExp_Explorer face_ex(glyph, TopAbs_FACE); face_ex.More();
         face_ex.Next()) {
      const TopoDS_Face& face = TopoDS::Face(face_ex.Current());

      std::vector<TopoDS_Wire> wires;
      wires.push_back(BRepTools::OuterWire(face));
      for (TopExp_Explorer wire_ex(face, TopAbs_WIRE); wire_ex.More();
           wire_ex.Next()) {
        const TopoDS_Wire& wire = TopoDS::Wire(wire_ex.Current());
        if (!wire.IsSame(wires[0])) {
          wires.push_back(wire);
        }
      }

      for (const auto& wire : wires) {
        TextContour contour;
        for (TopExp_Explorer edge_ex(wire, TopAbs_EDGE); edge_ex.More();
             edge_ex.Next()) {
          sample_edge(TopoDS::Edge(edge_ex.Current()), tolerance,
                      &contour.points);
        }
        clean_contour(&contour);
        if (!contour.points.empty()) {
          out->push_back(std::move(contour));
        }
      }
    }
  }

  void sample_edge(const TopoDS_Edge& edge,
                   double tolerance,
                   std::vector<TextPoint>* pts) {
    TopLoc_Location location;
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const occ::handle<Geom_Curve> curve =
        BRep_Tool::Curve(edge, location, first, last);
    if (curve.IsNull()) {
      return;
    }

    // Unwrap trimmed curves so the kind test sees the real geometry.
    occ::handle<Geom_Curve> basis = curve;
    while (basis->IsKind(STANDARD_TYPE(Geom_TrimmedCurve))) {
      basis = occ::handle<Geom_TrimmedCurve>::DownCast(basis)->BasisCurve();
    }

    auto emit = [&](const gp_Pnt& pnt) {
      gp_Pnt placed = pnt;
      if (!location.IsIdentity()) {
        placed.Transform(location.Transformation());
      }
      pts->push_back(make_point(placed));
    };

    if (basis->IsKind(STANDARD_TYPE(Geom_Line))) {
      emit(curve->Value(first));
      emit(curve->Value(last));
      return;
    }

    if (basis->IsKind(STANDARD_TYPE(Geom_Circle))) {
      const auto circle =
          occ::handle<Geom_Circle>::DownCast(basis);
      const double radius = circle->Radius();
      const double sweep = std::fabs(last - first);
      // Chordal angular step: chord length = 2r·sin(θ/2) ≤ tolerance.
      const double step =
          2.0 * std::acos(std::max(0.0, 1.0 - tolerance / radius));
      int samples = step > 1.0e-9
                        ? static_cast<int>(std::ceil(sweep / step))
                        : 2;
      samples = std::clamp(samples, 2, kMaxCircleSamples);
      for (int i = 1; i <= samples; ++i) {
        emit(curve->Value(first + sweep * static_cast<double>(i) /
                                      static_cast<double>(samples)));
      }
      return;
    }

    // Generic curve (glyph beziers arrive as B-Splines): recursive
    // chordal subdivision, deterministic for a given curve + tolerance.
    emit(curve->Value(first));
    subdivide(curve, first, last, tolerance, /*depth=*/0, emit);
  }

  void subdivide(const occ::handle<Geom_Curve>& curve,
                 double u0,
                 double u1,
                 double tolerance,
                 int depth,
                 const std::function<void(const gp_Pnt&)>& emit) {
    const gp_Pnt p0 = curve->Value(u0);
    const gp_Pnt p1 = curve->Value(u1);
    if (depth >= kMaxSubdivideDepth) {
      emit(p1);
      return;
    }
    const double um = 0.5 * (u0 + u1);
    const gp_Pnt pm = curve->Value(um);
    const double dx = p1.X() - p0.X();
    const double dy = p1.Y() - p0.Y();
    const double chord2 = dx * dx + dy * dy;
    const double distance =
        chord2 < 1.0e-24
            ? std::sqrt((pm.X() - p0.X()) * (pm.X() - p0.X()) +
                        (pm.Y() - p0.Y()) * (pm.Y() - p0.Y()))
            : std::fabs((pm.X() - p0.X()) * dy -
                        (pm.Y() - p0.Y()) * dx) /
                  std::sqrt(chord2);
    if (distance > tolerance) {
      subdivide(curve, u0, um, tolerance, depth + 1, emit);
      subdivide(curve, um, u1, tolerance, depth + 1, emit);
    } else {
      emit(p1);
    }
  }

  // Drops consecutive near-duplicate points (shared wire endpoints) and
  // contours too small to survive the sketch's zero-length cleanup.
  void clean_contour(TextContour* contour) {
    std::vector<TextPoint> kept;
    kept.reserve(contour->points.size());
    for (const auto& point : contour->points) {
      if (kept.empty()) {
        kept.push_back(point);
        continue;
      }
      const TextPoint& previous = kept.back();
      const double dx = point.x - previous.x;
      const double dy = point.y - previous.y;
      if (std::sqrt(dx * dx + dy * dy) < 1.0e-6) {
        continue;  // duplicate — wire edges share their endpoints
      }
      kept.push_back(point);
    }
    // Close the loop: drop a duplicate closing point (first == last).
    if (kept.size() > 1) {
      const TextPoint& first = kept.front();
      const TextPoint& back = kept.back();
      const double dx = back.x - first.x;
      const double dy = back.y - first.y;
      if (std::sqrt(dx * dx + dy * dy) < 1.0e-6) {
        kept.pop_back();
      }
    }
    if (kept.size() < 3) {
      // Degenerate contour (a glyph feature below the segment floor).
      // Dropping a hole is safer than emitting an open polyline that
      // corrupts profile detection.
      kept.clear();
    }
    contour->points = std::move(kept);
  }
};

TextEngine::TextEngine() = default;
TextEngine::~TextEngine() = default;

TextEngine& TextEngine::instance() {
  static TextEngine engine;
  if (!engine.impl_) {
    engine.impl_ = std::make_unique<Impl>();
  }
  return engine;
}

bool TextEngine::layout(const std::string& utf8_text,
                        double anchor_x,
                        double anchor_y,
                        const TextStyle& style,
                        TextLayout* out,
                        std::string* error) {
  if (out == nullptr || error == nullptr) {
    return false;
  }
  if (style.height_mm <= 0.0) {
    *error = "text height must be positive";
    return false;
  }
  if (!valid_h_align(style.h_align) || !valid_v_align(style.v_align)) {
    *error = "unsupported text alignment";
    return false;
  }

  std::vector<char32_t> chars;
  if (!decode_utf8(utf8_text, &chars, error)) {
    return false;
  }

  const occ::handle<Font_BRepFont> font =
      impl_->get_font(style, style.height_mm, error);
  if (font.IsNull()) {
    return false;
  }
  const std::string font_key =
      style.font_path + "|" + std::to_string(style.height_mm);
  const double line_spacing = font->LineSpacing();
  const double tolerance = tessellation_tolerance(style.height_mm);

  // Split into lines with per-glyph advances (two-arg AdvanceX applies
  // kerning for the current → next pair — the same metric
  // Font_TextFormatter uses, so there is no drift from OCCT's layout).
  struct GlyphPlacement {
    char32_t ch;
    double advance;
  };
  std::vector<std::vector<GlyphPlacement>> lines(1);
  for (size_t i = 0; i < chars.size(); ++i) {
    if (chars[i] == U'\n') {
      lines.emplace_back();
      continue;
    }
    const char32_t next =
        (i + 1 < chars.size() && chars[i + 1] != U'\n') ? chars[i + 1] : 0;
    lines.back().push_back(
        {chars[i],
         font->AdvanceX(chars[i], next) * (1.0 + style.char_spacing)});
  }

  // Path mode: validate the curve and compute per-line start distances
  // (h_align applies ALONG the path; v_align, angle, and the anchor are
  // ignored — the curve defines placement and rotation).
  const bool on_path = style.path.has_value();
  double curve_length = 0.0;
  std::vector<double> line_start_distance(lines.size(), 0.0);
  if (on_path) {
    curve_length = path_length(style.path.value());
    if (!(curve_length > 0.0) || !std::isfinite(curve_length)) {
      *error = "invalid text path";
      return false;
    }
    for (size_t k = 0; k < lines.size(); ++k) {
      double total = 0.0;
      for (const auto& placement : lines[k]) {
        total += placement.advance;
      }
      double start = 0.0;
      if (style.h_align == "center") {
        start = (curve_length - total) * 0.5;
      } else if (style.h_align == "right") {
        start = curve_length - total;
      }
      // Text longer than the curve overflows past the end (a "fit to
      // path" toggle is a later polish) — never start before the start.
      line_start_distance[k] = std::max(0.0, start);
    }
  }

  TextLayout layout;
  double pen_x = 0.0;
  double pen_y = 0.0;  // flat mode: baseline of the current line
  for (size_t k = 0; k < lines.size(); ++k) {
    pen_x = 0.0;
    double distance = line_start_distance[k];
    // Path mode: each further line stacks one line-height to the right
    // of travel (perpendicular to the curve), mirroring the flat
    // mode's downward stacking.
    const double perpendicular =
        on_path ? style.path_offset -
                      static_cast<double>(k) * line_spacing
                : 0.0;
    for (const auto& placement : lines[k]) {
      const std::vector<TextContour>& glyph =
          impl_->glyph_contours(font_key, font, placement.ch, tolerance);
      if (glyph.empty() && placement.ch != U' ') {
        log_debug("text_engine",
                  "no outline for codepoint " +
                      std::to_string(static_cast<uint32_t>(placement.ch)));
      }
      for (const auto& contour : glyph) {
        TextContour placed;
        placed.points.reserve(contour.points.size());
        for (const auto& point : contour.points) {
          if (on_path) {
            const PathSample sample =
                sample_path(style.path.value(), distance);
            const double sin_t = std::sin(sample.tangent);
            const double cos_t = std::cos(sample.tangent);
            // Baseline origin = curve point + perpendicular offset
            // (positive = left of travel); glyph points rotate by the
            // tangent angle around that origin.
            const double origin_x = sample.x - sin_t * perpendicular;
            const double origin_y = sample.y + cos_t * perpendicular;
            placed.points.push_back(
                TextPoint{origin_x + point.x * cos_t - point.y * sin_t,
                          origin_y + point.x * sin_t + point.y * cos_t});
          } else {
            placed.points.push_back(
                TextPoint{point.x + pen_x, point.y + pen_y});
          }
        }
        layout.contours.push_back(std::move(placed));
      }
      if (on_path) {
        distance += placement.advance;
      } else {
        pen_x += placement.advance;
      }
    }
    if (!on_path) {
      pen_y -= line_spacing;
    }
  }

  // Bounds of the placed text, then the alignment shift. The anchor
  // maps onto the aligned frame: h_align picks which frame edge lands
  // at the anchor x, v_align at the anchor y.
  double min_x = 0.0, max_x = 0.0, min_y = 0.0, max_y = 0.0;
  bool any = false;
  for (const auto& contour : layout.contours) {
    for (const auto& point : contour.points) {
      if (!any) {
        min_x = max_x = point.x;
        min_y = max_y = point.y;
        any = true;
      } else {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
      }
    }
  }
  // Flat mode only: alignment shift + angle rotation + anchor
  // translation. Path mode is already placed/rotated by the curve —
  // the anchor is meaningless there.
  if (!on_path) {
    double shift_x = 0.0;
    double shift_y = 0.0;
    if (any) {
      if (style.h_align == "left") {
        shift_x = -min_x;
      } else if (style.h_align == "right") {
        shift_x = -max_x;
      } else {
        shift_x = -0.5 * (min_x + max_x);
      }
      if (style.v_align == "top") {
        shift_y = -max_y;
      } else if (style.v_align == "bottom") {
        shift_y = -min_y;
      } else {
        shift_y = -0.5 * (min_y + max_y);
      }
    }

    const double angle_rad = style.angle_deg * (kPi / 180.0);
    const double cos_a = std::cos(angle_rad);
    const double sin_a = std::sin(angle_rad);
    for (auto& contour : layout.contours) {
      for (auto& point : contour.points) {
        const double x = point.x + shift_x;
        const double y = point.y + shift_y;
        point.x = x * cos_a - y * sin_a + anchor_x;
        point.y = x * sin_a + y * cos_a + anchor_y;
      }
    }
  }

  layout.width = any ? max_x - min_x : 0.0;
  layout.height = any ? max_y - min_y : 0.0;
  *out = std::move(layout);
  return true;
}

double TextEngine::tessellation_tolerance(double height_mm) {
  // Proportional to the height so height edits scale the glyphs without
  // changing the tessellation structure (stable generated ids). Floor of
  // 0.01 mm = the sketch's zero-length threshold; cap of 0.2 mm keeps
  // very large text from getting visibly faceted.
  return std::clamp(height_mm / 200.0, 0.01, 0.2);
}

std::string TextEngine::bundled_font_path() {
  const char* env = std::getenv("POLYSMITH_TEXT_FONT_PATH");
  if (env != nullptr && *env != '\0' && std::filesystem::exists(env)) {
    return std::string(env);
  }
  // Repo-relative fallbacks: first from the repo root (pnpm test:core),
  // then from native/cad-core (direct test runs).
  const std::vector<std::string> candidates = {
      "apps/desktop-ui/src-tauri/resources/fonts/LiberationSans-Regular.ttf",
      "../../apps/desktop-ui/src-tauri/resources/fonts/"
      "LiberationSans-Regular.ttf",
  };
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  return "";
}

}  // namespace polysmith::core::text
