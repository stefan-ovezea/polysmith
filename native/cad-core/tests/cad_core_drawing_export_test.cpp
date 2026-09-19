// Drawing export test (P8: SVG + DXF).
//
// Pins the sheet export backends end-to-end:
//   - the SVG backend is a pure stream → SVG writer: mm viewBox, the
//     y-axis flip to SVG's screen convention, arc sweep flags, the
//     tessellated ellipse, filled polygons, glyphs rendered / text
//     records skipped — golden
//   - the DXF backend round-trips through the libdxfrw READER (the
//     same parser real CAD tools use): entity counts per layer,
//     coordinates within 1e-4, the title block as a BLOCK + INSERT,
//     text as real DRW_Text, glyphs skipped
//   - export_drawing_sheet dispatches formats and throws on unknown
//     ids/formats without mutating the document

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "drw_entities.h"
#include "drw_interface.h"
#include "libdxfrw.h"

#include "core/document/document.h"
#include "core/drawing/drawing_sheet.h"
#include "core/drawing/export/drawing_export.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"

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

bool near(double a, double b, double tolerance = 1e-4) {
  return std::abs(a - b) < tolerance;
}

std::string temp_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() / name).string();
}

std::string read_file(const std::string& path) {
  std::ifstream in(path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
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

// ── Hand-built stream for the SVG backend golden ───────────────────

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

  SheetPrimitive arc;
  arc.kind = "circle_arc";
  arc.purpose = "view_geometry";
  arc.style = {"continuous", 0.5};
  arc.center = {{50.0, 50.0}};
  arc.radius = 10.0;
  arc.start_angle = 0.0;
  arc.end_angle = 3.14159265358979323846 / 2.0;
  arc.p0 = {60.0, 50.0};
  arc.p1 = {50.0, 60.0};
  stream.primitives.push_back(arc);

  SheetPrimitive ellipse;
  ellipse.kind = "ellipse_arc";
  ellipse.purpose = "view_geometry";
  ellipse.style = {"continuous", 0.5};
  ellipse.center = {{100.0, 100.0}};
  ellipse.major_dir = {{1.0, 0.0}};
  ellipse.major_radius = 10.0;
  ellipse.minor_radius = 5.0;
  ellipse.start_angle = 0.0;
  ellipse.end_angle = 3.14159265358979323846;
  stream.primitives.push_back(ellipse);

  SheetPrimitive poly;
  poly.kind = "filled_poly";
  poly.purpose = "dimension";
  poly.style = {"continuous", 0.25};
  poly.points = {{5.0, 5.0}, {15.0, 5.0}, {10.0, 12.0}};
  stream.primitives.push_back(poly);

  // A text record + one of its glyph segments (the flatten emits the
  // full glyph set; one segment suffices to pin the backend path).
  SheetText text;
  text.text = "20";
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

// ── Full fixture: box + front view + title block + dimension ───────

struct ExportFixture {
  DocumentManager manager;
  polysmith::core::DocumentState document;
  std::string drawing_id;
  std::string sheet_id;
  std::string view_id;
  std::string body_id;
};

ExportFixture make_fixture() {
  ExportFixture fixture;
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

// ── DXF parse-back: a counting read-side interface ─────────────────

struct DxfCounts {
  int lines = 0;
  int circles = 0;
  int arcs = 0;
  int ellipses = 0;
  int solids = 0;
  int texts = 0;
  int inserts = 0;
  std::vector<std::pair<std::string, std::string>> insert_names;
  std::vector<std::pair<std::string, double>> frame_corners;  // layer, x
  std::vector<std::pair<std::string, std::array<double, 2>>>
      visible_line_starts;
  int block_lines = 0;
  int block_texts = 0;
  bool in_block = false;
};

class CountingDxfInterface : public DRW_Interface {
 public:
  explicit CountingDxfInterface(DxfCounts* counts) : counts_(counts) {}

  // Write-side: unused, no-ops.
  void writeHeader(DRW_Header&) override {}
  void writeBlocks() override {}
  void writeBlockRecords() override {}
  void writeEntities() override {}
  void writeLTypes() override {}
  void writeLayers() override {}
  void writeTextstyles() override {}
  void writeVports() override {}
  void writeDimstyles() override {}
  void writeAppId() override {}

  // Read-side: count what the writer produced.
  void addHeader(const DRW_Header*) override {}
  void addLType(const DRW_LType&) override {}
  void addLayer(const DRW_Layer&) override {}
  void addDimStyle(const DRW_Dimstyle&) override {}
  void addVport(const DRW_Vport&) override {}
  void addTextStyle(const DRW_Textstyle&) override {}
  void addAppId(const DRW_AppId&) override {}
  // The block boundary signal on the DXF read path is addBlock →
  // entities → endBlock (setBlock is a DWG-conversion callback the
  // ASCII reader never fires).
  void addBlock(const DRW_Block&) override { counts_->in_block = true; }
  void setBlock(const int) override {}
  void endBlock() override { counts_->in_block = false; }
  void addPoint(const DRW_Point&) override {}
  void addLine(const DRW_Line& line) override {
    if (counts_->in_block) {
      ++counts_->block_lines;
      return;
    }
    ++counts_->lines;
    if (line.layer == "FRAME") {
      counts_->frame_corners.push_back({line.layer, line.basePoint.x});
    }
    if (line.layer == "VISIBLE") {
      counts_->visible_line_starts.push_back(
          {line.layer, {line.basePoint.x, line.basePoint.y}});
    }
  }
  void addRay(const DRW_Ray&) override {}
  void addXline(const DRW_Xline&) override {}
  void addArc(const DRW_Arc&) override {
    if (!counts_->in_block) {
      ++counts_->arcs;
    }
  }
  void addCircle(const DRW_Circle&) override {
    if (!counts_->in_block) {
      ++counts_->circles;
    }
  }
  void addEllipse(const DRW_Ellipse&) override {
    if (!counts_->in_block) {
      ++counts_->ellipses;
    }
  }
  void addLWPolyline(const DRW_LWPolyline&) override {}
  void addPolyline(const DRW_Polyline&) override {}
  void addSpline(const DRW_Spline*) override {}
  void addKnot(const DRW_Entity&) override {}
  void addInsert(const DRW_Insert& insert) override {
    ++counts_->inserts;
    counts_->insert_names.push_back({insert.name, insert.layer});
  }
  void addTrace(const DRW_Trace&) override {}
  void add3dFace(const DRW_3Dface&) override {}
  void addSolid(const DRW_Solid&) override { ++counts_->solids; }
  void addMText(const DRW_MText&) override {}
  void addText(const DRW_Text&) override {
    if (counts_->in_block) {
      ++counts_->block_texts;
    } else {
      ++counts_->texts;
    }
  }
  void addDimAlign(const DRW_DimAligned*) override {}
  void addDimLinear(const DRW_DimLinear*) override {}
  void addDimRadial(const DRW_DimRadial*) override {}
  void addDimDiametric(const DRW_DimDiametric*) override {}
  void addDimAngular(const DRW_DimAngular*) override {}
  void addDimAngular3P(const DRW_DimAngular3p*) override {}
  void addDimOrdinate(const DRW_DimOrdinate*) override {}
  void addLeader(const DRW_Leader*) override {}
  void addHatch(const DRW_Hatch*) override {}
  void addViewport(const DRW_Viewport&) override {}
  void addImage(const DRW_Image*) override {}
  void linkImage(const DRW_ImageDef*) override {}
  void addComment(const char*) override {}

 private:
  DxfCounts* counts_;
};

DxfCounts read_dxf_counts(const std::string& path) {
  DxfCounts counts;
  CountingDxfInterface iface(&counts);
  dxfRW dxf(path.c_str());
  if (!dxf.read(&iface, /*ext=*/true)) {
    throw std::runtime_error("parse-back failed: " + path);
  }
  return counts;
}

// ── Tests ──────────────────────────────────────────────────────────

bool test_svg_golden() {
  const std::string path = temp_path("drawing_export_tiny.svg");
  const auto result =
      polysmith::core::export_sheet_as_svg(tiny_stream(), path);
  if (!expect(result.format == "svg" && result.file_path == path,
              "svg export result")) {
    return false;
  }
  if (!expect(std::filesystem::exists(path), "svg file exists")) {
    return false;
  }
  const std::string text = read_file(path);
  if (!expect(text.find("viewBox=\"0 0 210.000 297.000\"") !=
                  std::string::npos,
              "mm viewBox")) {
    return false;
  }
  // The y-flip: sheet (10,20) → SVG y 297-20 = 277.
  if (!expect(text.find("M 10.000,277.000 L 30.000,257.000") !=
                  std::string::npos,
              "line y-flipped")) {
    return false;
  }
  // The text record stays DATA (no <text> element), the glyph line
  // renders as a path.
  if (!expect(text.find("<text") == std::string::npos,
              "text records skipped in SVG")) {
    return false;
  }
  return check_golden("drawing_svg_tiny", text);
}

bool test_dxf_parse_back() {
  ExportFixture fixture = make_fixture();
  const std::string path = temp_path("drawing_export_full.dxf");
  const auto result = polysmith::core::export_drawing_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id, "dxf", path);
  if (!expect(result.format == "dxf" && std::filesystem::exists(path),
              "dxf export result + file")) {
    return false;
  }

  const DxfCounts counts = read_dxf_counts(path);

  // Direct entities: 4 visible box edges, 4 frame lines, 4 centring
  // marks + 16 grid ticks on FURNITURE, the dimension (3 lines + 2
  // arrow solids) on ANNOTATION, the dimension text on TEXT.  The
  // title block travels in the BLOCK.
  if (!expect(counts.lines >= 4 + 4 + 20 + 3,
              "direct line count (frame + furniture + view + dim)")) {
    return false;
  }
  if (!expect(counts.solids == 2, "2 arrowhead solids")) {
    return false;
  }
  if (!expect(counts.texts == 1, "1 direct text (the dimension value)")) {
    std::cerr << "  DEBUG texts=" << counts.texts
              << " block_texts=" << counts.block_texts
              << " block_lines=" << counts.block_lines
              << " lines=" << counts.lines << " inserts=" << counts.inserts
              << "\n";
    return false;
  }
  if (!expect(counts.inserts == 1 && counts.insert_names.size() == 1 &&
                  counts.insert_names[0].first == "POLYSMITH_TITLE_BLOCK",
              "title block as one BLOCK + INSERT")) {
    return false;
  }
  if (!expect(counts.block_lines > 0 && counts.block_texts > 0,
              "block carries the title block lines + texts")) {
    return false;
  }
  // Coordinates: the frame's bottom-left corner (20,10) and the box's
  // front bottom edge (30,40)-(50,40).
  bool frame_corner = false;
  for (const auto& corner : counts.frame_corners) {
    if (near(corner.second, 20.0)) {
      frame_corner = true;
    }
  }
  if (!expect(frame_corner, "frame corner x=20 present")) {
    return false;
  }
  bool box_edge = false;
  for (const auto& start : counts.visible_line_starts) {
    if (near(start.second[0], 30.0) && near(start.second[1], 40.0)) {
      box_edge = true;
    }
  }
  return expect(box_edge, "box front bottom edge at (30,40) present");
}

bool test_svg_end_to_end() {
  ExportFixture fixture = make_fixture();
  const std::string path = temp_path("drawing_export_full.svg");
  const auto result = polysmith::core::export_drawing_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id, "svg", path);
  if (!expect(result.format == "svg" && std::filesystem::exists(path),
              "svg end-to-end result + file")) {
    return false;
  }
  const std::string text = read_file(path);
  return expect(text.find("<?xml") == 0 && text.find("</svg>") !=
                                               std::string::npos,
                "svg end-to-end well-formed envelope");
}

bool test_errors() {
  ExportFixture fixture = make_fixture();
  const std::string path = temp_path("drawing_export_err.svg");

  bool unknown_format = false;
  try {
    polysmith::core::export_drawing_sheet(fixture.document,
                                          fixture.drawing_id,
                                          fixture.sheet_id, "pdf", path);
  } catch (const std::runtime_error& error) {
    unknown_format = std::string(error.what()).find("Unknown drawing export")
                     != std::string::npos;
  }
  if (!expect(unknown_format, "unknown format throws")) {
    return false;
  }

  bool unknown_drawing = false;
  try {
    polysmith::core::export_drawing_sheet(fixture.document, "nope",
                                          fixture.sheet_id, "svg", path);
  } catch (const std::runtime_error& error) {
    unknown_drawing = std::string(error.what()).find(
                          "Unknown drawing or sheet") != std::string::npos;
  }
  if (!expect(unknown_drawing, "unknown drawing throws")) {
    return false;
  }

  bool empty_path = false;
  try {
    polysmith::core::export_drawing_sheet(fixture.document,
                                          fixture.drawing_id,
                                          fixture.sheet_id, "svg", "");
  } catch (const std::runtime_error& error) {
    empty_path = std::string(error.what()).find("path is empty") !=
                 std::string::npos;
  }
  if (!expect(empty_path, "empty path throws")) {
    return false;
  }

  // Exports never mutate: the revision must be unchanged.
  const int revision_before = fixture.document.revision;
  const auto result = polysmith::core::export_drawing_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id, "svg", path);
  return expect(result.format == "svg" &&
                    fixture.document.revision == revision_before,
                "export never bumps the revision");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_export_test\n";
  std::cout << "  Test 1: SVG backend golden (y-flip, arcs, glyphs)... ";
  if (test_svg_golden()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: DXF parse-back (layers, block, text)... ";
  if (test_dxf_parse_back()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: SVG end-to-end... ";
  if (test_svg_end_to_end()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: error paths + non-mutation... ";
  if (test_errors()) {
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
