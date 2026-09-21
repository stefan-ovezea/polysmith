// Drawing annotation test (GEOMETRY / SYMBOLS / ANNOTATE tabs, R4+R5).
//
// Pins the generic non-dimension annotation pipeline end-to-end with
// leader_text as the pilot kind:
//   - drawing_annotation_create resolves the pick against the view's
//     CURRENT projection, mints the witness + attach_param (a param
//     fraction, never a stored coordinate), and computes text_offset
//     from the placement click
//   - kind validation throws BEFORE the undo push (unknown kinds,
//     center_mark on a line, edge_extension on a circle)
//   - flatten emits the leader line + arrowhead + annotation text
//     (annotation_id set) through the attachment runtime cache
//   - cosmetic update (text_offset) moves the text and re-aims the
//     leader; delete + undo/redo work; save/load keeps attach_param
//   - delete the source body → dependency_broken + warning + the
//     last-known placement held (marked stale), never blank
//   - golden pins the flattened emission

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "core/document/document.h"
#include "core/drawing/drawing_annotation_geometry.h"
#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_sheet.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::Annotation;
using polysmith::core::AnnotationGraphics;
using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::ProjectedEdgeRecord;
using polysmith::core::ResolvedAttachment;
using polysmith::core::SourceEdgeWitness;
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

int count_purpose(const SheetPrimitiveStream& stream,
                  const std::string& purpose) {
  return static_cast<int>(std::count_if(
      stream.primitives.begin(), stream.primitives.end(),
      [&](const SheetPrimitive& p) { return p.purpose == purpose; }));
}

const SheetText* find_text(const SheetPrimitiveStream& stream,
                           const std::string& purpose) {
  for (const auto& t : stream.texts) {
    if (t.purpose == purpose) {
      return &t;
    }
  }
  return nullptr;
}

// ── Fixture: document with a 20x20x10 box, a drawing, and a front
//    view at sheet [30, 40], scale 1.  The front view shows the y-z
//    face: content y∈[0,20] (view X), z∈[0,10] (view Y); the top
//    edge sits at view (0,10)-(20,10) → sheet (30,50)-(50,50).
struct AnnotationFixture {
  DocumentManager manager;
  DocumentState document;
  std::string body_id;
  std::string drawing_id;
  std::string sheet_id;
  std::string view_id;
};

AnnotationFixture make_fixture(bool cylinder = false) {
  AnnotationFixture fixture;
  fixture.manager.create_document();
  if (cylinder) {
    // Cylinder (r10, h30, axis Z, centered at the origin) with a TOP
    // view: the rim circle sits at the view origin.
    fixture.manager.add_cylinder_feature({.radius = 10.0, .height = 30.0});
  } else {
    fixture.manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
  }
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
  view.standard_view = cylinder ? "top" : "front";
  view.source_body_ids = {fixture.body_id};
  view.sheet_position = {30.0, 40.0};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, view);
  fixture.view_id = fixture.document.drawing.drawings[0].views[0].view_id;
  return fixture;
}

const Annotation* find_annotation(const DocumentState& document,
                                  const std::string& id) {
  for (const auto& drawing : document.drawing.drawings) {
    for (const auto& annotation : drawing.annotations) {
      if (annotation.annotation_id == id) {
        return &annotation;
      }
    }
  }
  return nullptr;
}

// ── Hand-built projections (validation + emitter tests) ────────────

ProjectedEdgeRecord make_line_record(double x0, double y0, double x1,
                                     double y1,
                                     const std::string& witness_body) {
  ProjectedEdgeRecord rec;
  rec.line_class = "visible";
  rec.curve_kind = "line";
  rec.p_start = {x0, y0};
  rec.p_end = {x1, y1};
  rec.first_param = 0.0;
  rec.last_param = std::hypot(x1 - x0, y1 - y0);
  SourceEdgeWitness witness;
  witness.body_id = witness_body;
  witness.curve_kind = "line";
  witness.start_point = {x0, y0, 0.0};
  witness.end_point = {x1, y1, 0.0};
  witness.length = rec.last_param;
  witness.tangent = {1.0, 0.0, 0.0};
  rec.source = witness;
  return rec;
}

ProjectedEdgeRecord make_circle_record(double cx, double cy, double radius,
                                       const std::string& witness_body) {
  ProjectedEdgeRecord rec;
  rec.line_class = "visible";
  rec.curve_kind = "circle";
  rec.circle_center = {{cx, cy}};
  rec.circle_radius = radius;
  rec.start_angle = 0.0;
  rec.end_angle = 2.0 * 3.141592653589793;
  rec.p_start = {cx + radius, cy};
  rec.p_end = {cx + radius, cy};
  rec.first_param = 0.0;
  rec.last_param = 2.0 * 3.141592653589793;
  SourceEdgeWitness witness;
  witness.body_id = witness_body;
  witness.curve_kind = "circle";
  witness.center = {{cx, cy, 0.0}};
  witness.radius = radius;
  witness.axis = {{0.0, 0.0, 1.0}};
  witness.param_range = {0.0, 2.0 * 3.141592653589793};
  rec.source = witness;
  return rec;
}

