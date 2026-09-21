// Drawing detail views test (ISO 128-3 enlarged features, R3).
//
// Pins the detail pipeline end-to-end plus the clip math:
//   - clip_projection_to_circle: lines clipped to their inside span,
//     circle arcs to consistent sub-arcs (endpoints AND angles),
//     ellipses kept only when fully inside, a circle containing the
//     window dropped, provenance (witnesses) copied verbatim
//   - drawing_view_create with kind "detail": validation (unknown
//     parent, radius ≤ 0, section parent, missing definition) throws
//     BEFORE the undo push
//   - the refresh derives the detail's projection from the parent's
//     cached projection (second pass) and degrades with the parent
//   - flatten: the detail's boundary circle + "A (2:1)" label, bounds
//     = the circle extent, the parent's marker circle + letter
//   - deleting the parent cascade-deletes the detail; deleting the
//     source body degrades both (stale, never blank); undo restores
//   - save/load keeps the detail definition; golden pins the sheet

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "core/document/document.h"
#include "core/drawing/drawing_detail_clip.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_sheet.h"
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
using polysmith::core::ProjectedEdgeRecord;
using polysmith::core::ProjectionResult;
using polysmith::core::SheetPrimitive;
using polysmith::core::SheetPrimitiveStream;
using polysmith::core::SheetText;
using polysmith::core::SourceEdgeWitness;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) < tolerance;
}

bool throws(const std::function<void()>& fn) {
  try {
    fn();
  } catch (const std::runtime_error&) {
    return true;
  }
  return false;
}

double quant(double v) { return std::round(v * 1e6) / 1e6; }

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

std::string dump_stream(const SheetPrimitiveStream& flat) {
  std::ostringstream out;
  out << "sheet " << flat.width_mm << "x" << flat.height_mm << "\n";
  for (const auto& p : flat.primitives) {
    if (p.purpose == "text_glyph") {
      continue;
    }
    out << p.purpose << " " << p.kind << " " << quant(p.p0[0]) << ","
        << quant(p.p0[1]) << " " << quant(p.p1[0]) << ","
        << quant(p.p1[1]);
    if (p.center.has_value()) {
      out << " c" << quant(p.center.value()[0]) << ","
          << quant(p.center.value()[1]);
    }
    if (p.radius.has_value()) {
      out << " r" << quant(p.radius.value());
    }
    if (!p.points.empty()) {
      out << " pts";
      for (const auto& point : p.points) {
        out << " " << quant(point[0]) << "," << quant(point[1]);
      }
    }
    out << "\n";
  }
  for (const auto& t : flat.texts) {
    out << "text " << t.purpose << " \"" << t.text << "\" "
        << quant(t.position[0]) << "," << quant(t.position[1]) << " h"
        << quant(t.height_mm) << (t.stale ? " stale" : "") << "\n";
  }
  return out.str();
}

// ── Hand-built records for the clip-math tests ─────────────────────

ProjectedEdgeRecord make_line(double x0, double y0, double x1, double y1,
                              const std::string& body = "body-1") {
  ProjectedEdgeRecord rec;
  rec.line_class = "visible";
  rec.curve_kind = "line";
  rec.p_start = {x0, y0};
  rec.p_end = {x1, y1};
  SourceEdgeWitness witness;
  witness.body_id = body;
  witness.curve_kind = "line";
  witness.start_point = {x0, y0, 0.0};
  witness.end_point = {x1, y1, 0.0};
  witness.length = std::hypot(x1 - x0, y1 - y0);
  rec.source = witness;
  return rec;
}

ProjectedEdgeRecord make_arc(double cx, double cy, double radius,
                             double start_angle, double end_angle,
                             const std::string& body = "body-1") {
  ProjectedEdgeRecord rec;
  rec.line_class = "visible";
  rec.curve_kind = "circle";
  rec.circle_center = {{cx, cy}};
  rec.circle_radius = radius;
  rec.start_angle = start_angle;
  rec.end_angle = end_angle;
  rec.p_start = {cx + radius * std::cos(start_angle),
                 cy + radius * std::sin(start_angle)};
  rec.p_end = {cx + radius * std::cos(end_angle),
               cy + radius * std::sin(end_angle)};
  SourceEdgeWitness witness;
  witness.body_id = body;
  witness.curve_kind = "circle";
  witness.center = {{cx, cy, 0.0}};
  witness.radius = radius;
  witness.axis = {{0.0, 0.0, 1.0}};
  witness.param_range = {start_angle, end_angle};
  rec.source = witness;
  return rec;
}

