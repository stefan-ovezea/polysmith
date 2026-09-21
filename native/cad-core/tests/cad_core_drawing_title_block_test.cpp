// Drawing title block test (P7).
//
// Pins the ISO 7200 title block + drawing text pipeline end-to-end:
//   - drawing_title_block_update stores the eight mandatory fields
//     plus revision rows; save/load and undo/redo round-trip them
//   - the flatten renders the block bottom-right inside the frame
//     (purpose "title_block" lines + one SheetText per non-empty
//     field) with the auto-fills: the ISO 5455 scale from the
//     sheet's FIRST view, "Sheet x/y" from the sheet index,
//     "Dimensions in millimetres" + the ISO 8015 note, and the
//     projection symbol inside the top-right cell
//   - revision rows render above the block (zone/rev/description/
//     date/approved)
//   - every text record emits deterministic OSIFONT vector glyphs
//     (purpose "text_glyph") — golden
//   - section views label their cutting-plane trace on the parent
//     view (the label letter at both ends + a filled arrow each,
//     purpose "section_label")

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <BRepPrimAPI_MakeBox.hxx>

#include "core/document/document.h"
#include "core/drawing/drawing_sheet.h"
#include "core/drawing/drawing_text.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::SectionDefinition;
using polysmith::core::SheetPrimitive;
using polysmith::core::SheetPrimitiveStream;
using polysmith::core::SheetText;
using polysmith::core::TitleBlock;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

std::filesystem::path golden_dir() {
  std::filesystem::path source(__FILE__);
  return source.parent_path() / "golden";
}

bool check_golden(const std::string& name, const std::string& text) {
  const bool update = std::getenv("POLYSMITH_UPDATE_GOLDEN") != nullptr;
  const auto path = golden_dir() / (name + ".txt");
  if (update) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path.string());
    out << text;
    return expect(out.good(), "golden file written");
  }
  std::ifstream in(path.string());
  if (!in.good()) {
    std::cerr << "FAIL: golden file missing: " << path.string()
              << " (run with POLYSMITH_UPDATE_GOLDEN=1)\n";
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return expect(buffer.str() == text,
                ("golden mismatch: " + path.string()).c_str());
}

int count_purpose(const SheetPrimitiveStream& stream,
                  const std::string& purpose) {
  return static_cast<int>(std::count_if(
      stream.primitives.begin(), stream.primitives.end(),
      [&](const SheetPrimitive& p) { return p.purpose == purpose; }));
}

bool has_text(const SheetPrimitiveStream& stream, const std::string& text,
              const std::string& purpose = "") {
  return std::any_of(
      stream.texts.begin(), stream.texts.end(), [&](const SheetText& t) {
        return t.text == text && (purpose.empty() || t.purpose == purpose);
      });
}

// ── Fixture: document with a 20x20x10 box, a drawing on A4, and a
//    front view at sheet [30, 40], scale 1.
struct TitleBlockFixture {
  DocumentManager manager;
  DocumentState document;
  std::string body_id;
  std::string drawing_id;
  std::string sheet_id;
  std::string view_id;
};

TitleBlockFixture make_fixture() {
  TitleBlockFixture fixture;
  fixture.manager.create_document();
  fixture.manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      fixture.manager.get_document().value(), /*include_meshes=*/false);
  fixture.body_id = bodies.bodies[0].id;

  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  sheet.orientation = "portrait";  // explicit: the tests pin portrait
  sheet.paper_size = "A4";
  drawing.sheets.push_back(sheet);
  fixture.document = fixture.manager.drawing_create(drawing);
  fixture.drawing_id = fixture.document.drawing.drawings[0].drawing_id;
  fixture.sheet_id = fixture.document.drawing.drawings[0].sheets[0].sheet_id;

  DrawingView view;
  view.kind = "projection";
  view.standard_view = "front";
  view.source_body_ids = {fixture.body_id};
  view.sheet_position = {30.0, 40.0};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, view);
  fixture.view_id = fixture.document.drawing.drawings[0].views[0].view_id;
  return fixture;
}

TitleBlock filled_title_block() {
  TitleBlock title_block;
  title_block.legal_owner = "Polysmith GmbH";
  title_block.identification = "PS-2026-001";
  title_block.date = "2026-09-19";
  title_block.title = "Test Bracket";
  title_block.approver = "A. Approver";
  title_block.creator = "C. Creator";
  title_block.document_type = "Part drawing";
  return title_block;
}

