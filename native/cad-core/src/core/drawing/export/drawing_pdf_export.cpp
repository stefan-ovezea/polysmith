#include "core/drawing/export/drawing_export.h"

#include "hpdf.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/diagnostics/logger.h"
#include "core/text_engine.h"

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kEllipseSegments = 48;

// libharu raises errors through a callback; the export converts the
// first error into a runtime_error at SaveToFile time.
struct PdfErrorContext {
  bool failed = false;
  HPDF_STATUS code = 0;
};

void pdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no,
                       void* user_data) {
  auto* context = static_cast<PdfErrorContext*>(user_data);
  if (!context->failed) {
    context->failed = true;
    context->code = error_no;
  }
  (void)detail_no;
}

/// The PageArc angles are DEGREES, y-up, 0° = +x — the same
/// convention as the sheet's math coordinates, so no flip anywhere
/// (PDF user space is y-up).
double deg(double radians) { return radians * 180.0 / kPi; }

/// A partial ellipse (start..end angle along the major axis) — the
/// PDF backend tessellates it to a polyline (the same fidelity the
/// SVG backend and the viewport use).
std::vector<std::array<double, 2>> ellipse_points(const SheetPrimitive& p) {
  const double cx = p.center.value()[0];
  const double cy = p.center.value()[1];
  const double mx = p.major_dir.value()[0];
  const double my = p.major_dir.value()[1];
  const double major = p.major_radius.value();
  const double minor = p.minor_radius.value();
  const double sweep = p.end_angle - p.start_angle;
  const int steps = std::max(2, static_cast<int>(
      std::ceil(std::abs(sweep) / (2.0 * kPi) * kEllipseSegments)));
  std::vector<std::array<double, 2>> points;
  points.reserve(steps + 1);
  for (int i = 0; i <= steps; ++i) {
    const double a = p.start_angle + sweep * i / steps;
    points.push_back(
        {cx + mx * major * std::cos(a) - my * minor * std::sin(a),
         cy + my * major * std::cos(a) + mx * minor * std::sin(a)});
  }
  return points;
}

}  // namespace

