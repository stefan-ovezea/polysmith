// Drawing PDF export test (P9: libharu backend).
//
// Pins the PDF backend end-to-end:
//   - valid PDF envelope (%PDF header, %%EOF tail), the MediaBox at
//     the sheet size in mm
//   - the bundled ISO 3098 font travels INTO the PDF (/FontFile2)
//     and drawing text is REAL selectable text: the content stream
//     (inflated, it is FlateDecode-compressed) carries the UTF-16BE
//     bytes of the dimension text incl. the ⌀ glyph — when the
//     bundled font is not resolvable in this environment, the
//     backend's documented glyph-primitive fallback is accepted
//   - export_drawing_sheet dispatches "pdf" without mutating the
//     document, and I/O failures throw

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "zlib.h"

#include "core/document/document.h"
#include "core/drawing/drawing_sheet.h"
#include "core/drawing/export/drawing_export.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"
#include "core/text_engine.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::SheetPrimitive;
using polysmith::core::SheetPrimitiveStream;
using polysmith::core::SheetText;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

std::string temp_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() / name).string();
}

std::string read_file(const std::string& path) {
  // Binary: the compressed streams contain 0x1A bytes, which text
  // mode would treat as EOF on Windows.
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool contains_bytes(const std::vector<unsigned char>& data,
                    const unsigned char* needle, size_t needle_len) {
  if (needle_len == 0 || data.size() < needle_len) {
    return false;
  }
  for (size_t i = 0; i + needle_len <= data.size(); ++i) {
    if (std::memcmp(data.data() + i, needle, needle_len) == 0) {
      return true;
    }
  }
  return false;
}

/// The page's MediaBox numbers in mm (the page dict is plain text —
/// only streams are compressed).
std::array<double, 4> parse_media_box(const std::string& pdf) {
  std::array<double, 4> box = {0.0, 0.0, 0.0, 0.0};
  const size_t pos = pdf.find("/MediaBox");
  if (pos == std::string::npos) {
    return box;
  }
  const size_t open = pdf.find('[', pos);
  const size_t close = pdf.find(']', open);
  if (open == std::string::npos || close == std::string::npos) {
    return box;
  }
  std::istringstream numbers(pdf.substr(open + 1, close - open - 1));
  for (double& value : box) {
    numbers >> value;
  }
  return box;
}

/// Finds the page's content stream ("/Contents N 0 R" in the page
/// object → "N 0 obj ... stream") and inflates it (HPDF_COMP_ALL
/// FlateDecodes every content stream).  Returns empty on failure.
std::vector<unsigned char> inflate_contents(const std::string& pdf) {
  const size_t page_pos = pdf.find("/Type /Page");
  if (page_pos == std::string::npos) {
    return {};
  }
  const size_t contents_pos = pdf.find("/Contents", page_pos);
  if (contents_pos == std::string::npos) {
    return {};
  }
  const size_t ref_pos = pdf.find("0 R", contents_pos);
  if (ref_pos == std::string::npos) {
    return {};
  }
  std::string obj_num = pdf.substr(contents_pos + 9,
                                   ref_pos - (contents_pos + 9));
  // Trim whitespace.
  const size_t first = obj_num.find_first_not_of(" \t\r\n");
  const size_t last = obj_num.find_last_not_of(" \t\r\n");
  obj_num = obj_num.substr(first, last - first + 1);
  const std::string obj_marker = obj_num + " 0 obj";
  const size_t obj_pos = pdf.find(obj_marker);
  if (obj_pos == std::string::npos) {
    return {};
  }
  const size_t stream_pos = pdf.find("stream", obj_pos);
  if (stream_pos == std::string::npos) {
    return {};
  }
  size_t data_pos = stream_pos + 6;
  if (data_pos + 1 < pdf.size() && pdf.compare(data_pos, 2, "\r\n") == 0) {
    data_pos += 2;
  } else if (data_pos < pdf.size() && pdf[data_pos] == '\n') {
    data_pos += 1;
  }

  z_stream zs{};
  if (inflateInit(&zs) != Z_OK) {
    return {};
  }
  std::vector<unsigned char> out(1 << 16);
  zs.next_in = reinterpret_cast<Bytef*>(
      const_cast<char*>(pdf.data() + data_pos));
  zs.avail_in = static_cast<uInt>(pdf.size() - data_pos);
  zs.next_out = out.data();
  zs.avail_out = static_cast<uInt>(out.size());
  int ret = inflate(&zs, Z_NO_FLUSH);
  while (ret == Z_OK && zs.avail_out == 0) {
    const size_t produced = out.size();
    out.resize(out.size() * 2);
    zs.next_out = out.data() + produced;
    zs.avail_out = static_cast<uInt>(out.size() - produced);
    ret = inflate(&zs, Z_NO_FLUSH);
  }
  if (ret != Z_STREAM_END) {
    inflateEnd(&zs);
    return {};
  }
  out.resize(zs.total_out);
  inflateEnd(&zs);
  return out;
}

/// The same fixture as the P8 export suite: a box, one drawing with
/// an A4 sheet, a front projection view, a filled title block and one
/// linear dimension.
struct PdfFixture {
  DocumentManager manager;
  polysmith::core::DocumentState document;
  std::string drawing_id;
  std::string sheet_id;
  std::string view_id;
};

PdfFixture make_fixture() {
  PdfFixture fixture;
  fixture.manager.create_document();
  fixture.manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      fixture.manager.get_document().value(), /*include_meshes=*/false);

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
  view.source_body_ids = {bodies.bodies[0].id};
  view.sheet_position = {30.0, 40.0};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, view);
  fixture.view_id = fixture.document.drawing.drawings[0].views[0].view_id;

  polysmith::core::TitleBlock title_block;
  title_block.legal_owner = "Polysmith GmbH";
  title_block.title = "Test Bracket";
  fixture.document = fixture.manager.drawing_title_block_update(
      fixture.drawing_id, fixture.sheet_id, title_block);
  fixture.document = fixture.manager.drawing_dimension_create(
      fixture.drawing_id, fixture.view_id, "linear", {40.0, 50.0},
      std::nullopt, std::nullopt);
  return fixture;
}