bool test_eight_fields_render() {
  TitleBlockFixture fixture = make_fixture();
  TitleBlock title_block = filled_title_block();
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, title_block);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!flat.has_value()) {
    return expect(false, "flatten_sheet returns a stream");
  }

  if (!expect(count_purpose(flat.value(), "title_block") > 10,
              "title block lines exist")) {
    return false;
  }
  // Every non-empty field value is a text record.
  if (!expect(has_text(flat.value(), "Polysmith GmbH", "title_block"),
              "legal owner rendered") ||
      !expect(has_text(flat.value(), "PS-2026-001", "title_block"),
              "identification rendered") ||
      !expect(has_text(flat.value(), "2026-09-19", "title_block"),
              "date rendered") ||
      !expect(has_text(flat.value(), "Test Bracket", "title_block"),
              "title rendered") ||
      !expect(has_text(flat.value(), "A. Approver", "title_block"),
              "approver rendered") ||
      !expect(has_text(flat.value(), "C. Creator", "title_block"),
              "creator rendered") ||
      !expect(has_text(flat.value(), "Part drawing", "title_block"),
              "document type rendered")) {
    return false;
  }
  // The optional ISO 7200 extras + auto-fills.
  if (!expect(has_text(flat.value(), "Dimensions in millimetres",
                       "title_block"),
              "units note rendered") ||
      !expect(has_text(flat.value(), "Tolerancing per ISO 8015",
                       "title_block"),
              "ISO 8015 note rendered") ||
      !expect(has_text(flat.value(), "1:1", "title_block"),
              "scale auto-fill 1:1") ||
      !expect(has_text(flat.value(), "1/1", "title_block"),
              "sheet x/y auto-fill")) {
    return false;
  }
  // The projection symbol sits INSIDE the block now (top-right cell:
  // the A4 block spans x 20..200, y 10..73 — the cell is 150..200,
  // 54..73).
  bool symbol_inside = true;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "projection_symbol") {
      continue;
    }
    for (const auto& point : {p.p0, p.p1}) {
      if (point[0] < 150.0 || point[0] > 200.0 || point[1] < 54.0 ||
          point[1] > 73.0) {
        symbol_inside = false;
      }
    }
  }
  return expect(symbol_inside,
                "projection symbol inside the title block top-right cell");
}

bool test_scale_auto_fill_from_first_view() {
  TitleBlockFixture fixture = make_fixture();
  // First view at scale 0.5 → "1:2"; a second view at 2.0 does NOT
  // win — the FIRST view on the sheet defines the sheet scale.
  auto drawing = fixture.document.drawing.drawings[0];
  drawing.views[0].scale = 0.5;
  fixture.document = fixture.manager.drawing_view_update(
      fixture.drawing_id, drawing.views[0]);
  DrawingView second;
  second.kind = "projection";
  second.standard_view = "top";
  second.source_body_ids = {fixture.body_id};
  second.scale = 2.0;
  second.sheet_position = {30.0, 100.0};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, second);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!flat.has_value() ||
      !expect(has_text(flat.value(), "1:2", "title_block"),
              "scale auto-fills 1:2 from the first view")) {
    return false;
  }

  // A non-series scale still formats with the decimal separator.
  drawing = fixture.document.drawing.drawings[0];
  drawing.views[0].scale = 0.4;
  fixture.document = fixture.manager.drawing_view_update(
      fixture.drawing_id, drawing.views[0]);
  const auto flat_odd = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  return flat_odd.has_value() &&
         expect(has_text(flat_odd.value(), "1:2.5", "title_block"),
                "scale 0.4 formats as 1:2.5 (decimal dot)");
}

bool test_sheet_x_of_y() {
  TitleBlockFixture fixture = make_fixture();
  DrawingSheet second_sheet;
  second_sheet.name = "Sheet 2";
  second_sheet.orientation = "portrait";
  second_sheet.paper_size = "A4";
  fixture.document = fixture.manager.drawing_sheet_create(
      fixture.drawing_id, second_sheet);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  return flat.has_value() &&
         expect(has_text(flat.value(), "1/2", "title_block"),
                "first sheet shows 1/2 after a second sheet exists");
}

bool test_revision_rows_render() {
  TitleBlockFixture fixture = make_fixture();
  TitleBlock title_block = filled_title_block();
  title_block.revision_rows.push_back(
      {"A1", "B", "Added mounting holes", "2026-09-19", "A. Approver"});
  title_block.revision_rows.push_back(
      {"", "A", "Initial release", "2026-09-18", "A. Approver"});
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, title_block);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!flat.has_value()) {
    return expect(false, "flatten_sheet returns a stream");
  }
  return expect(has_text(flat.value(), "Added mounting holes",
                         "title_block"),
                "revision description rendered") &&
         expect(has_text(flat.value(), "Initial release", "title_block"),
                "second revision description rendered") &&
         expect(has_text(flat.value(), "B", "title_block"),
                "revision letter rendered");
}