polysmith::core::ProjectionResult make_projection(
    std::vector<ProjectedEdgeRecord> edges) {
  polysmith::core::ProjectionResult result;
  result.edges = std::move(edges);
  result.source_revision = 1;
  return result;
}

DrawingView view_at_origin() {
  DrawingView view;
  view.view_id = "view-test";
  view.kind = "projection";
  view.standard_view = "front";
  view.scale = 1.0;
  view.sheet_position = {0.0, 0.0};
  return view;
}

// ── Dumps (golden pinning) ─────────────────────────────────────────

void dump_body(std::ostringstream& out,
               const std::vector<SheetPrimitive>& primitives,
               const std::vector<SheetText>& texts) {
  for (const auto& p : primitives) {
    // Glyph segments are pinned by the title-block glyph golden.
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
  for (const auto& t : texts) {
    out << "text " << t.purpose << " \"" << t.text << "\" "
        << quant(t.position[0]) << "," << quant(t.position[1]) << " h"
        << quant(t.height_mm) << (t.stale ? " stale" : "") << "\n";
  }
}

std::string dump_stream(const SheetPrimitiveStream& flat) {
  std::ostringstream out;
  out << "sheet " << flat.width_mm << "x" << flat.height_mm << "\n";
  dump_body(out, flat.primitives, flat.texts);
  return out.str();
}

/// The graphics builder's output, for kinds without an end-to-end
/// fixture (centerline needs two circles in one view).
std::string dump_graphics(const AnnotationGraphics& g) {
  std::ostringstream out;
  dump_body(out, g.primitives,
            g.text.has_value()
                ? std::vector<SheetText>{g.text.value()}
                : std::vector<SheetText>{});
  return out.str();
}

// ── Test 1: create a leader text from a pick + placement ───────────

bool test_leader_text_create() {
  AnnotationFixture fixture = make_fixture();
  // Pick the top edge at its midpoint (sheet 40,50), place the text
  // at (40,60) — 4 mm beyond the default anchor (40,56).
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {40.0, 50.0}, std::nullopt, "M6 THREAD", std::nullopt, {},
      std::array<double, 2>{40.0, 60.0});
  const auto& annotations = document.drawing.drawings[0].annotations;
  if (!expect(annotations.size() == 1, "one annotation created")) {
    return false;
  }
  const Annotation& annotation = annotations[0];
  if (!expect(annotation.kind == "leader_text" &&
                  annotation.annotation_id == "drawing-annotation-1" &&
                  annotation.source_edge_id == "drawing-edge-1" &&
                  annotation.view_id == fixture.view_id &&
                  annotation.witness.body_id == fixture.body_id &&
                  annotation.witness.curve_kind == "line",
              "leader text annotation minted with a line witness")) {
    return false;
  }
  // attach_param: the pick is the edge midpoint → fraction 0.5.
  if (!expect(annotation.attach_param.has_value() &&
                  near(annotation.attach_param.value(), 0.5, 1e-3),
              "attach_param is the midpoint fraction")) {
    return false;
  }
  // text_offset = placement − default anchor: the default anchor sits
  // 6 mm above the attach point (perp of the +x edge), so (0, 4).
  if (!expect(annotation.text_offset.has_value() &&
                  near(annotation.text_offset.value()[0], 0.0, 1e-3) &&
                  near(annotation.text_offset.value()[1], 4.0, 1e-3),
              "text_offset = placement − default anchor")) {
    return false;
  }
  // The refresh pass resolved the attachment into the runtime cache.
  const ResolvedAttachment* resolved =
      polysmith::core::drawing_runtime::cached_attachment(
          document, annotation.annotation_id);
  if (!expect(resolved != nullptr && !resolved->broken &&
                  near(resolved->attach_point[0], 10.0) &&
                  near(resolved->attach_point[1], 10.0) &&
                  resolved->text == "M6 THREAD",
              "attachment resolved at the pick point")) {
    return false;
  }
  // Flatten: leader line + arrowhead (filled_poly) + text record.
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value() &&
                  count_purpose(flat.value(), "annotation") == 2,
              "leader line + arrowhead emitted")) {
    return false;
  }
  const SheetText* text = find_text(flat.value(), "annotation");
  if (!expect(text != nullptr && text->text == "M6 THREAD" &&
                  text->annotation_id.has_value() &&
                  text->annotation_id.value() == "drawing-annotation-1" &&
                  near(text->position[0], 40.0) &&
                  near(text->position[1], 60.0),
              "annotation text lands at the placement")) {
    return false;
  }
  return true;
}

// ── Test 2: kind validation throws before mutating ─────────────────

