// Text engine unit tests — layout, determinism, tessellation policy,
// multi-line / spacing / angle / alignment behavior, and graceful font
// load failures. The engine renders through OCCT's StdPrs_BRepFont with
// the embedded DejaVu fallback, so no font file is required on disk.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/text_engine.h"

namespace {

using polysmith::core::text::TextEngine;
using polysmith::core::text::TextLayout;
using polysmith::core::text::TextStyle;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

void bounds(const TextLayout& layout,
            double* min_x,
            double* max_x,
            double* min_y,
            double* max_y) {
  *min_x = *max_x = *min_y = *max_y = 0.0;
  bool any = false;
  for (const auto& contour : layout.contours) {
    for (const auto& point : contour.points) {
      if (!any) {
        *min_x = *max_x = point.x;
        *min_y = *max_y = point.y;
        any = true;
      } else {
        *min_x = std::min(*min_x, point.x);
        *max_x = std::max(*max_x, point.x);
        *min_y = std::min(*min_y, point.y);
        *max_y = std::max(*max_y, point.y);
      }
    }
  }
}

size_t point_count(const TextLayout& layout) {
  size_t count = 0;
  for (const auto& contour : layout.contours) {
    count += contour.points.size();
  }
  return count;
}

bool same_layout(const TextLayout& a, const TextLayout& b) {
  if (a.contours.size() != b.contours.size()) return false;
  for (size_t c = 0; c < a.contours.size(); ++c) {
    if (a.contours[c].points.size() != b.contours[c].points.size()) {
      return false;
    }
    for (size_t p = 0; p < a.contours[c].points.size(); ++p) {
      if (a.contours[c].points[p].x != b.contours[c].points[p].x ||
          a.contours[c].points[p].y != b.contours[c].points[p].y) {
        return false;
      }
    }
  }
  return true;
}

bool test_default_font_renders_text() {
  TextLayout layout;
  std::string error;
  const TextStyle style{/*font_path=*/"", /*height_mm=*/10.0};
  if (!expect(TextEngine::instance().layout("HELLO", 0.0, 0.0, style,
                                            &layout, &error),
              "layout: default font must render HELLO")) {
    return false;
  }
  if (!expect(!layout.contours.empty(), "layout: contours present")) {
    return false;
  }
  double min_x, max_x, min_y, max_y;
  bounds(layout, &min_x, &max_x, &min_y, &max_y);
  const double width = max_x - min_x;
  const double height = max_y - min_y;
  if (!expect(width > 10.0,
              "layout: HELLO at 10mm spans >10mm (5 caps ~ 0.6em each)")) {
    return false;
  }
  if (!expect(height > 4.0 && height < 15.0,
              "layout: HELLO cap height is on the order of 10mm")) {
    return false;
  }
  // Centered + middle aligned at (0,0): bounds straddle the origin.
  return expect(min_x < 0.0 && max_x > 0.0 && min_y < 0.0 && max_y > 0.0,
                "layout: center/middle alignment centers bounds on anchor");
}

bool test_determinism() {
  const TextStyle style{/*font_path=*/"", /*height_mm=*/10.0};
  TextLayout first, second;
  std::string error;
  if (!TextEngine::instance().layout("PolySmith 123", 3.0, -2.0, style,
                                     &first, &error) ||
      !TextEngine::instance().layout("PolySmith 123", 3.0, -2.0, style,
                                     &second, &error)) {
    return expect(false, "determinism: layout calls must succeed");
  }
  return expect(same_layout(first, second),
                "determinism: identical inputs give bit-identical output");
}

bool test_tolerance_policy() {
  if (!expect(TextEngine::tessellation_tolerance(10.0) >
                  TextEngine::tessellation_tolerance(5.0),
              "tolerance: proportional to height")) {
    return false;
  }
  if (!expect(TextEngine::tessellation_tolerance(0.1) == 0.01,
              "tolerance: floor at 0.01 mm")) {
    return false;
  }
  return expect(TextEngine::tessellation_tolerance(500.0) == 0.2,
                "tolerance: cap at 0.2 mm");
}

bool test_multiline() {
  TextLayout one_line, two_lines;
  std::string error;
  const TextStyle style{/*font_path=*/"", /*height_mm=*/10.0};
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &one_line,
                                     &error) ||
      !TextEngine::instance().layout("AB\nCD", 0.0, 0.0, style, &two_lines,
                                     &error)) {
    return expect(false, "multiline: layout calls must succeed");
  }
  double min_x, max_x, min_y, max_y;
  bounds(one_line, &min_x, &max_x, &min_y, &max_y);
  const double height_one = max_y - min_y;
  bounds(two_lines, &min_x, &max_x, &min_y, &max_y);
  const double height_two = max_y - min_y;
  if (!expect(height_two > height_one * 1.4,
              "multiline: two lines are taller than one")) {
    return false;
  }
  // 4 glyphs, each a closed contour with its own profile-worth outline
  // (A/B/C have holes, so contours >= 4).
  return expect(two_lines.contours.size() >= 4,
                "multiline: contours for all four glyphs present");
}