bool test_glyphs_deterministic_golden() {
  TitleBlockFixture fixture = make_fixture();
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, filled_title_block());

  const auto dump_glyphs = [&]() {
    const auto flat = polysmith::core::flatten_sheet(
        fixture.document, fixture.drawing_id, fixture.sheet_id);
    std::ostringstream out;
    if (!flat.has_value()) {
      return std::string("NO STREAM\n");
    }
    out << "texts " << flat->texts.size() << "\n";
    for (const auto& t : flat->texts) {
      out << "text " << t.purpose << " \"" << t.text << "\" "
          << t.height_mm << " " << t.h_align << "\n";
    }
    out << "glyphs\n";
    for (const auto& p : flat->primitives) {
      if (p.purpose != "text_glyph") {
        continue;
      }
      out << std::fixed << std::setprecision(4) << p.p0[0] << ","
          << p.p0[1] << " " << p.p1[0] << "," << p.p1[1] << "\n";
    }
    return out.str();
  };

  const std::string first = dump_glyphs();
  // Glyphs must exist (the bundled OSIFONT resolved in the test env)
  // and re-flattening must reproduce them exactly.
  if (first.find("glyphs\n") == std::string::npos ||
      first.size() == std::string("texts 0\nglyphs\n").size()) {
    std::cerr << first;
    return expect(false, "vector glyphs emitted for the title block texts");
  }
  if (!expect(first == dump_glyphs(), "glyph layout is deterministic")) {
    return false;
  }
  return check_golden("drawing_title_block_glyphs", first);
}

bool test_section_labels() {
  TitleBlockFixture fixture = make_fixture();
  // A section view (plane y=10, normal +Y) — its cutting plane traces
  // onto the front view (edge-on) as a vertical line at view X=10 →
  // sheet x=40, y 40..50.
  DrawingView section_view;
  section_view.kind = "section";
  section_view.source_body_ids = {fixture.body_id};
  SectionDefinition section;
  section.cutting_plane_point = {0.0, 10.0, 0.0};
  section.cutting_plane_normal = {0.0, 1.0, 0.0};
  section.label = "B";
  section_view.section = section;
  section_view.scale = 0.5;
  section_view.sheet_position = {120.0, 40.0};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, section_view);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!flat.has_value()) {
    return expect(false, "flatten_sheet returns a stream");
  }
  // Two label letters + two filled arrows (one per trace end).
  int letter_count = 0;
  int arrow_count = 0;
  for (const auto& t : flat->texts) {
    if (t.purpose == "section_label") {
      ++letter_count;
      if (t.text != "B") {
        return expect(false, "section label letter rendered");
      }
    }
  }
  for (const auto& p : flat->primitives) {
    if (p.purpose != "section_label") {
      continue;
    }
    if (p.kind == "filled_poly" && p.points.size() == 3) {
      ++arrow_count;
    }
  }
  return expect(letter_count == 2, "label letter at both trace ends") &&
         expect(arrow_count == 2, "filled arrow at both trace ends");
}

bool test_save_load_roundtrip() {
  TitleBlockFixture fixture = make_fixture();
  TitleBlock title_block = filled_title_block();
  title_block.revision_rows.push_back(
      {"A1", "A", "Initial release", "2026-09-19", "A. Approver"});
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, title_block);

  // Round-trip through the payload layer (what the .polysmith file
  // stores).
  const auto payload =
      polysmith::protocol::to_payload(fixture.document);
  const auto restored =
      polysmith::protocol::document_from_payload(payload);
  const auto& restored_sheet = restored.drawing.drawings[0].sheets[0];
  const auto& restored_tb = restored_sheet.title_block;
  if (!expect(restored_tb.legal_owner == "Polysmith GmbH" &&
                  restored_tb.identification == "PS-2026-001" &&
                  restored_tb.date == "2026-09-19" &&
                  restored_tb.title == "Test Bracket" &&
                  restored_tb.approver == "A. Approver" &&
                  restored_tb.creator == "C. Creator" &&
                  restored_tb.document_type == "Part drawing",
              "all eight fields round-trip") ||
      !expect(restored_tb.revision_rows.size() == 1 &&
                  restored_tb.revision_rows[0][1] == "A" &&
                  restored_tb.revision_rows[0][2] == "Initial release",
              "revision rows round-trip")) {
    return false;
  }

  // And the restored document flattens identically.
  const auto flat_before = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  const auto flat_after = polysmith::core::flatten_sheet(
      restored, restored_sheet.sheet_id.empty()
                    ? fixture.sheet_id
                    : restored.drawing.drawings[0].drawing_id,
      restored.drawing.drawings[0].sheets[0].sheet_id);
  return flat_before.has_value() && flat_after.has_value() &&
         expect(flat_before->texts.size() == flat_after->texts.size(),
                "restored document flattens the same text set");
}