bool test_annotation_kind_validation() {
  AnnotationFixture fixture = make_fixture();
  if (!expect(throws([&]() {
        fixture.manager.drawing_annotation_create(
            fixture.drawing_id, fixture.view_id, "not_a_kind",
            {40.0, 50.0}, std::nullopt, std::nullopt, std::nullopt, {},
            std::nullopt);
      }),
              "unknown kind throws")) {
    return false;
  }
  // center_mark / centerline on line edges (validated by
  // resolve_annotation_picks, exercised here though those kinds ship
  // in P2); edge_extension needs a line.
  if (!expect(throws([&]() {
        fixture.manager.drawing_annotation_create(
            fixture.drawing_id, fixture.view_id, "center_mark",
            {40.0, 50.0}, std::nullopt, std::nullopt, std::nullopt, {},
            std::nullopt);
      }),
              "center_mark on a line throws")) {
    return false;
  }
  if (!expect(throws([&]() {
        fixture.manager.drawing_annotation_create(
            fixture.drawing_id, fixture.view_id, "centerline",
            {40.0, 50.0}, std::nullopt, std::nullopt, std::nullopt, {},
            std::nullopt);
      }),
              "centerline without a second pick throws")) {
    return false;
  }
  if (!expect(throws([&]() {
        fixture.manager.drawing_annotation_create(
            fixture.drawing_id, "no-such-view", "leader_text",
            {40.0, 50.0}, std::nullopt, std::nullopt, std::nullopt, {},
            std::nullopt);
      }),
              "unknown view throws")) {
    return false;
  }
  return expect(fixture.manager.get_document().value()
                        .drawing.drawings[0]
                        .annotations.empty(),
                "nothing was mutated");
}

// ── Test 3: attach_param follows the pick along the edge ───────────

bool test_attach_param_roundtrip() {
  AnnotationFixture fixture = make_fixture();
  // Pick at 3/4 of the top edge → fraction 0.75.
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {45.0, 50.0}, std::nullopt, "x", std::nullopt, {}, std::nullopt);
  const Annotation& annotation =
      document.drawing.drawings[0].annotations[0];
  if (!expect(annotation.attach_param.has_value() &&
                  near(annotation.attach_param.value(), 0.75, 1e-3),
              "pick at 3/4 stores fraction 0.75")) {
    return false;
  }
  // The resolved attach point sits at the fraction.
  const ResolvedAttachment* resolved =
      polysmith::core::drawing_runtime::cached_attachment(
          document, annotation.annotation_id);
  return expect(resolved != nullptr &&
                    near(resolved->attach_point[0], 15.0) &&
                    near(resolved->attach_point[1], 10.0),
                "resolved attach point at the fraction (view 15,10)");
}

// ── Test 4: attach_param + text_offset survive save/load ───────────

bool test_attach_param_save_load() {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {45.0, 50.0}, std::nullopt, "saved", std::nullopt, {},
      std::array<double, 2>{45.0, 62.0});
  const auto payload = polysmith::protocol::to_payload(document, true);
  const DocumentState restored =
      polysmith::protocol::document_from_payload(payload);
  const auto& annotations = restored.drawing.drawings[0].annotations;
  if (!expect(annotations.size() == 1, "annotation survives the round-trip")) {
    return false;
  }
  const Annotation& annotation = annotations[0];
  return expect(annotation.kind == "leader_text" &&
                    annotation.attach_param.has_value() &&
                    near(annotation.attach_param.value(), 0.75, 1e-3) &&
                    annotation.text_offset.has_value() &&
                    annotation.text_override.has_value() &&
                    annotation.text_override.value() == "saved" &&
                    annotation.witness.body_id == fixture.body_id,
                "kind/attach_param/text_offset/text/witness survive");
}

// ── Test 5: cosmetic update moves the text and re-aims the leader ──

bool test_leader_text_update_text_offset() {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {40.0, 50.0}, std::nullopt, "drag me", std::nullopt, {},
      std::array<double, 2>{40.0, 60.0});
  // Move the text 10 mm right: the leader re-aims from the attach
  // point through the dragged text.
  document = fixture.manager.drawing_annotation_update(
      fixture.drawing_id, "drawing-annotation-1", std::nullopt, std::nullopt,
      std::nullopt, std::array<double, 2>{10.0, 4.0}, std::nullopt,
      std::nullopt);
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  const SheetText* text = find_text(flat.value(), "annotation");
  if (!expect(text != nullptr && near(text->position[0], 50.0) &&
                  near(text->position[1], 60.0),
              "dragged text lands at (50,60)")) {
    return false;
  }
  // The leader line runs from the attach point (40,50) toward the
  // text — its far end stops short of the text's near edge.
  const SheetPrimitive* leader = nullptr;
  for (const auto& p : flat->primitives) {
    if (p.purpose == "annotation" && p.kind == "line") {
      leader = &p;
      break;
    }
  }
  if (!expect(leader != nullptr && near(leader->p0[0], 40.0) &&
                  near(leader->p0[1], 50.0),
              "leader anchored at the attach point")) {
    return false;
  }
  const double dx = leader->p1[0] - leader->p0[0];
  const double dy = leader->p1[1] - leader->p0[1];
  // The leader re-aims from the attach point straight toward the
  // dragged text and stops 2.75 mm short of it (text half-height +
  // landing gap).  The text sits at attach + (10,10): +10 from the
  // offset x, +6 default rise + 4 offset y — so the leader direction
  // is ∥ (10,10) with length |(10,10)| − 2.75.
  return expect(near(std::hypot(dx, dy),
                     std::hypot(10.0, 10.0) - 2.75, 0.01) &&
                    near(dx / dy, 1.0, 0.01),
                "leader re-aims toward the dragged text");
}