ProjectedEdgeRecord make_ellipse(double cx, double cy, double major,
                                 double minor, const std::string& body =
                                                     "body-1") {
  ProjectedEdgeRecord rec;
  rec.line_class = "visible";
  rec.curve_kind = "ellipse";
  rec.ellipse_center = {{cx, cy}};
  rec.ellipse_major_dir = {{1.0, 0.0}};
  rec.ellipse_major_radius = major;
  rec.ellipse_minor_radius = minor;
  rec.start_angle = 0.0;
  rec.end_angle = 2.0 * 3.141592653589793;
  rec.p_start = {cx + major, cy};
  rec.p_end = {cx + major, cy};
  SourceEdgeWitness witness;
  witness.body_id = body;
  rec.source = witness;
  return rec;
}

ProjectionResult make_projection(std::vector<ProjectedEdgeRecord> edges) {
  ProjectionResult result;
  result.edges = std::move(edges);
  result.source_revision = 1;
  return result;
}

// ── Test 1: line clipping ──────────────────────────────────────────

bool test_clip_line() {
  const auto clipped = polysmith::core::clip_projection_to_circle(
      make_projection({
          make_line(-10.0, 0.0, 10.0, 0.0),   // crosses the window
          make_line(20.0, 0.0, 30.0, 0.0),   // fully outside
          make_line(-2.0, 2.0, 2.0, 2.0),    // fully inside (chord)
      }),
      {0.0, 0.0}, 5.0);
  if (!expect(clipped.edges.size() == 2, "crossing + inside kept, outside "
                                         "dropped")) {
    return false;
  }
  // The crossing line keeps the inside span [−5, 5].
  const ProjectedEdgeRecord* span = nullptr;
  for (const auto& rec : clipped.edges) {
    if (near(rec.p_start[1], 0.0) && near(rec.p_start[0], -5.0)) {
      span = &rec;
    }
  }
  if (!expect(span != nullptr && near(span->p_end[0], 5.0, 1e-3) &&
                  near(span->p_start[0], -5.0, 1e-3),
              "crossing line clipped to the inside span")) {
    return false;
  }
  bool inside_chord_kept = false;
  for (const auto& rec : clipped.edges) {
    if (near(rec.p_start[1], 2.0) && near(rec.p_start[0], -2.0)) {
      inside_chord_kept = near(rec.p_end[0], 2.0);
    }
  }
  return expect(inside_chord_kept, "fully-inside line kept as-is");
}

// ── Test 2: circle-arc clipping ────────────────────────────────────

bool test_clip_circle_arc() {
  // A full circle of radius 8 centered at (0,0) clipped by a window
  // radius 5 at (6,0): the kept arc spans the inside angles; its
  // endpoints AND angles must be consistent (both on the window).
  const auto clipped = polysmith::core::clip_projection_to_circle(
      make_projection({make_arc(0.0, 0.0, 8.0, 0.0, 0.0)}), {6.0, 0.0},
      5.0);
  if (!expect(clipped.edges.size() == 1,
              "overlapping full circle kept as one arc")) {
    return false;
  }
  const auto& rec = clipped.edges[0];
  // The intersection points lie on both circles: |p − (0,0)| = 8 and
  // |p − (6,0)| = 5.  Law of cosines: the chord x-coordinate is
  // (8² − 5² + 6²) / (2·6) = 75/12 = 6.25 on the record circle.
  const double expected_x = (64.0 - 25.0 + 36.0) / 12.0;  // 6.25
  const double expected_y = std::sqrt(64.0 - expected_x * expected_x);
  const double p0_r = std::hypot(rec.p_start[0], rec.p_start[1]);
  const double p1_r = std::hypot(rec.p_end[0], rec.p_end[1]);
  if (!expect(near(p0_r, 8.0, 1e-3) && near(p1_r, 8.0, 1e-3) &&
                  near(std::abs(rec.p_start[0]), expected_x, 1e-3) &&
                  near(std::abs(rec.p_end[0]), expected_x, 1e-3),
              "kept arc endpoints lie on both circles")) {
    return false;
  }
  // Angles and endpoints agree (the flatten/SVG consistency contract).
  const double a0 = rec.start_angle;
  const double a1 = rec.end_angle;
  if (!expect(near(rec.p_start[0], 8.0 * std::cos(a0), 1e-3) &&
                  near(rec.p_start[1], 8.0 * std::sin(a0), 1e-3) &&
                  near(rec.p_end[0], 8.0 * std::cos(a1), 1e-3) &&
                  near(rec.p_end[1], 8.0 * std::sin(a1), 1e-3),
              "sub-arc angles recomputed with the endpoints")) {
    return false;
  }
  // The witness survives verbatim.
  const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
  if (!expect(witness != nullptr && witness->body_id == "body-1" &&
                  near(witness->radius.value(), 8.0),
              "provenance copied through the clip")) {
    return false;
  }
  // A circle fully inside → kept whole; one fully containing the
  // window → dropped; one outside → dropped.
  const auto inside = polysmith::core::clip_projection_to_circle(
      make_projection({make_arc(0.0, 0.0, 2.0, 0.0, 0.0)}), {0.0, 0.0},
      5.0);
  const auto containing = polysmith::core::clip_projection_to_circle(
      make_projection({make_arc(0.0, 0.0, 20.0, 0.0, 0.0)}), {0.0, 0.0},
      5.0);
  const auto outside = polysmith::core::clip_projection_to_circle(
      make_projection({make_arc(30.0, 0.0, 2.0, 0.0, 0.0)}), {0.0, 0.0},
      5.0);
  return expect(inside.edges.size() == 1 && containing.edges.empty() &&
                    outside.edges.empty(),
                "inside kept whole, containing + outside dropped");
}