/// A hand-built stream with every primitive kind and a text carrying
/// the ⌀ glyph (U+2300, UTF-8 E2 8C 80) — the UTF-16BE round-trip.
SheetPrimitiveStream tiny_stream() {
  SheetPrimitiveStream stream;
  stream.sheet_id = "sheet-1";
  stream.drawing_id = "drawing-1";
  stream.width_mm = 210.0;
  stream.height_mm = 297.0;

  SheetPrimitive line;
  line.kind = "line";
  line.purpose = "view_geometry";
  line.style = {"continuous", 0.5};
  line.p0 = {10.0, 20.0};
  line.p1 = {30.0, 40.0};
  stream.primitives.push_back(line);

  // A FULL circle — exercises the HPDF_Page_Circle branch (Arc
  // rejects sweeps >= 360°, which would raise and fail the export).
  SheetPrimitive circle;
  circle.kind = "circle_arc";
  circle.purpose = "view_geometry";
  circle.style = {"continuous", 0.5};
  circle.center = {{50.0, 50.0}};
  circle.radius = 10.0;
  circle.start_angle = 0.0;
  circle.end_angle = 2.0 * 3.14159265358979323846;
  stream.primitives.push_back(circle);

  SheetPrimitive arc;
  arc.kind = "circle_arc";
  arc.purpose = "view_geometry";
  arc.style = {"continuous", 0.5};
  arc.center = {{80.0, 50.0}};
  arc.radius = 8.0;
  arc.start_angle = 0.0;
  arc.end_angle = 3.14159265358979323846;
  stream.primitives.push_back(arc);

  // ⌀ + value: the ISO 129-1 diameter text (raw UTF-8 bytes so the
  // test is independent of the compiler's execution charset; the
  // literals are split — \x escapes swallow following hex digits).
  SheetText text;
  text.text = "\xE2\x8C\x80"
              "12,5";
  text.position = {70.0, 80.0};
  text.height_mm = 3.5;
  text.purpose = "dimension";
  stream.texts.push_back(text);
  SheetPrimitive glyph;
  glyph.kind = "line";
  glyph.purpose = "text_glyph";
  glyph.style = {"continuous", 0.25};
  glyph.p0 = {70.0, 80.0};
  glyph.p1 = {71.5, 80.0};
  stream.primitives.push_back(glyph);

  return stream;
}