// ── Test 6: delete ─────────────────────────────────────────────────

bool test_leader_text_delete() {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {40.0, 50.0}, std::nullopt, "x", std::nullopt, {}, std::nullopt);
  document = fixture.manager.drawing_annotation_delete(
      fixture.drawing_id, "drawing-annotation-1");
  if (!expect(document.drawing.drawings[0].annotations.empty(),
              "annotation deleted")) {
    return false;
  }
  return expect(polysmith::core::drawing_runtime::cached_attachment(
                    document, "drawing-annotation-1") == nullptr,
                "cached attachment erased with the annotation");
}

// ── Test 7: undo/redo ──────────────────────────────────────────────

bool test_leader_text_undo_redo() {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {40.0, 50.0}, std::nullopt, "x", std::nullopt, {}, std::nullopt);
  const std::string annotation_id =
      document.drawing.drawings[0].annotations[0].annotation_id;

  document = fixture.manager.undo();
  if (!expect(find_annotation(document, annotation_id) == nullptr,
              "undo removes the annotation")) {
    return false;
  }
  if (!expect(polysmith::core::drawing_runtime::cached_attachment(
                  document, annotation_id) == nullptr,
              "undo clears the cached attachment")) {
    return false;
  }
  document = fixture.manager.redo();
  const Annotation* annotation = find_annotation(document, annotation_id);
  if (!expect(annotation != nullptr, "redo restores the annotation")) {
    return false;
  }
  return expect(polysmith::core::drawing_runtime::cached_attachment(
                    document, annotation_id) != nullptr,
                "redo re-resolves the attachment");
}

// ── Test 8: deleting the source body degrades (never blank) ────────

bool test_leader_text_broken_on_source_delete() {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {40.0, 50.0}, std::nullopt, "x", std::nullopt, {}, std::nullopt);
  const std::string annotation_id =
      document.drawing.drawings[0].annotations[0].annotation_id;

  document = fixture.manager.delete_feature(fixture.body_id);
  const Annotation* annotation = find_annotation(document, annotation_id);
  if (!expect(annotation != nullptr && annotation->dependency_broken &&
                  !annotation->warning.empty(),
              "broken body sets dependency_broken + warning")) {
    return false;
  }
  const ResolvedAttachment* resolved =
      polysmith::core::drawing_runtime::cached_attachment(
          document, annotation_id);
  return expect(resolved != nullptr && resolved->stale,
                "last-known attachment held, marked stale");
}

// ── Test 9: golden (A4 sheet with one leader text) ─────────────────

bool test_leader_text_golden() {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "leader_text",
      {40.0, 50.0}, std::nullopt, "M6", std::nullopt, {},
      std::array<double, 2>{40.0, 60.0});
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet flattens")) {
    return false;
  }
  return check_golden("drawing_annotation_leader_text",
                      dump_stream(flat.value()));
}

// ── Phase 2/3 shared helper ────────────────────────────────────────

/// The sheet point on the fixture view's first circle record (the
/// right side of the rim) — derived from the cached projection so the
/// test stays correct whatever the standard-frame origin convention.
std::optional<std::array<double, 2>> first_circle_rim_sheet(
    const AnnotationFixture& fixture) {
  const polysmith::core::ProjectionResult* projection =
      polysmith::core::drawing_runtime::cached_projection(fixture.document,
                                                          fixture.view_id);
  if (projection == nullptr) {
    return std::nullopt;
  }
  const DrawingView* view = nullptr;
  for (const auto& v : fixture.document.drawing.drawings[0].views) {
    if (v.view_id == fixture.view_id) {
      view = &v;
      break;
    }
  }
  if (view == nullptr) {
    return std::nullopt;
  }
  for (const auto& rec : projection->edges) {
    if (rec.curve_kind == "circle" && rec.circle_center.has_value() &&
        rec.circle_radius.has_value()) {
      return std::array<double, 2>{
          view->sheet_position[0] +
              view->scale * (rec.circle_center.value()[0] +
                             rec.circle_radius.value()),
          view->sheet_position[1] +
              view->scale * (rec.circle_center.value()[1])};
    }
  }
  return std::nullopt;
}

// ── Test 10: center mark end-to-end (cylinder top view) ────────────