ExportResult export_sheet_as_pdf(const SheetPrimitiveStream& stream,
                                 const std::string& file_path) {
  PdfErrorContext error;
  HPDF_Doc doc = HPDF_New(pdf_error_handler, &error);
  if (doc == nullptr) {
    throw std::runtime_error("Cannot create PDF document: " + file_path);
  }
  HPDF_SetCompressionMode(doc, HPDF_COMP_ALL);

  // The bundled ISO 3098 font travels INTO the PDF (subset-embedded)
  // so the drawing text is selectable and prints identically on
  // machines without the font.  When no bundled font exists, fall
  // back to the stream's glyph primitives instead (text records stay
  // DATA either way).  libharu cannot rotate a text run, so real-text
  // mode needs EVERY record horizontal — otherwise the glyph
  // primitives carry the whole sheet (the SVG-backend behavior).
  const bool all_texts_horizontal = std::all_of(
      stream.texts.begin(), stream.texts.end(),
      [](const SheetText& t) { return t.angle_deg == 0.0; });
  std::string font_path = text::TextEngine::bundled_iso3098_font_path();
  if (font_path.empty()) {
    font_path = text::TextEngine::bundled_font_path();
  }
  const char* font_name = nullptr;
  if (!font_path.empty()) {
    font_name = HPDF_LoadTTFontFromFile(doc, font_path.c_str(),
                                        HPDF_TRUE /*embed*/);
    if (font_name != nullptr) {
      HPDF_UseUTFEncodings(doc);
    } else {
      // libharu raises the load failure through the error handler —
      // it is HANDLED by the glyph fallback, so clear the flag.
      error.failed = false;
      log_warn("drawing_export",
               "PDF: could not load font " + font_path +
                   " — falling back to vector glyph primitives");
    }
  }
  const bool use_real_text = font_name != nullptr && all_texts_horizontal;

  HPDF_Page page = HPDF_AddPage(doc);
  HPDF_Page_SetWidth(page, static_cast<HPDF_REAL>(stream.width_mm));
  HPDF_Page_SetHeight(page, static_cast<HPDF_REAL>(stream.height_mm));
  HPDF_Page_SetLineJoin(page, HPDF_ROUND_JOIN);

  int exported_count = 0;
  for (const auto& p : stream.primitives) {
    HPDF_Page_SetLineWidth(page, static_cast<HPDF_REAL>(p.style.width_mm));
    if (p.kind == "line") {
      // Includes the text_glyph primitives when real text is
      // unavailable (see above).
      if (p.purpose == "text_glyph" && use_real_text) {
        continue;
      }
      HPDF_Page_MoveTo(page, p.p0[0], p.p0[1]);
      HPDF_Page_LineTo(page, p.p1[0], p.p1[1]);
      HPDF_Page_Stroke(page);
    } else if (p.kind == "circle_arc" && p.center.has_value() &&
               p.radius.has_value()) {
      const double sweep = std::abs(p.end_angle - p.start_angle);
      if (sweep >= 2.0 * kPi - 1e-9) {
        HPDF_Page_Circle(page, p.center.value()[0], p.center.value()[1],
                         p.radius.value());
        HPDF_Page_Stroke(page);
      } else {
        HPDF_Page_Arc(page, p.center.value()[0], p.center.value()[1],
                      p.radius.value(), deg(p.start_angle),
                      deg(p.end_angle));
        HPDF_Page_Stroke(page);
      }
    } else if (p.kind == "ellipse_arc" && p.center.has_value() &&
               p.major_dir.has_value() && p.major_radius.has_value() &&
               p.minor_radius.has_value()) {
      const auto points = ellipse_points(p);
      HPDF_Page_MoveTo(page, points[0][0], points[0][1]);
      for (size_t i = 1; i < points.size(); ++i) {
        HPDF_Page_LineTo(page, points[i][0], points[i][1]);
      }
      HPDF_Page_Stroke(page);
    } else if (p.kind == "filled_poly" && p.points.size() >= 3) {
      HPDF_Page_MoveTo(page, p.points[0][0], p.points[0][1]);
      for (size_t i = 1; i < p.points.size(); ++i) {
        HPDF_Page_LineTo(page, p.points[i][0], p.points[i][1]);
      }
      HPDF_Page_Fill(page);
    } else {
      continue;  // unknown kind — skipped defensively
    }
    ++exported_count;
  }

  // Real text: the records' anchor is the CENTER of the text box;
  // libharu places the BASELINE, so shift up by ~0.35×height (the
  // cap-height middle).  h_align offsets by the measured width.
  // All drawing texts are horizontal today (unidirectional dimension
  // text, title block) — rotated text falls back to the glyph
  // primitives.
  if (use_real_text) {
    HPDF_Font font = HPDF_GetFont(doc, font_name, "UTF-8");
    for (const auto& t : stream.texts) {
      if (t.angle_deg != 0.0 || t.text.empty()) {
        continue;
      }
      const double height = t.height_mm;
      // libharu measures in 1/1000 text-em units — convert to user
      // units (mm) at the record's height.
      const double width =
          HPDF_Font_TextWidth(
              font, reinterpret_cast<const HPDF_BYTE*>(t.text.c_str()),
              static_cast<HPDF_UINT>(t.text.size()))
              .width /
          1000.0 * height;
      double x = t.position[0];
      if (t.h_align == "left") {
        // the anchor IS the left edge
      } else if (t.h_align == "right") {
        x -= width;
      } else {
        x -= width / 2.0;
      }
      const double y = t.position[1] - 0.35 * height;
      HPDF_Page_BeginText(page);
      HPDF_Page_SetFontAndSize(page, font, height);
      HPDF_Page_TextOut(page, x, y, t.text.c_str());
      HPDF_Page_EndText(page);
      ++exported_count;
    }
  }

  if (error.failed) {
    HPDF_Free(doc);
    throw std::runtime_error("PDF generation failed (error " +
                             std::to_string(static_cast<long>(error.code)) +
                             ")");
  }
  if (HPDF_SaveToFile(doc, file_path.c_str()) != HPDF_OK || error.failed) {
    HPDF_Free(doc);
    throw std::runtime_error("Cannot write PDF file: " + file_path);
  }
  HPDF_Free(doc);
  return ExportResult{file_path, "pdf", exported_count};
}

}  // namespace polysmith::core