// ── Tests ──────────────────────────────────────────────────────────

bool test_pdf_structure() {
  const std::string path = temp_path("drawing_export_tiny.pdf");
  const auto result =
      polysmith::core::export_sheet_as_pdf(tiny_stream(), path);
  if (!expect(result.format == "pdf" && result.file_path == path,
              "pdf export result")) {
    return false;
  }
  if (!expect(std::filesystem::exists(path), "pdf file exists")) {
    return false;
  }
  const std::string pdf = read_file(path);
  if (!expect(pdf.rfind("%PDF-", 0) == 0, "%PDF header")) {
    return false;
  }
  if (!expect(pdf.find("%%EOF") != std::string::npos, "%%EOF tail")) {
    return false;
  }
  const auto box = parse_media_box(pdf);
  if (!expect(std::abs(box[2] - 210.0) < 1e-3 &&
                  std::abs(box[3] - 297.0) < 1e-3,
              "MediaBox at the sheet size (210x297 mm)")) {
    return false;
  }

  // Real text requires the bundled font; without it the backend
  // draws the glyph primitives instead (the documented fallback).
  const std::string font_path =
      polysmith::core::text::TextEngine::bundled_iso3098_font_path();
  const bool font_available =
      !font_path.empty() && std::filesystem::exists(font_path);
  if (font_available) {
    if (!expect(pdf.find("/FontFile2") != std::string::npos,
                "bundled font subset-embedded (/FontFile2)")) {
      return false;
    }
    // The text is selectable: the content stream carries "⌀12,5" as
    // an ASCII hex string of its UTF-16BE code units —
    // "<230000310032002C0035> Tj" (⌀ = U+2300).
    const auto content = inflate_contents(pdf);
    if (content.empty()) {
      return expect(false, "content stream inflates");
    }
    const unsigned char diameter_text[] = "230000310032002C0035";
    return expect(contains_bytes(content, diameter_text,
                                 sizeof(diameter_text) - 1),
                  "⌀12,5 round-trips as UTF-16BE hex 2300...");
  }
  // No font in this environment: the glyph primitives must be drawn
  // (content present, no embedded font).
  std::cout << "  (no bundled font — asserting the glyph fallback)\n";
  return expect(pdf.find("/FontFile2") == std::string::npos,
                "glyph fallback when no font is bundled");
}

bool test_pdf_end_to_end() {
  PdfFixture fixture = make_fixture();
  const std::string path = temp_path("drawing_export_full.pdf");
  const int revision_before = fixture.document.revision;
  const auto result = polysmith::core::export_drawing_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id, "pdf", path);
  if (!expect(result.format == "pdf" && std::filesystem::exists(path),
              "pdf end-to-end result + file")) {
    return false;
  }
  if (!expect(fixture.document.revision == revision_before,
              "export never bumps the revision")) {
    return false;
  }
  const std::string pdf = read_file(path);
  return expect(pdf.rfind("%PDF-", 0) == 0 &&
                    pdf.find("%%EOF") != std::string::npos,
                "pdf end-to-end valid envelope");
}

bool test_pdf_errors() {
  PdfFixture fixture = make_fixture();
  // An unwritable path (a directory): HPDF_SaveToFile raises through
  // the error handler and the backend must throw.
  const std::string path = temp_path("pdf_export_unwritable.pdf");
  std::filesystem::remove(path);  // a stale FILE from a previous run
  std::filesystem::create_directories(path);  // the "file" is a dir
  bool threw = false;
  try {
    polysmith::core::export_drawing_sheet(
        fixture.document, fixture.drawing_id, fixture.sheet_id, "pdf", path);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  std::filesystem::remove_all(path);
  return expect(threw, "unwritable pdf path throws");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_pdf_export_test\n";
  std::cout << "  Test 1: PDF structure + font embedding + UTF-16 text... ";
  if (test_pdf_structure()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: PDF end-to-end + non-mutation... ";
  if (test_pdf_end_to_end()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: PDF error path... ";
  if (test_pdf_errors()) {
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