bool test_center_mark_create_flatten() {
  AnnotationFixture fixture = make_fixture(/*cylinder=*/true);
  const auto rim = first_circle_rim_sheet(fixture);
  if (!expect(rim.has_value(), "cylinder top view shows a circle record")) {
    return false;
  }
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "center_mark",
      rim.value(), std::nullopt, std::nullopt, std::nullopt, {}, std::nullopt);
  const auto& annotations = document.drawing.drawings[0].annotations;
  if (!expect(annotations.size() == 1, "one center mark created")) {
    return false;
  }
  const Annotation& annotation = annotations[0];
  if (!expect(annotation.kind == "center_mark" &&
                  annotation.witness.curve_kind == "circle" &&
                  !annotation.attach_param.has_value(),
              "center mark: circle witness, no attach_param (center anchor)")) {
    return false;
  }
  // Copy the id NOW — the reference dies with the pre-delete document
  // below.
  const std::string annotation_id = annotation.annotation_id;
  const ResolvedAttachment* resolved =
      polysmith::core::drawing_runtime::cached_attachment(
          document, annotation_id);
  if (!expect(resolved != nullptr && !resolved->broken &&
                  resolved->a_kind == "circle" &&
                  resolved->a_center.has_value(),
              "attachment resolved on the circle center")) {
    return false;
  }
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value() &&
                  count_purpose(flat.value(), "annotation") == 2,
              "two cross arms emitted")) {
    return false;
  }
  // The cross is centered on the circle center (sheet-mm).
  const double cx = 30.0 + resolved->a_center.value()[0];
  const double cy = 40.0 + resolved->a_center.value()[1];
  bool saw_h = false;
  bool saw_v = false;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "annotation") {
      continue;
    }
    if (near(p.p0[0], cx - 2.5) && near(p.p1[0], cx + 2.5) &&
        near(p.p0[1], cy) && near(p.p1[1], cy)) {
      saw_h = true;
    }
    if (near(p.p0[0], cx) && near(p.p1[0], cx) &&
        near(p.p0[1], cy - 2.5) && near(p.p1[1], cy + 2.5)) {
      saw_v = true;
    }
  }
  if (!expect(saw_h && saw_v, "cross centered on the circle center")) {
    return false;
  }
  if (!expect(find_text(flat.value(), "annotation") == nullptr,
              "no text for a center mark")) {
    return false;
  }
  // Delete the cylinder → degraded, never blank.
  document = fixture.manager.delete_feature(fixture.body_id);
  const Annotation* broken_annotation =
      find_annotation(document, annotation_id);
  return expect(broken_annotation != nullptr &&
                    broken_annotation->dependency_broken &&
                    !broken_annotation->warning.empty(),
                "broken body degrades the center mark");
}

// ── Test 11: center mark refuses a line edge ───────────────────────

bool test_center_mark_rejects_line() {
  auto projection = make_projection(
      {make_line_record(10.0, 10.0, 30.0, 10.0, "b1")});
  std::string error;
  const auto resolved = polysmith::core::resolve_annotation_picks(
      projection, view_at_origin(), "center_mark", {20.0, 10.0}, std::nullopt,
      5.0, std::nullopt, "", &error);
  return expect(!resolved.has_value() &&
                    error.find("circle") != std::string::npos,
                "center mark on a line refused with the circle message");
}

// ── Test 12: centerline validation + chain emitter ─────────────────

bool test_centerline_validation_and_emitter() {
  const DrawingView view = view_at_origin();
  auto projection = make_projection({
      make_circle_record(10.0, 10.0, 5.0, "a"),
      make_circle_record(30.0, 10.0, 5.0, "b"),
  });
  std::string error;
  // Missing second pick.
  if (!expect(!polysmith::core::resolve_annotation_picks(
                       projection, view, "centerline", {10.0, 15.0},
                       std::nullopt, 5.0, std::nullopt, "", &error)
                       .has_value() &&
                  error.find("second") != std::string::npos,
              "centerline without a second pick refused")) {
    return false;
  }
  // Second pick on a line.
  auto mixed = make_projection({
      make_circle_record(10.0, 10.0, 5.0, "a"),
      make_line_record(60.0, 0.0, 70.0, 0.0, "c"),
  });
  if (!expect(!polysmith::core::resolve_annotation_picks(
                       mixed, view, "centerline", {10.0, 15.0},
                       std::array<double, 2>{65.0, 0.0},
                       5.0, std::nullopt, "", &error)
                       .has_value() &&
                  error.find("two circles") != std::string::npos,
              "centerline on a line second pick refused")) {
    return false;
  }
  // Concentric circles.
  auto concentric = make_projection({
      make_circle_record(10.0, 10.0, 5.0, "a"),
      make_circle_record(10.0, 10.0, 8.0, "b"),
  });
  if (!expect(!polysmith::core::resolve_annotation_picks(
                       concentric, view, "centerline", {15.0, 10.0},
                       std::array<double, 2>{10.0, 2.0}, 5.0, std::nullopt,
                       "", &error)
                       .has_value() &&
                  error.find("different centers") != std::string::npos,
              "concentric circles refused")) {
    return false;
  }
  // Valid: second center captured; chain spans 7→33 (3 mm overshoot
  // both sides) with the dash+dot coverage 21.5 (spans 6/0.25 cycles).
  const auto resolved = polysmith::core::resolve_annotation_picks(
      projection, view, "centerline", {10.0, 15.0},
      std::array<double, 2>{30.0, 15.0}, 5.0, std::nullopt, "", &error);
  if (!expect(resolved.has_value() && resolved->b_center.has_value() &&
                  near(resolved->b_center.value()[0], 30.0) &&
                  near(resolved->b_center.value()[1], 10.0),
              "second circle center captured")) {
    return false;
  }
  Annotation annotation;
  annotation.kind = "centerline";
  AnnotationGraphics graphics = polysmith::core::build_annotation_graphics(
      resolved.value(), annotation, view, false);
  if (!expect(!graphics.primitives.empty() && !graphics.text.has_value(),
              "chain line emitted, no text")) {
    return false;
  }
  double covered = 0.0;
  bool starts_at_overrun = false;
  bool ends_at_overrun = false;
  for (const auto& p : graphics.primitives) {
    if (p.kind != "line" || p.purpose != "annotation") {
      return expect(false, "chain line is line primitives only");
    }
    if (!near(p.p0[1], 10.0) || !near(p.p1[1], 10.0)) {
      return expect(false, "chain line stays horizontal through the centers");
    }
    covered += std::hypot(p.p1[0] - p.p0[0], p.p1[1] - p.p0[1]);
    if (near(p.p0[0], 7.0)) {
      starts_at_overrun = true;
    }
    if (near(p.p1[0], 33.0)) {
      ends_at_overrun = true;
    }
  }
  return expect(near(covered, 21.5, 0.02) && starts_at_overrun &&
                    ends_at_overrun,
                "chain spans 7→33 with dash+dot coverage 21.5");
}

