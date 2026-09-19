#include "core/drawing/drawing_title_block.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/document/document_state.h"
#include "core/drawing/drawing_sheet.h"

namespace polysmith::core {

namespace {

// ISO 7200: the block is 180 mm wide, the height is free (63 mm is
// the common CAD default).  Bottom-right inside the frame.
constexpr double kBlockWidthMm = 180.0;
constexpr double kBlockHeightMm = 63.0;
constexpr double kMarginMm = 10.0;  // ISO 5457 margin incl. frame
constexpr double kThickLineMm = 0.5;
constexpr double kThinLineMm = 0.25;
// Field text sizes: labels 2.5, values 3.5, title + scale/sheet 5.
constexpr double kLabelH = 2.5;
constexpr double kValueH = 3.5;
constexpr double kBigH = 5.0;
constexpr double kInsetMm = 2.0;  // cell text inset from the divider
constexpr double kPi = 3.14159265358979323846;

void add_line(SheetPrimitiveStream& stream, std::array<double, 2> p0,
              std::array<double, 2> p1, double width_mm,
              const std::string& purpose = "title_block") {
  SheetPrimitive p;
  p.kind = "line";
  p.purpose = purpose;
  p.style = {"continuous", width_mm};
  p.p0 = p0;
  p.p1 = p1;
  stream.primitives.push_back(std::move(p));
}

void add_text(SheetPrimitiveStream& stream, const std::string& text, double x,
              double y, double height_mm, const std::string& h_align) {
  if (text.empty()) {
    return;
  }
  SheetText t;
  t.text = text;
  t.position = {x, y};
  t.height_mm = height_mm;
  t.h_align = h_align;
  t.purpose = "title_block";
  stream.texts.push_back(std::move(t));
}

/// One label-over-value cell: the field name (2.5 mm) at the cell
/// top, the value below it, both left-aligned with an inset.
void add_cell(SheetPrimitiveStream& stream, double x0, double x1, double y0,
              double y1, const std::string& label, const std::string& value,
              double value_h) {
  add_text(stream, label, x0 + kInsetMm, y1 - 2.5, kLabelH, "left");
  if (!value.empty()) {
    add_text(stream, value, x0 + kInsetMm, y0 + 2.0 + value_h / 2.0, value_h,
             "left");
  }
}

/// One single-line note centered in the cell.
void add_note(SheetPrimitiveStream& stream, double x0, double x1, double y0,
              double y1, const std::string& text) {
  add_text(stream, text, (x0 + x1) / 2.0, (y0 + y1) / 2.0, kLabelH, "center");
}

/// ISO 5455 scale as a label: 1:1, 2:1, 1:2, 1:5 … — the drawing's
/// decimal separator is honored (scales are exact in the series, so
/// the digits are integral in practice).
std::string format_scale(double scale, const std::string& separator) {
  if (std::abs(scale - 1.0) < 1e-9) {
    return "1:1";
  }
  const double n = scale > 1.0 ? scale : 1.0 / scale;
  std::string digits = std::to_string(std::round(n * 100.0) / 100.0);
  while (digits.size() > 1 && digits.back() == '0') {
    digits.pop_back();
  }
  if (!digits.empty() && digits.back() == '.') {
    digits.pop_back();
  }
  if (separator != ".") {
    const auto dot = digits.find('.');
    if (dot != std::string::npos) {
      digits.replace(dot, 1, separator);
    }
  }
  return scale > 1.0 ? digits + ":1" : "1:" + digits;
}

/// The ISO 5456-2 projection symbol inside the block's top-right
/// cell: a truncated cone (frustum) + two concentric circles.
/// h = 10d = 5 mm, H = 20d = 10 mm at d = 0.5.  First angle: the
/// frustum's LARGE end nearest the circles; third angle: the small
/// end nearest.  `center_x`/`center_y` anchor the 30×10 symbol.
void emit_projection_symbol(SheetPrimitiveStream& stream, double center_x,
                            double center_y,
                            const std::string& projection_angle) {
  const double H = 10.0;
  const double hh = 5.0;
  const double L = 15.0;
  const double symbol_left = center_x - (L + 1.5 * H) / 2.0;  // 30 wide
  const double x1 = symbol_left + 1.5 * H;   // near base of the frustum
  const double cy = center_y;                // centerline
  const double circle_cx = symbol_left + H / 2.0;

  const auto line = [&](std::array<double, 2> a, std::array<double, 2> b) {
    add_line(stream, a, b, kThickLineMm, "projection_symbol");
  };
  const auto circle = [&](double cx, double r) {
    SheetPrimitive p;
    p.kind = "circle_arc";
    p.purpose = "projection_symbol";
    p.style = {"continuous", kThickLineMm};
    p.center = {{cx, cy}};
    p.radius = r;
    p.start_angle = 0.0;
    p.end_angle = 2.0 * kPi;
    p.p0 = {cx + r, cy};
    p.p1 = {cx + r, cy};
    stream.primitives.push_back(std::move(p));
  };
  circle(circle_cx, H / 2.0);
  circle(circle_cx, H / 4.0);
  // Frustum: near base (x1) and far base (x1 + L); first angle keeps
  // the large end nearest the circles.
  const double near_h = projection_angle == "first_angle" ? H : hh;
  const double far_h = projection_angle == "first_angle" ? hh : H;
  line({x1, cy - near_h / 2.0}, {x1, cy + near_h / 2.0});
  line({x1 + L, cy - far_h / 2.0}, {x1 + L, cy + far_h / 2.0});
  line({x1, cy - near_h / 2.0}, {x1 + L, cy - far_h / 2.0});
  line({x1, cy + near_h / 2.0}, {x1 + L, cy + far_h / 2.0});
}

/// The revision table above the block (convention, not normative —
/// ISO 7200 leaves revisions optional).  Columns zone/rev/
/// description/date/approved; the header sits on the block top edge
/// and rows stack upward (newest entry nearest the header).  Drawn
/// only while at least one row exists.
void emit_revision_table(SheetPrimitiveStream& stream,
                         const TitleBlock& title_block, double bx, double top,
                         double right) {
  if (title_block.revision_rows.empty()) {
    return;
  }
  // Bound the table height: sheets are finite, 20 rows is plenty for
  // a v1 drawing.
  const size_t rows = std::min<size_t>(title_block.revision_rows.size(), 20);
  const double row_h = 5.0;
  const double table_top = top + row_h * (1.0 + static_cast<double>(rows));
  const double columns[6] = {0.0, 16.0, 32.0, 122.0, 146.0, 180.0};

  for (size_t i = 0; i <= rows; ++i) {
    const double y = top + row_h * static_cast<double>(i);
    add_line(stream, {bx, y}, {right, y}, kThinLineMm);
  }
  for (double column : columns) {
    add_line(stream, {bx + column, top}, {bx + column, table_top},
             kThinLineMm);
  }

  static const char* kHeaders[5] = {"Zone", "Rev", "Description", "Date",
                                    "Approved"};
  for (int c = 0; c < 5; ++c) {
    add_text(stream, kHeaders[c], bx + (columns[c] + columns[c + 1]) / 2.0,
             top + row_h / 2.0, kLabelH, "center");
  }
  for (size_t i = 0; i < rows; ++i) {
    const double y = top + row_h * (static_cast<double>(i) + 1.5);
    for (int c = 0; c < 5; ++c) {
      const auto& cell = title_block.revision_rows[i][c];
      if (cell.empty()) {
        continue;
      }
      add_text(stream, cell, bx + (columns[c] + columns[c + 1]) / 2.0, y,
               kLabelH, "center");
    }
  }
}

}  // namespace

void flatten_title_block(const DocumentState& document,
                         const Drawing& drawing, const DrawingSheet& sheet,
                         size_t sheet_index, size_t sheet_count,
                         SheetPrimitiveStream& stream) {
  const double bx = stream.width_mm - kMarginMm - kBlockWidthMm;
  const double by = kMarginMm;
  const double top = by + kBlockHeightMm;
  const double right = bx + kBlockWidthMm;

  // Grid: vertical dividers at 50/100/130, horizontal at 14/30/44.
  const double vx[4] = {bx + 50.0, bx + 100.0, bx + 130.0, right};
  const double hy[3] = {by + 14.0, by + 30.0, by + 44.0};
  add_line(stream, {bx, by}, {right, by}, kThickLineMm);
  add_line(stream, {right, by}, {right, top}, kThickLineMm);
  add_line(stream, {right, top}, {bx, top}, kThickLineMm);
  add_line(stream, {bx, top}, {bx, by}, kThickLineMm);
  for (double x : vx) {
    add_line(stream, {x, by}, {x, top}, kThinLineMm);
  }
  for (double y : hy) {
    add_line(stream, {bx, y}, {right, y}, kThinLineMm);
  }

  // ── Cells (ISO 7200 mandatory fields + optional notes) ─────────
  // Top strip: notes + auto-fills.
  add_note(stream, bx, bx + 50.0, by + 44.0, top,
           "Dimensions in millimetres");
  // Scale auto-fills from the sheet's FIRST view (ISO 5455 series).
  std::string scale_text = "1:1";
  for (const auto& view_id : sheet.view_ids) {
    const auto found = std::find_if(
        drawing.views.begin(), drawing.views.end(),
        [&](const DrawingView& v) { return v.view_id == view_id; });
    if (found != drawing.views.end()) {
      scale_text = format_scale(found->scale,
                                document.drawing.decimal_separator);
      break;
    }
  }
  add_cell(stream, bx + 50.0, bx + 100.0, by + 44.0, top, "Scale",
           scale_text, kBigH);
  add_cell(stream, bx + 100.0, bx + 130.0, by + 44.0, top, "Sheet",
           std::to_string(sheet_index + 1) + "/" +
               std::to_string(sheet_count),
           kBigH);
  // Projection symbol in the top-right cell (inside the block, per
  // ISO 7200's "near or inside the title block").
  emit_projection_symbol(stream, bx + 155.0, by + 53.5,
                         sheet.projection_angle);

  // Middle rows: the remaining mandatory fields.
  add_cell(stream, bx, bx + 130.0, by + 30.0, by + 44.0, "Legal owner",
           sheet.title_block.legal_owner, kValueH);
  add_cell(stream, bx + 130.0, right, by + 30.0, by + 44.0,
           "Identification number", sheet.title_block.identification,
           kValueH);
  add_cell(stream, bx, bx + 130.0, by + 14.0, by + 30.0, "Title",
           sheet.title_block.title, kBigH);
  add_cell(stream, bx + 130.0, right, by + 14.0, by + 30.0, "Document type",
           sheet.title_block.document_type, kValueH);

  // Bottom row: creator / approver / date + the ISO 8015 note.
  add_cell(stream, bx, bx + 50.0, by, by + 14.0, "Creator",
           sheet.title_block.creator, kLabelH);
  add_cell(stream, bx + 50.0, bx + 100.0, by, by + 14.0, "Approval person",
           sheet.title_block.approver, kLabelH);
  add_cell(stream, bx + 100.0, bx + 130.0, by, by + 14.0, "Date of issue",
           sheet.title_block.date, kLabelH);
  add_note(stream, bx + 130.0, right, by, by + 14.0,
           "Tolerancing per ISO 8015");

  emit_revision_table(stream, sheet.title_block, bx, top, right);
}

}  // namespace polysmith::core