bool test_undo_redo() {
  TitleBlockFixture fixture = make_fixture();
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, filled_title_block());
  const auto flat_with = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!flat_with.has_value() ||
      !expect(has_text(flat_with.value(), "Test Bracket", "title_block"),
              "title block filled")) {
    return false;
  }

  fixture.manager.undo();
  const auto flat_undone = polysmith::core::flatten_sheet(
      fixture.manager.get_document().value(), fixture.drawing_id,
      fixture.sheet_id);
  if (!flat_undone.has_value() ||
      !expect(!has_text(flat_undone.value(), "Test Bracket", "title_block"),
              "undo clears the title block")) {
    return false;
  }

  fixture.manager.redo();
  const auto flat_redone = polysmith::core::flatten_sheet(
      fixture.manager.get_document().value(), fixture.drawing_id,
      fixture.sheet_id);
  return flat_redone.has_value() &&
         expect(has_text(flat_redone.value(), "Test Bracket",
                         "title_block"),
                "redo restores the title block");
}

bool test_landscape_keeps_title_block() {
  TitleBlockFixture fixture = make_fixture();
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, filled_title_block());
  // The user's flow: flip the sheet to landscape AFTER filling the
  // title block — every field must survive and the block must stay
  // inside the new sheet bounds (regression for "the title box does
  // not get populated in landscape").
  fixture.document = fixture.manager.drawing_sheet_update(
      fixture.drawing_id, fixture.sheet_id, "A4", "landscape",
      "first_angle", "Sheet 1");
  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!flat.has_value()) {
    return expect(false, "flatten_sheet returns a stream");
  }
  if (!expect(std::abs(flat->width_mm - 297.0) < 1e-6 &&
                  std::abs(flat->height_mm - 210.0) < 1e-6,
              "landscape stream dims 297x210")) {
    return false;
  }
  if (!expect(has_text(flat.value(), "Polysmith GmbH", "title_block") &&
                  has_text(flat.value(), "PS-2026-001", "title_block") &&
                  has_text(flat.value(), "Test Bracket", "title_block") &&
                  has_text(flat.value(), "Dimensions in millimetres",
                           "title_block"),
              "every title block field survives the orientation flip")) {
    return false;
  }
  if (!expect(count_purpose(flat.value(), "title_block") > 10,
              "title block lines exist in landscape")) {
    return false;
  }
  // The block + symbol re-anchor at the landscape bottom-right
  // (bx = 297−10−180 = 107) — nothing may land off the sheet.
  for (const auto& p : flat->primitives) {
    if (p.purpose != "title_block" && p.purpose != "projection_symbol") {
      continue;
    }
    for (const auto& point : {p.p0, p.p1}) {
      if (point[0] < 0.0 || point[0] > 297.0 || point[1] < 0.0 ||
          point[1] > 210.0) {
        std::cerr << "  DEBUG off-sheet point: " << point[0] << ", "
                  << point[1] << "\n";
        return expect(false, "title block inside the landscape sheet");
      }
    }
  }
  for (const auto& t : flat->texts) {
    if (t.purpose != "title_block") {
      continue;
    }
    if (t.position[0] < 0.0 || t.position[0] > 297.0 ||
        t.position[1] < 0.0 || t.position[1] > 210.0) {
      return expect(false, "title block texts inside the landscape sheet");
    }
  }
  return true;
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_title_block_test\n";
  std::cout << "  Test 1: eight mandatory fields + auto-fills render... ";
  if (test_eight_fields_render()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: scale auto-fill from the first view... ";
  if (test_scale_auto_fill_from_first_view()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: sheet x-of-y auto-fill... ";
  if (test_sheet_x_of_y()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: revision rows render... ";
  if (test_revision_rows_render()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: vector glyphs deterministic (golden)... ";
  if (test_glyphs_deterministic_golden()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: section labels + arrows on the parent view... ";
  if (test_section_labels()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: save/load round-trip... ";
  if (test_save_load_roundtrip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 8: undo/redo... ";
  if (test_undo_redo()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 9: landscape keeps the populated title block... ";
  if (test_landscape_keeps_title_block()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "ALL PASS\n";
    return 0;
  }
  std::cout << "SOME FAILED\n";
  return 1;
}