// ── Test 13: edge extension snaps to the picked end ────────────────

bool test_edge_extension_snap_end() {
  auto projection = make_projection(
      {make_line_record(10.0, 10.0, 30.0, 10.0, "b1")});
  const DrawingView view = view_at_origin();
  std::string error;
  std::optional<double> attach_param;
  // Near the left end → fraction snapped to 0, extension out the left.
  auto resolved = polysmith::core::resolve_annotation_picks(
      projection, view, "edge_extension", {10.6, 10.0}, std::nullopt, 5.0,
      std::nullopt, "", &error, nullptr, &attach_param);
  if (!expect(resolved.has_value() && attach_param.has_value() &&
                  near(attach_param.value(), 0.0) &&
                  near(resolved->attach_point[0], 10.0),
              "left pick snaps to end 0")) {
    return false;
  }
  Annotation annotation;
  annotation.kind = "edge_extension";
  annotation.attach_param = 0.0;
  auto graphics = polysmith::core::build_annotation_graphics(
      resolved.value(), annotation, view, false);
  if (!expect(graphics.primitives.size() == 1 &&
                  graphics.primitives[0].kind == "line" &&
                  near(graphics.primitives[0].p0[0], 10.0) &&
                  near(graphics.primitives[0].p1[0], 5.0),
              "extension runs 5 mm out the left end")) {
    return false;
  }
  // Near the right end → fraction 1, extension out the right.
  attach_param.reset();
  resolved = polysmith::core::resolve_annotation_picks(
      projection, view, "edge_extension", {29.4, 10.0}, std::nullopt, 5.0,
      std::nullopt, "", &error, nullptr, &attach_param);
  if (!expect(resolved.has_value() && attach_param.has_value() &&
                  near(attach_param.value(), 1.0),
              "right pick snaps to end 1")) {
    return false;
  }
  annotation.attach_param = 1.0;
  graphics = polysmith::core::build_annotation_graphics(
      resolved.value(), annotation, view, false);
  return expect(graphics.primitives.size() == 1 &&
                    near(graphics.primitives[0].p0[0], 30.0) &&
                    near(graphics.primitives[0].p1[0], 35.0),
                "extension runs 5 mm out the right end");
}

// ── Test 14: edge extension end-to-end (box top edge) ──────────────

bool test_edge_extension_create_flatten() {
  AnnotationFixture fixture = make_fixture();
  // Box top edge spans sheet x∈[30,50]; pick near the left end.
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "edge_extension",
      {31.0, 50.0}, std::nullopt, std::nullopt, std::nullopt, {},
      std::nullopt);
  const auto& annotations = document.drawing.drawings[0].annotations;
  if (!expect(annotations.size() == 1 &&
                  annotations[0].attach_param.has_value() &&
                  near(annotations[0].attach_param.value(), 0.0),
              "edge extension: fraction snapped to the left end")) {
    return false;
  }
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value() &&
                  count_purpose(flat.value(), "annotation") == 1,
              "one extension line emitted")) {
    return false;
  }
  const SheetPrimitive* ext = nullptr;
  for (const auto& p : flat->primitives) {
    if (p.purpose == "annotation") {
      ext = &p;
      break;
    }
  }
  return expect(ext != nullptr && near(ext->p0[0], 30.0) &&
                    near(ext->p0[1], 50.0) && near(ext->p1[0], 25.0) &&
                    near(ext->p1[1], 50.0),
                "extension 5 mm outward from the left end");
}

// ── Test 15: geometry annotation save/load ─────────────────────────

bool test_geometry_annotation_save_load() {
  AnnotationFixture fixture = make_fixture(/*cylinder=*/true);
  const auto rim = first_circle_rim_sheet(fixture);
  if (!expect(rim.has_value(), "cylinder top view shows a circle record")) {
    return false;
  }
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "center_mark",
      rim.value(), std::nullopt, std::nullopt, std::nullopt, {}, std::nullopt);
  const auto payload = polysmith::protocol::to_payload(document, true);
  const DocumentState restored =
      polysmith::protocol::document_from_payload(payload);
  const auto& annotations = restored.drawing.drawings[0].annotations;
  if (!expect(annotations.size() == 1, "center mark survives the round-trip")) {
    return false;
  }
  const Annotation& annotation = annotations[0];
  return expect(annotation.kind == "center_mark" &&
                    !annotation.attach_param.has_value() &&
                    annotation.witness.curve_kind == "circle" &&
                    annotation.witness.body_id == fixture.body_id,
                "kind/witness survive; attach_param stays absent");
}