// ── Test 3: ellipse heuristic ──────────────────────────────────────

bool test_clip_ellipse_heuristic() {
  const auto inside = polysmith::core::clip_projection_to_circle(
      make_projection({make_ellipse(0.0, 0.0, 2.0, 1.0)}), {0.0, 0.0},
      5.0);
  const auto crossing = polysmith::core::clip_projection_to_circle(
      make_projection({make_ellipse(0.0, 0.0, 8.0, 4.0)}), {0.0, 0.0},
      5.0);
  return expect(inside.edges.size() == 1 && crossing.edges.empty(),
                "inside ellipse kept, crossing ellipse dropped");
}

// ── Fixture: box 20x20x10 front view at sheet (30,40) ──────────────

struct DetailFixture {
  DocumentManager manager;
  DocumentState document;
  std::string body_id;
  std::string drawing_id;
  std::string sheet_id;
  std::string view_id;
};

DetailFixture make_fixture() {
  DetailFixture fixture;
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
  sheet.orientation = "portrait";
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

/// Creates the standard detail view used by the end-to-end tests:
/// circle center (10,5) r6 in the parent's view-mm (clips the box's
/// top and bottom edges), scale 2, placed at sheet (60,40).
DocumentState add_detail(DetailFixture& fixture,
                         const std::string& parent_view_id) {
  DrawingView detail;
  detail.kind = "detail";
  detail.source_body_ids = {fixture.body_id};
  detail.scale = 2.0;
  detail.sheet_position = {60.0, 40.0};
  detail.show_hidden = false;
  detail.detail = polysmith::core::DetailDefinition{
      parent_view_id, {10.0, 5.0}, 6.0, "A"};
  return fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, detail);
}

// ── Test 4: create + flatten (clipped content, boundary, marker) ───

