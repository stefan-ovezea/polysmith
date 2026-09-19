#include "core/drawing/drawing_text.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "core/diagnostics/logger.h"
#include "core/text_engine.h"

namespace polysmith::core {

namespace {

constexpr double kThinLineMm = 0.25;  // ISO 128-2 thin line

// ── Layout cache ───────────────────────────────────────────────────
// TextEngine::layout runs FreeType + glyph tessellation per call; a
// flattened sheet re-emits its texts on every viewport sync, so cache
// the placed contours and only translate them per record.  Layouts
// are keyed WITHOUT the anchor — the placement math is affine, so a
// (0,0) layout translated by the anchor equals a direct layout.
struct GlyphCacheKey {
  std::string font_path;
  std::string text;
  double height_mm = 0.0;
  double angle_deg = 0.0;
  std::string h_align;
};

bool operator==(const GlyphCacheKey& a, const GlyphCacheKey& b) {
  return a.font_path == b.font_path && a.text == b.text &&
         a.height_mm == b.height_mm && a.angle_deg == b.angle_deg &&
         a.h_align == b.h_align;
}

struct GlyphCacheKeyHash {
  size_t operator()(const GlyphCacheKey& key) const {
    size_t hash = std::hash<std::string>{}(key.font_path);
    hash ^= std::hash<std::string>{}(key.text) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    hash ^= std::hash<double>{}(key.height_mm) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    hash ^= std::hash<double>{}(key.angle_deg) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    hash ^= std::hash<std::string>{}(key.h_align) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    return hash;
  }
};

// One closed contour polyline placed at (0,0).
using ContourList = std::vector<std::vector<text::TextPoint>>;

ContourList& cached_contours(const GlyphCacheKey& key) {
  static std::unordered_map<GlyphCacheKey, ContourList, GlyphCacheKeyHash>
      cache;
  auto found = cache.find(key);
  if (found != cache.end()) {
    return found->second;
  }
  // Bound the cache: distinct texts per drawing are few, but a user
  // flipping fonts/heights repeatedly could grow it.  Clearing loses
  // only layout time, never data.
  if (cache.size() >= 512) {
    cache.clear();
  }
  ContourList contours;
  text::TextStyle style;
  style.font_path = key.font_path;
  style.height_mm = key.height_mm;
  style.angle_deg = key.angle_deg;
  style.h_align = key.h_align;
  style.v_align = "middle";
  text::TextLayout layout;
  std::string error;
  if (!text::TextEngine::instance().layout(key.text, 0.0, 0.0, style,
                                           &layout, &error)) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      log_warn("drawing_text", "text layout failed: " + error);
    }
  } else {
    for (const auto& contour : layout.contours) {
      contours.push_back(contour.points);
    }
  }
  return cache.emplace(key, std::move(contours)).first->second;
}

}  // namespace

std::vector<SheetPrimitive> drawing_text_glyphs(const SheetText& text) {
  std::vector<SheetPrimitive> out;
  if (text.text.empty()) {
    return out;
  }
  // ISO 3098 font first, the sketch text font as the fallback — a
  // drawing must never lose its text because one font is absent.
  std::string font_path = text::TextEngine::bundled_iso3098_font_path();
  if (font_path.empty()) {
    font_path = text::TextEngine::bundled_font_path();
  }

  GlyphCacheKey key{font_path, text.text, text.height_mm, text.angle_deg,
                    text.h_align};
  const ContourList& contours = cached_contours(key);

  const auto emit_segment = [&](const text::TextPoint& a,
                                const text::TextPoint& b) {
    SheetPrimitive p;
    p.kind = "line";
    p.purpose = "text_glyph";
    p.style = {"continuous", kThinLineMm};
    p.p0 = {a.x + text.position[0], a.y + text.position[1]};
    p.p1 = {b.x + text.position[0], b.y + text.position[1]};
    out.push_back(std::move(p));
  };
  const auto emit_contour = [&](const std::vector<text::TextPoint>& contour) {
    for (size_t i = 1; i < contour.size(); ++i) {
      emit_segment(contour[i - 1], contour[i]);
    }
    // Contours are closed loops (the last point connects back to the
    // first) — glyphs whose outline was a single open stroke come
    // back from the engine as a degenerate two-point loop, so the
    // closing segment collapses to the opening one and only the
    // stroke remains.  Skip the closing segment for size-1 loops.
    if (contour.size() > 2) {
      emit_segment(contour.back(), contour.front());
    }
  };
  // OCCT's StdPrs_BRepFont orders a glyph's faces by iterating an
  // NCollection_DataMap (hash order), so multi-part glyphs like "i"
  // come back with their contours in process-dependent order.  Sort
  // by a geometric key so the stream — and the golden tests — are
  // reproducible across runs.  Identical keys imply identical
  // geometry, so a swap between them changes nothing.
  std::vector<std::array<double, 4>> bounds;
  bounds.reserve(contours.size());
  for (const auto& contour : contours) {
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    bool any = false;
    for (const auto& point : contour) {
      if (!any) {
        min_x = max_x = point.x;
        min_y = max_y = point.y;
        any = true;
      } else {
        min_x = std::min(min_x, point.x);
        min_y = std::min(min_y, point.y);
        max_x = std::max(max_x, point.x);
        max_y = std::max(max_y, point.y);
      }
    }
    bounds.push_back({min_x, min_y, max_x, max_y});
  }
  std::vector<size_t> order(contours.size());
  for (size_t i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return bounds[a] < bounds[b]; });
  for (size_t index : order) {
    emit_contour(contours[index]);
  }
  return out;
}

}  // namespace polysmith::core