// ── Test 16: symbol emitters end-to-end (box fixture) ──────────────

bool create_symbol_and_check(const std::string& kind,
                             const std::string& text,
                             const std::string& label) {
  AnnotationFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, kind, {40.0, 50.0}, std::nullopt,
      text, std::nullopt, {}, std::nullopt);
  if (!expect(document.drawing.drawings[0].annotations.size() == 1,
              (label + ": one annotation created").c_str())) {
    return false;
  }
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), (label + ": sheet flattens").c_str())) {
    return false;
  }
  const SheetText* text_record = find_text(flat.value(), "annotation");
  if (!expect(text_record != nullptr && text_record->text == text,
              (label + ": text record carries the content").c_str())) {
    return false;
  }
  // Per-kind expected graphics (the attach is the box top edge at
  // sheet (40,50); the default direction is straight up).
  int lines = 0;
  int polys = 0;
  int arcs = 0;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "annotation") {
      continue;
    }
    if (p.kind == "line") {
      ++lines;
    } else if (p.kind == "filled_poly") {
      ++polys;
    } else if (p.kind == "circle_arc") {
      ++arcs;
    }
  }
  if (kind == "surface_finish") {
    if (!expect(lines == 2 && polys == 0 && arcs == 0 &&
                    near(text_record->position[0], 42.8, 0.01) &&
                    near(text_record->position[1], 52.2, 0.01) &&
                    text_record->h_align == "left",
                (label + ": check mark + left value text").c_str())) {
      return false;
    }
  } else if (kind == "welding") {
    if (!expect(lines == 2 && polys == 1 && arcs == 0 &&
                    near(text_record->position[0], 35.0, 0.01) &&
                    near(text_record->position[1], 58.25, 0.01) &&
                    text_record->h_align == "left",
                (label + ": arrow + reference line + symbol text").c_str())) {
      return false;
    }
  } else if (kind == "tolerance_frame") {
    if (!expect(lines == 5 && polys == 0 && arcs == 0 &&
                    near(text_record->position[0], 47.0, 0.01) &&
                    near(text_record->position[1], 56.0, 0.01),
                (label + ": leader + 4 frame lines + centered text").c_str())) {
      return false;
    }
  } else if (kind == "datum") {
    if (!expect(lines == 4 && polys == 1 && arcs == 0 &&
                    near(text_record->position[0], 44.25, 0.01) &&
                    near(text_record->position[1], 51.5, 0.01),
                (label + ": filled triangle + letter box").c_str())) {
      return false;
    }
  } else if (kind == "balloon") {
    if (!expect(lines == 1 && polys == 0 && arcs == 1 &&
                    near(text_record->position[0], 40.0, 0.01) &&
                    near(text_record->position[1], 56.0, 0.01),
                (label + ": leader + circle + centered number").c_str())) {
      return false;
    }
  } else {
    return expect(false, "unknown symbol kind in the test");
  }
  return true;
}

bool test_symbol_emitters_create() {
  return create_symbol_and_check("surface_finish", "N8", "surface_finish") &&
         create_symbol_and_check("welding", "W", "welding") &&
         create_symbol_and_check("tolerance_frame", "T", "tolerance_frame") &&
         create_symbol_and_check("datum", "A", "datum") &&
         create_symbol_and_check("balloon", "1", "balloon");
}

// ── Test 17: symbol extensions round-trip ──────────────────────────

bool test_symbol_extensions_roundtrip() {
  AnnotationFixture fixture = make_fixture();
  std::vector<polysmith::core::AnnotationExtension> extensions = {
      {"tolerance_frame", {{"tolerance", "0.1"}}},
  };
  DocumentState document = fixture.manager.drawing_annotation_create(
      fixture.drawing_id, fixture.view_id, "welding", {40.0, 50.0},
      std::nullopt, "ISO 2553", std::nullopt, extensions, std::nullopt);
  const auto& created = document.drawing.drawings[0].annotations;
  if (!expect(created.size() == 1 && created[0].extensions.size() == 1 &&
                  created[0].extensions[0].kind == "tolerance_frame" &&
                  created[0].extensions[0].fields.size() == 1 &&
                  created[0].extensions[0].fields[0][0] == "tolerance" &&
                  created[0].extensions[0].fields[0][1] == "0.1",
              "created annotation carries the extensions")) {
    return false;
  }
  const auto payload = polysmith::protocol::to_payload(document, true);
  const DocumentState restored =
      polysmith::protocol::document_from_payload(payload);
  const auto& saved = restored.drawing.drawings[0].annotations;
  return expect(saved.size() == 1 && saved[0].extensions.size() == 1 &&
                    saved[0].extensions[0].kind == "tolerance_frame" &&
                    saved[0].extensions[0].fields[0][1] == "0.1",
                "extensions survive the round-trip");
}