bool test_detail_create_flatten() {
  DetailFixture fixture = make_fixture();
  DocumentState document = add_detail(fixture, fixture.view_id);
  const auto& views = document.drawing.drawings[0].views;
  if (!expect(views.size() == 2 && views[1].kind == "detail" &&
                  views[1].detail.has_value() &&
                  views[1].detail.value().label == "A",
              "detail view created with its definition")) {
    return false;
  }
  const std::string detail_id = views[1].view_id;
  // The refresh derived the clipped projection from the parent.
  const ProjectionResult* clipped =
      polysmith::core::drawing_runtime::cached_projection(document,
                                                          detail_id);
  if (!expect(clipped != nullptr && !clipped->stale && !clipped->edges.empty(),
              "clipped projection cached for the detail")) {
    return false;
  }
  // The circle center (10,5) r6 clips the box's top (y=10) and
  // bottom (y=0) edges — exactly two records, both horizontal spans.
  if (!expect(clipped->edges.size() == 2,
              "top + bottom edges survive the clip")) {
    return false;
  }
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet flattens")) {
    return false;
  }
  // Clipped content lands at the detail's scale 2 + position (60,40):
  // top edge (y=10): x∈[6.683,13.317] view → sheet x∈[73.37,86.63],
  // y=60; bottom edge → y=40.
  bool saw_top = false;
  bool saw_bottom = false;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "view_geometry" || p.kind != "line") {
      continue;
    }
    if (near(p.p0[1], 60.0) && near(p.p1[1], 60.0) &&
        near(p.p0[0], 73.3667, 0.01) && near(p.p1[0], 86.6333, 0.01)) {
      saw_top = true;
    }
    if (near(p.p0[1], 40.0) && near(p.p1[1], 40.0)) {
      saw_bottom = true;
    }
  }
  if (!expect(saw_top && saw_bottom,
              "clipped spans drawn at the detail scale/position")) {
    return false;
  }
  // The detail's own boundary circle + "A (2:1)" label.
  bool saw_boundary = false;
  for (const auto& p : flat->primitives) {
    if (p.purpose == "detail_boundary" && p.kind == "circle_arc" &&
        p.center.has_value() && near(p.center.value()[0], 80.0) &&
        near(p.center.value()[1], 50.0) && near(p.radius.value(), 12.0)) {
      saw_boundary = true;
    }
  }
  if (!expect(saw_boundary, "detail boundary circle at scale 2")) {
    return false;
  }
  const SheetText* detail_label = nullptr;
  const SheetText* parent_letter = nullptr;
  for (const auto& t : flat->texts) {
    if (t.purpose == "detail_label" && t.text == "A (2:1)") {
      detail_label = &t;
    }
    if (t.purpose == "detail_label" && t.text == "A") {
      parent_letter = &t;
    }
  }
  if (!expect(detail_label != nullptr && parent_letter != nullptr,
              "detail label + parent marker letter emitted")) {
    return false;
  }
  // Bounds = the circle extent (the viewport hit-test contract).
  const polysmith::core::SheetViewBounds* bounds = nullptr;
  for (const auto& b : flat->views) {
    if (b.view_id == detail_id) {
      bounds = &b;
    }
  }
  return expect(bounds != nullptr &&
                    near(bounds->min[0], 68.0) && near(bounds->min[1], 38.0) &&
                    near(bounds->max[0], 92.0) && near(bounds->max[1], 62.0) &&
                    bounds->label == "A (2:1)",
                "bounds = the circle extent, label synthesized");
}

// ── Test 5: validation throws before mutating ─────────────────────

bool test_detail_validation() {
  DetailFixture fixture = make_fixture();
  // Unknown parent.
  if (!expect(throws([&]() {
        DrawingView view;
        view.kind = "detail";
        view.source_body_ids = {fixture.body_id};
        view.detail = polysmith::core::DetailDefinition{
            "no-such-view", {10.0, 5.0}, 6.0, "A"};
        fixture.manager.drawing_view_create(
            fixture.drawing_id, fixture.sheet_id, view);
      }),
              "unknown parent throws")) {
    return false;
  }
  // Radius ≤ 0.
  if (!expect(throws([&]() {
        DrawingView view;
        view.kind = "detail";
        view.source_body_ids = {fixture.body_id};
        view.detail = polysmith::core::DetailDefinition{
            fixture.view_id, {10.0, 5.0}, 0.0, "A"};
        fixture.manager.drawing_view_create(
            fixture.drawing_id, fixture.sheet_id, view);
      }),
              "non-positive radius throws")) {
    return false;
  }
  // Missing definition.
  if (!expect(throws([&]() {
        DrawingView view;
        view.kind = "detail";
        view.source_body_ids = {fixture.body_id};
        fixture.manager.drawing_view_create(
            fixture.drawing_id, fixture.sheet_id, view);
      }),
              "missing detail definition throws")) {
    return false;
  }
  // Section parent.
  DrawingView section;
  section.kind = "section";
  section.source_body_ids = {fixture.body_id};
  section.sheet_position = {60.0, 40.0};
  section.section = polysmith::core::SectionDefinition{
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, true, "A", 45.0, 3.0, std::nullopt};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, section);
  const std::string section_id =
      fixture.document.drawing.drawings[0].views[1].view_id;
  if (!expect(throws([&]() {
        DrawingView view;
        view.kind = "detail";
        view.source_body_ids = {fixture.body_id};
        view.detail = polysmith::core::DetailDefinition{
            section_id, {10.0, 5.0}, 6.0, "A"};
        fixture.manager.drawing_view_create(
            fixture.drawing_id, fixture.sheet_id, view);
      }),
              "section parent throws")) {
    return false;
  }
  return expect(fixture.manager.get_document().value()
                        .drawing.drawings[0]
                        .views.size() == 2,
                "failed creates mutated nothing (only the section landed)");
}

// ── Test 6: deleting the parent cascades ───────────────────────────

