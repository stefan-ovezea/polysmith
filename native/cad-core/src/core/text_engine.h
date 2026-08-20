#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace polysmith::core::text {

struct TextPoint {
  double x = 0.0;
  double y = 0.0;
};

// One closed glyph contour (outer outline or hole). `points` is the loop
// WITHOUT repeating the first point — the last point connects back to the
// first. Coordinates are in sketch-local mm, fully placed (anchor +
// alignment + rotation applied).
struct TextContour {
  std::vector<TextPoint> points;
};

struct TextLayout {
  // One entry per contour, in glyph order. Segment count per contour is
  // deterministic for a given (font, string, style) — the engine only
  // emits segments that can survive the sketch's zero-length cleanup.
  std::vector<TextContour> contours;
  // Bounding box of the placed text BEFORE the alignment shift (alignment
  // is already baked into the contour points above).
  double width = 0.0;
  double height = 0.0;
};

// Curve the text flows along (Fusion-style "Text on Path"). When set,
// every glyph is placed at its advance distance along the curve and
// rotated to the curve tangent. In path mode `angle_deg` and `v_align`
// are ignored — the tangent drives rotation and `path_offset` drives
// the above/below shift.
struct TextPath {
  bool is_arc = false;
  // Straight segment (is_arc == false).
  double start_x = 0.0;
  double start_y = 0.0;
  double end_x = 0.0;
  double end_y = 0.0;
  // Arc (is_arc == true). Travel runs from start_angle through
  // sweep_angle radians; direction +1 = counter-clockwise, -1 =
  // clockwise.
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  double start_angle = 0.0;
  double sweep_angle = 0.0;  // always positive
  int direction = 1;
};

// Parameters shared by the SketchText entity and the engine call.
struct TextStyle {
  // Empty = default font (embedded DejaVu via Font_FontMgr). Otherwise an
  // absolute .ttf path.
  std::string font_path;
  double height_mm = 10.0;
  double angle_deg = 0.0;
  double char_spacing = 0.0;
  // "left" | "center" | "right" — in path mode this aligns along the
  // path (start / middle / end of the curve).
  std::string h_align = "center";
  // "top" | "middle" | "bottom" — flat mode only.
  std::string v_align = "middle";
  // Text on path (Fusion "align to curve"). Nullopt = flat text.
  std::optional<TextPath> path;
  // Perpendicular baseline shift in mm (path mode only); positive =
  // left of the direction of travel. Multi-line text stacks each
  // further line one line-height to the right of travel.
  double path_offset = 0.0;
};

// Converts text strings into polygonal closed contours using OCCT's
// StdPrs_BRepFont (FreeType glyph outlines as BRep shapes). The core is
// single-threaded, so one process-wide instance holds the font cache.
class TextEngine {
 public:
  static TextEngine& instance();

  // Lays out `utf8_text` anchored at (anchor_x, anchor_y) with `style`.
  // Returns false and fills `error` when the font can't be loaded or the
  // layout fails; `out` is left untouched in that case.
  bool layout(const std::string& utf8_text,
              double anchor_x,
              double anchor_y,
              const TextStyle& style,
              TextLayout* out,
              std::string* error);

  // Chordal tessellation tolerance for a given font height. Proportional
  // to height so that a height edit scales the glyphs without changing
  // segment counts (stable generated entity ids = TNP-safe edits).
  // Clamped to [0.01, 0.2] mm; heights below 2 mm re-tessellate more
  // finely relative to the glyph and therefore break the id stability
  // contract (documented in the wiki).
  static double tessellation_tolerance(double height_mm);

  // Resolves the bundled font path: POLYSMITH_TEXT_FONT_PATH env var
  // first, then repo-relative fallbacks so core tests find the resource
  // without the Tauri spawn. Returns "" when nothing exists — the caller
  // then falls back to the embedded default font.
  static std::string bundled_font_path();

 private:
  // Defined out-of-line (text_engine.cpp) so the unique_ptr<Impl>
  // destructor never needs the incomplete Impl type in header-only TUs
  // (the sketch-profile test links a stub engine without OCCT).
  TextEngine();
  ~TextEngine();
  struct Impl;
  // Owned via unique_ptr to keep OCCT handles out of the header.
  std::unique_ptr<Impl> impl_;
};

}  // namespace polysmith::core::text