bool test_char_spacing_widens() {
  TextLayout tight, spaced;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0};
  // Four glyphs give three inter-character gaps, so +50% advance grows
  // the width by a wide margin regardless of the font's kerning table.
  if (!TextEngine::instance().layout("ABCD", 0.0, 0.0, style, &tight,
                                     &error)) {
    return expect(false, "spacing: tight layout must succeed");
  }
  style.char_spacing = 0.5;
  if (!TextEngine::instance().layout("ABCD", 0.0, 0.0, style, &spaced,
                                     &error)) {
    return expect(false, "spacing: spaced layout must succeed");
  }
  double min_x, max_x, min_y, max_y;
  bounds(tight, &min_x, &max_x, &min_y, &max_y);
  const double tight_width = max_x - min_x;
  bounds(spaced, &min_x, &max_x, &min_y, &max_y);
  const double spaced_width = max_x - min_x;
  return expect(spaced_width > tight_width * 1.25,
                "spacing: +50% character spacing widens the text");
}

bool test_angle_rotates_bounds() {
  TextLayout straight, rotated;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0};
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &straight,
                                     &error)) {
    return expect(false, "angle: straight layout must succeed");
  }
  style.angle_deg = 90.0;
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &rotated,
                                     &error)) {
    return expect(false, "angle: rotated layout must succeed");
  }
  double min_x, max_x, min_y, max_y;
  bounds(straight, &min_x, &max_x, &min_y, &max_y);
  const double sw = max_x - min_x;
  const double sh = max_y - min_y;
  bounds(rotated, &min_x, &max_x, &min_y, &max_y);
  const double rw = max_x - min_x;
  const double rh = max_y - min_y;
  return expect(std::abs(rw - sh) < 0.5 && std::abs(rh - sw) < 0.5,
                "angle: 90 degrees swaps the bounding box axes");
}

bool test_h_align_anchors() {
  TextLayout left, right;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0,
                  /*angle_deg=*/0.0, /*char_spacing=*/0.0,
                  /*h_align=*/"left"};
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &left,
                                     &error)) {
    return expect(false, "align: left layout must succeed");
  }
  style.h_align = "right";
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &right,
                                     &error)) {
    return expect(false, "align: right layout must succeed");
  }
  double min_x, max_x, min_y, max_y;
  bounds(left, &min_x, &max_x, &min_y, &max_y);
  const double left_min_x = min_x;
  bounds(right, &min_x, &max_x, &min_y, &max_y);
  // Left-aligned text starts at the anchor (min_x ≈ 0); right-aligned
  // text ends at the anchor (max_x ≈ 0) and lives to its left.
  return expect(std::abs(left_min_x) < 0.001 && std::abs(max_x) < 0.001,
                "align: left starts at anchor, right ends at anchor");
}

bool test_line_path_rotates_to_tangent() {
  // Vertical path → every glyph rotates 90°: the text's bounds swap
  // axes compared to the flat layout.
  TextLayout flat, on_path;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0};
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &flat,
                                     &error)) {
    return expect(false, "path: flat layout must succeed");
  }
  style.path = polysmith::core::text::TextPath{
      .start_x = 0.0, .start_y = 0.0, .end_x = 0.0, .end_y = 100.0};
  style.h_align = "left";
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &on_path,
                                     &error)) {
    return expect(false, "path: line-path layout must succeed");
  }
  double min_x, max_x, min_y, max_y;
  bounds(flat, &min_x, &max_x, &min_y, &max_y);
  const double flat_width = max_x - min_x;
  const double flat_height = max_y - min_y;
  bounds(on_path, &min_x, &max_x, &min_y, &max_y);
  const double path_width = max_x - min_x;
  const double path_height = max_y - min_y;
  if (!expect(std::abs(path_width - flat_height) < 2.0 &&
                  std::abs(path_height - flat_width) < 2.0,
              "path: vertical path rotates the text 90 degrees")) {
    return false;
  }
  // Every glyph point stays within glyph-extent of the path's x == 0.
  return expect(std::abs(min_x) < 15.0 && std::abs(max_x) < 15.0,
                "path: glyphs hug the vertical path line");
}