// ── Test 18: goldens (geometry + symbols) ──────────────────────────

bool test_geometry_symbol_goldens() {
  bool ok = true;
  {
    AnnotationFixture fixture = make_fixture(/*cylinder=*/true);
    const auto rim = first_circle_rim_sheet(fixture);
    if (expect(rim.has_value(), "cylinder rim for the golden")) {
      DocumentState document = fixture.manager.drawing_annotation_create(
          fixture.drawing_id, fixture.view_id, "center_mark", rim.value(),
          std::nullopt, std::nullopt, std::nullopt, {}, std::nullopt);
      const auto flat = polysmith::core::flatten_sheet(
          document, fixture.drawing_id, fixture.sheet_id);
      ok = ok && expect(flat.has_value(), "center mark golden flattens") &&
           check_golden("drawing_annotation_center_mark",
                        dump_stream(flat.value()));
    } else {
      ok = false;
    }
  }
  {
    AnnotationFixture fixture = make_fixture();
    DocumentState document = fixture.manager.drawing_annotation_create(
        fixture.drawing_id, fixture.view_id, "edge_extension",
        {31.0, 50.0}, std::nullopt, std::nullopt, std::nullopt, {},
        std::nullopt);
    const auto flat = polysmith::core::flatten_sheet(
        document, fixture.drawing_id, fixture.sheet_id);
    ok = ok && expect(flat.has_value(), "edge extension golden flattens") &&
         check_golden("drawing_annotation_edge_extension",
                      dump_stream(flat.value()));
  }
  const struct {
    const char* kind;
    const char* text;
  } symbol_goldens[] = {
      {"surface_finish", "N8"}, {"welding", "W"},  {"tolerance_frame", "T"},
      {"datum", "A"},           {"balloon", "1"},
  };
  for (const auto& entry : symbol_goldens) {
    AnnotationFixture fixture = make_fixture();
    DocumentState document = fixture.manager.drawing_annotation_create(
        fixture.drawing_id, fixture.view_id, entry.kind, {40.0, 50.0},
        std::nullopt, entry.text, std::nullopt, {}, std::nullopt);
    const auto flat = polysmith::core::flatten_sheet(
        document, fixture.drawing_id, fixture.sheet_id);
    ok = ok && expect(flat.has_value(), "symbol golden flattens") &&
         check_golden(std::string("drawing_annotation_") + entry.kind,
                      dump_stream(flat.value()));
  }
  {
    // Centerline: no two-circle end-to-end fixture — pin the graphics
    // builder's output directly.
    auto projection = make_projection({
        make_circle_record(10.0, 10.0, 5.0, "a"),
        make_circle_record(30.0, 10.0, 5.0, "b"),
    });
    const DrawingView view = view_at_origin();
    std::string error;
    const auto resolved = polysmith::core::resolve_annotation_picks(
        projection, view, "centerline", {10.0, 15.0},
        std::array<double, 2>{30.0, 15.0}, 5.0, std::nullopt, "", &error);
    if (expect(resolved.has_value(), "centerline resolves for the golden")) {
      Annotation annotation;
      annotation.kind = "centerline";
      AnnotationGraphics graphics = polysmith::core::build_annotation_graphics(
          resolved.value(), annotation, view, false);
      ok = ok && check_golden("drawing_annotation_centerline",
                              dump_graphics(graphics));
    } else {
      ok = false;
    }
  }
  return ok;
}

}  // namespace

int main() {
  int passed = 0;
  const int total = 18;
  struct Test {
    const char* name;
    bool (*fn)();
  };
  const Test tests[] = {
      {"leader_text_create", test_leader_text_create},
      {"annotation_kind_validation", test_annotation_kind_validation},
      {"attach_param_roundtrip", test_attach_param_roundtrip},
      {"attach_param_save_load", test_attach_param_save_load},
      {"leader_text_update_text_offset", test_leader_text_update_text_offset},
      {"leader_text_delete", test_leader_text_delete},
      {"leader_text_undo_redo", test_leader_text_undo_redo},
      {"leader_text_broken_on_source_delete",
       test_leader_text_broken_on_source_delete},
      {"leader_text_golden", test_leader_text_golden},
      {"center_mark_create_flatten", test_center_mark_create_flatten},
      {"center_mark_rejects_line", test_center_mark_rejects_line},
      {"centerline_validation_and_emitter",
       test_centerline_validation_and_emitter},
      {"edge_extension_snap_end", test_edge_extension_snap_end},
      {"edge_extension_create_flatten", test_edge_extension_create_flatten},
      {"geometry_annotation_save_load", test_geometry_annotation_save_load},
      {"symbol_emitters_create", test_symbol_emitters_create},
      {"symbol_extensions_roundtrip", test_symbol_extensions_roundtrip},
      {"geometry_symbol_goldens", test_geometry_symbol_goldens},
  };
  for (const auto& test : tests) {
    if (test.fn()) {
      ++passed;
    } else {
      std::cerr << "FAILED TEST: " << test.name << "\n";
    }
  }
  std::cout << passed << "/" << total << " drawing annotation tests passed\n";
  return passed == total ? 0 : 1;
}