bool test_detail_cascade_on_parent_delete() {
  DetailFixture fixture = make_fixture();
  DocumentState document = add_detail(fixture, fixture.view_id);
  const std::string detail_id =
      document.drawing.drawings[0].views[1].view_id;
  document = fixture.manager.drawing_view_delete(
      fixture.drawing_id, fixture.view_id);
  const auto& drawing = document.drawing.drawings[0];
  if (!expect(drawing.views.empty(), "parent + detail both removed")) {
    return false;
  }
  bool ids_clean = true;
  for (const auto& sheet : drawing.sheets) {
    if (std::find(sheet.view_ids.begin(), sheet.view_ids.end(), detail_id) !=
        sheet.view_ids.end()) {
      ids_clean = false;
    }
  }
  return expect(ids_clean, "sheet ordering lists cleaned");
}

// ── Test 7: source body deleted → both degrade (never blank) ───────

bool test_detail_stale_on_body_delete() {
  DetailFixture fixture = make_fixture();
  DocumentState document = add_detail(fixture, fixture.view_id);
  const std::string detail_id =
      document.drawing.drawings[0].views[1].view_id;
  document = fixture.manager.delete_feature(fixture.body_id);
  const auto& views = document.drawing.drawings[0].views;
  const DrawingView* detail = nullptr;
  for (const auto& v : views) {
    if (v.view_id == detail_id) {
      detail = &v;
    }
  }
  if (!expect(detail != nullptr && detail->broken_ref.has_value() &&
                  !detail->warning.empty(),
              "detail degrades with the broken parent")) {
    return false;
  }
  const ProjectionResult* stale = polysmith::core::drawing_runtime::
      cached_projection(document, detail_id);
  if (!expect(stale != nullptr && stale->stale,
              "last-known content kept, marked stale")) {
    return false;
  }
  // Undo restores both.
  document = fixture.manager.undo();
  const ProjectionResult* restored = polysmith::core::drawing_runtime::
      cached_projection(document, detail_id);
  const DrawingView* restored_view = nullptr;
  for (const auto& v : document.drawing.drawings[0].views) {
    if (v.view_id == detail_id) {
      restored_view = &v;
    }
  }
  return expect(restored != nullptr && !restored->stale &&
                    restored_view != nullptr &&
                    !restored_view->broken_ref.has_value(),
                "undo restores the detail");
}

// ── Test 8: save/load round-trip ───────────────────────────────────

bool test_detail_save_load() {
  DetailFixture fixture = make_fixture();
  DocumentState document = add_detail(fixture, fixture.view_id);
  const auto payload = polysmith::protocol::to_payload(document, true);
  const DocumentState restored =
      polysmith::protocol::document_from_payload(payload);
  const auto& views = restored.drawing.drawings[0].views;
  if (!expect(views.size() == 2 && views[1].kind == "detail" &&
                  views[1].detail.has_value(),
              "detail survives the round-trip")) {
    return false;
  }
  const auto& detail = views[1].detail.value();
  return expect(detail.parent_view_id == fixture.view_id &&
                    near(detail.center[0], 10.0) &&
                    near(detail.center[1], 5.0) && near(detail.radius, 6.0) &&
                    detail.label == "A" && near(views[1].scale, 2.0),
                "definition fields survive the round-trip");
}

// ── Test 9: golden ─────────────────────────────────────────────────

bool test_detail_golden() {
  DetailFixture fixture = make_fixture();
  DocumentState document = add_detail(fixture, fixture.view_id);
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet flattens")) {
    return false;
  }
  return check_golden("drawing_detail_box", dump_stream(flat.value()));
}

}  // namespace

int main() {
  int passed = 0;
  const int total = 9;
  struct Test {
    const char* name;
    bool (*fn)();
  };
  const Test tests[] = {
      {"clip_line", test_clip_line},
      {"clip_circle_arc", test_clip_circle_arc},
      {"clip_ellipse_heuristic", test_clip_ellipse_heuristic},
      {"detail_create_flatten", test_detail_create_flatten},
      {"detail_validation", test_detail_validation},
      {"detail_cascade_on_parent_delete", test_detail_cascade_on_parent_delete},
      {"detail_stale_on_body_delete", test_detail_stale_on_body_delete},
      {"detail_save_load", test_detail_save_load},
      {"detail_golden", test_detail_golden},
  };
  for (const auto& test : tests) {
    if (test.fn()) {
      ++passed;
    } else {
      std::cerr << "FAILED TEST: " << test.name << "\n";
    }
  }
  std::cout << passed << "/" << total << " drawing detail tests passed\n";
  return passed == total ? 0 : 1;
}