bool test_arc_path_places_on_circle() {
  TextLayout layout;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0,
                  /*angle_deg=*/0.0, /*char_spacing=*/0.0,
                  /*h_align=*/"left"};
  style.path = polysmith::core::text::TextPath{
      .is_arc = true,
      .center_x = 0.0,
      .center_y = 0.0,
      .radius = 50.0,
      .start_angle = 0.0,
      .sweep_angle = 3.141592653589793,
      .direction = 1,
  };
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &layout,
                                     &error)) {
    return expect(false, "path: arc-path layout must succeed");
  }
  if (!expect(!layout.contours.empty(), "path: contours present")) {
    return false;
  }
  // The baseline rides the circle; glyph points extend radially (local
  // +y is outward after the tangent rotation). Every point must stay
  // within glyph-extent of the circle radius.
  for (const auto& contour : layout.contours) {
    for (const auto& point : contour.points) {
      const double radius = std::sqrt(point.x * point.x + point.y * point.y);
      if (!expect(radius > 35.0 && radius < 65.0,
                  "path: glyph points hug the circle")) {
        return false;
      }
    }
  }
  return true;
}

bool test_path_offset_shifts_perpendicular() {
  // Horizontal path: two path layouts differing only by path_offset
  // differ by an exact rigid +10 mm shift perpendicular to the path
  // (path mode applies no alignment shift, so the comparison is exact).
  TextLayout base, offset;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0,
                  /*angle_deg=*/0.0, /*char_spacing=*/0.0,
                  /*h_align=*/"left"};
  style.path = polysmith::core::text::TextPath{
      .start_x = 0.0, .start_y = 0.0, .end_x = 100.0, .end_y = 0.0};
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &base,
                                     &error)) {
    return expect(false, "offset: base path layout must succeed");
  }
  style.path_offset = 10.0;
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &offset,
                                     &error)) {
    return expect(false, "offset: shifted path layout must succeed");
  }
  if (!expect(base.contours.size() == offset.contours.size(),
              "offset: same contour structure")) {
    return false;
  }
  for (size_t c = 0; c < base.contours.size(); ++c) {
    if (!expect(base.contours[c].points.size() ==
                    offset.contours[c].points.size(),
                "offset: same point counts")) {
      return false;
    }
    for (size_t p = 0; p < base.contours[c].points.size(); ++p) {
      const auto& b = base.contours[c].points[p];
      const auto& o = offset.contours[c].points[p];
      if (!expect(std::abs(o.x - b.x) < 1e-6 &&
                      std::abs(o.y - (b.y + 10.0)) < 1e-6,
                  "offset: points shifted exactly 10 mm perpendicular")) {
        return false;
      }
    }
  }
  return true;
}

bool test_path_align_centers_along_curve() {
  TextLayout layout;
  std::string error;
  TextStyle style{/*font_path=*/"", /*height_mm=*/10.0,
                  /*angle_deg=*/0.0, /*char_spacing=*/0.0,
                  /*h_align=*/"center"};
  style.path = polysmith::core::text::TextPath{
      .start_x = 0.0, .start_y = 0.0, .end_x = 100.0, .end_y = 0.0};
  if (!TextEngine::instance().layout("AB", 0.0, 0.0, style, &layout,
                                     &error)) {
    return expect(false, "align: path layout must succeed");
  }
  double min_x, max_x, min_y, max_y;
  bounds(layout, &min_x, &max_x, &min_y, &max_y);
  return expect(std::abs((min_x + max_x) / 2.0 - 50.0) < 2.0,
                "align: center aligns the text on the curve midpoint");
}

bool test_missing_font_fails_gracefully() {
  TextLayout layout;
  std::string error;
  const TextStyle bad{/*font_path=*/"/nonexistent/no-such-font.ttf",
                      /*height_mm=*/10.0};
  if (!expect(!TextEngine::instance().layout("AB", 0.0, 0.0, bad, &layout,
                                             &error),
              "font: missing file must fail")) {
    return false;
  }
  return expect(!error.empty(), "font: failure carries an error message");
}

}  // namespace

int main() {
  if (!test_default_font_renders_text()) return 1;
  if (!test_determinism()) return 1;
  if (!test_tolerance_policy()) return 1;
  if (!test_multiline()) return 1;
  if (!test_char_spacing_widens()) return 1;
  if (!test_angle_rotates_bounds()) return 1;
  if (!test_h_align_anchors()) return 1;
  if (!test_missing_font_fails_gracefully()) return 1;
  if (!test_line_path_rotates_to_tangent()) return 1;
  if (!test_arc_path_places_on_circle()) return 1;
  if (!test_path_offset_shifts_perpendicular()) return 1;
  if (!test_path_align_centers_along_curve()) return 1;

  std::cout << "text_engine_test passed\n";
  return 0;
}
