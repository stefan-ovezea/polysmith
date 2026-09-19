// Drawing sheet flattening test (P5).
//
// Pins the flattened sheet stream (core/drawing/drawing_sheet.cpp):
//   - ISO 5457 trimmed sizes + orientation swap
//   - furniture: 0.7 mm frame at the 20/10 mm margins, 4 centring
//     marks extending 5 mm into the margin, grid-reference ticks
//     (A4: 4x6 divisions), ISO 5456-2 projection symbol (first and
//     third angle orientation)
//   - view flattening: transforms, ISO line styles (thick visible,
//     thin seams, dropped smooth), hidden dashed with the Annex-A
//     corner rule (exact span sets), cutting-plane chain with dots
//   - coincidence priority (a visible edge beats a cutting-plane
//     trace on the same geometry)
//   - drawing_sheet_update validation + re-flatten
//   - determinism + golden

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <variant>

#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pnt.hxx>

#include "core/document/document.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_sheet.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::ProjectionInput;
using polysmith::core::ProjectionResult;
using polysmith::core::SectionDefinition;
using polysmith::core::SheetPrimitive;
using polysmith::core::SheetPrimitiveStream;
using polysmith::core::SourceBody;

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

/// Creates a document with one drawing + one A4 sheet and returns
/// the ids.
struct DrawingFixture {
  DocumentManager manager;
  DocumentState document;
  std::string drawing_id;
  std::string sheet_id;
};

DrawingFixture make_fixture(const std::string& paper_size = "A4",
                            const std::string& orientation = "portrait",
                            const std::string& angle = "first_angle") {
  DrawingFixture fixture;
  fixture.manager.create_document();
  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  sheet.paper_size = paper_size;
  sheet.orientation = orientation;
  sheet.projection_angle = angle;
  drawing.sheets.push_back(sheet);
  fixture.document = fixture.manager.drawing_create(drawing);
  fixture.drawing_id = fixture.document.drawing.drawings[0].drawing_id;
  fixture.sheet_id = fixture.document.drawing.drawings[0].sheets[0].sheet_id;
  return fixture;
}

// ── Test 1: ISO 5457 trimmed sizes ────────────────────────────────

bool test_paper_sizes() {
  const auto expect_size = [&](const char* name, double w, double h) {
    const auto size = polysmith::core::paper_size_mm(name);
    return expect(near(size[0], w) && near(size[1], h), name);
  };
  if (!expect_size("A0", 841.0, 1189.0)) return false;
  if (!expect_size("A1", 594.0, 841.0)) return false;
  if (!expect_size("A2", 420.0, 594.0)) return false;
  if (!expect_size("A3", 297.0, 420.0)) return false;
  if (!expect_size("A4", 210.0, 297.0)) return false;
  const auto unknown = polysmith::core::paper_size_mm("Letter");
  return expect(near(unknown[0], 210.0) && near(unknown[1], 297.0),
                "unknown paper size falls back to A4");
}

// ── Test 2: furniture (frame, centring, grid, symbol) ─────────────

bool test_furniture() {
  DrawingFixture fixture = make_fixture();
  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "A4 sheet flattens")) {
    return false;
  }
  if (!expect(near(flat->width_mm, 210.0) && near(flat->height_mm, 297.0),
              "A4 portrait: 210x297")) {
    return false;
  }

  // Frame: 4 primitives, 0.7 mm, at the ISO 5457 margins (left 20,
  // others 10).
  if (!expect(count_purpose(flat.value(), "frame") == 4,
              "frame has 4 primitives")) {
    return false;
  }
  bool frame_ok = true;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "frame") {
      continue;
    }
    if (!near(p.style.width_mm, 0.7)) frame_ok = false;
    for (const auto& q : {p.p0, p.p1}) {
      const bool on_x = near(q[0], 20.0) || near(q[0], 200.0);
      const bool on_y = near(q[1], 10.0) || near(q[1], 287.0);
      if (!on_x || !on_y) frame_ok = false;
    }
  }
  if (!expect(frame_ok, "frame at 20/10 mm margins, 0.7 mm wide")) {
    return false;
  }

  // Centring marks: 4, straddling each side's midpoint, extending
  // 5 mm OUTWARD into the margin.
  if (!expect(count_purpose(flat.value(), "centring_mark") == 4,
              "4 centring marks")) {
    return false;
  }
  bool marks_ok = true;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "centring_mark") {
      continue;
    }
    if (!near(p.style.width_mm, 0.5)) marks_ok = false;
    // Each mark is vertical at mid-x or horizontal at mid-y.
    const bool vertical = near(p.p0[0], 110.0) && near(p.p1[0], 110.0) &&
                          near(p.p0[1], 10.0) && near(p.p1[1], 5.0) ||
                          near(p.p0[0], 110.0) && near(p.p1[0], 110.0) &&
                          near(p.p0[1], 287.0) && near(p.p1[1], 292.0);
    const bool horizontal = near(p.p0[1], 148.5) && near(p.p1[1], 148.5) &&
                            near(p.p0[0], 20.0) && near(p.p1[0], 15.0) ||
                            near(p.p0[1], 148.5) && near(p.p1[1], 148.5) &&
                            near(p.p0[0], 200.0) && near(p.p1[0], 205.0);
    if (!vertical && !horizontal) marks_ok = false;
  }
  if (!expect(marks_ok, "centring marks straddle the side midpoints")) {
    return false;
  }

  // Grid-reference ticks: A4 = 6 long-side / 4 short-side divisions
  // → (4-1)*2 vertical + (6-1)*2 horizontal = 16 ticks.
  if (!expect(count_purpose(flat.value(), "grid_ref") == 16,
              "A4 grid ticks: 4x6 divisions → 16 ticks")) {
    return false;
  }

  // Projection symbol: 2 concentric circles + a frustum (4 lines),
  // first angle = the large end nearest the circles.
  if (!expect(count_purpose(flat.value(), "projection_symbol") == 6,
              "projection symbol: 2 circles + 4 frustum lines")) {
    return false;
  }
  bool circles_ok = false;
  bool frustum_first_angle = true;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "projection_symbol") {
      continue;
    }
    if (p.kind == "circle_arc" && p.radius.has_value()) {
      circles_ok = circles_ok || near(p.radius.value(), 5.0) ||
                   near(p.radius.value(), 2.5);
    }
  }
  // Frustum: the near base (x ≈ 110) must be the TALL one in first
  // angle.  The two vertical base lines are at x≈110 and x≈125.
  double near_base_height = 0.0;
  double far_base_height = 0.0;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "projection_symbol" || p.kind != "line") {
      continue;
    }
    if (near(p.p0[0], p.p1[0], 1e-4) && near(p.p0[0], 110.0, 0.01)) {
      near_base_height = std::abs(p.p1[1] - p.p0[1]);
    }
    if (near(p.p0[0], p.p1[0], 1e-4) && near(p.p0[0], 125.0, 0.01)) {
      far_base_height = std::abs(p.p1[1] - p.p0[1]);
    }
  }
  if (!expect(near(near_base_height, 10.0) && near(far_base_height, 5.0),
              "first angle: large end nearest the circles")) {
    std::cerr << "  near base " << near_base_height << " far base "
              << far_base_height << "\n";
    return false;
  }
  if (!expect(circles_ok, "symbol circles: radii 5 and 2.5")) {
    return false;
  }
  return true;
}

// ── Test 3: view flattening + dash patterns ───────────────────────

/// Stores a custom projection for the view (the flatten reads the
/// runtime cache — direct injection keeps the test focused on the
/// flatten math).
ProjectionResult store_projection(DocumentState& document,
                                  const std::string& view_id,
                                  const ProjectionInput& input) {
  ProjectionResult result = polysmith::core::project(input);
  polysmith::core::drawing_runtime::store_projection_at(
      document, view_id, result, document.revision);
  return result;
}

bool test_view_flatten_and_dashes() {
  // Two boxes: a front 20x20x10 box and a smaller 10x10x10 box
  // behind it (x in [-15,-5]) — the back box's edges project INSIDE
  // the front face, giving unique HIDDEN lines for the dash test.
  const TopoDS_Shape front_box = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();
  const TopoDS_Shape back_box =
      BRepPrimAPI_MakeBox(gp_Pnt(-15.0, 5.0, 0.0), gp_Pnt(-5.0, 15.0, 10.0))
          .Shape();

  DrawingFixture fixture = make_fixture();
  // The view must reference a REAL body id or the refresh marks it
  // broken; the box feature provides it (the projection itself is
  // injected below).
  fixture.manager.add_box_feature({.width = 20.0, .height = 20.0,
                                   .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      fixture.manager.get_document().value(), /*include_meshes=*/false);
  const std::string body_id = bodies.bodies[0].id;

  DrawingView view;
  view.kind = "projection";
  view.standard_view = "front";
  view.source_body_ids = {body_id};
  view.show_hidden = true;
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, view);
  const std::string view_id =
      fixture.document.drawing.drawings[0].views[0].view_id;

  ProjectionInput input;
  input.sources.push_back({"body-front", front_box});
  input.sources.push_back({"body-back", back_box});
  input.frame = polysmith::core::standard_view_frame("front").value();
  input.show_hidden = true;
  input.source_revision = 1;
  store_projection(fixture.document, view_id, input);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet with a view flattens")) {
    return false;
  }

  // Visible geometry: thick continuous.  Hidden: dashed thin with
  // the Annex-A corner rule — every dash ≤ 3 mm (12d), gaps 0.75 mm
  // (3d), the final dash extends to the corner, never a trailing gap.
  bool have_visible = false;
  bool have_hidden = false;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "view_geometry") {
      continue;
    }
    if (p.line_class == "visible") {
      have_visible = true;
      if (!near(p.style.width_mm, 0.5) ||
          p.style.line_type != "continuous") {
        std::cerr << "  visible style wrong\n";
        return false;
      }
    } else if (p.line_class == "hidden") {
      have_hidden = true;
      const double length = std::hypot(p.p1[0] - p.p0[0],
                                       p.p1[1] - p.p0[1]);
      if (!expect(p.style.width_mm == 0.25 &&
                      p.style.line_type == "continuous" &&
                      length <= 3.0 + 1e-4,
                  "hidden dashes: thin, continuous, ≤ 12d")) {
        return false;
      }
    }
  }
  if (!expect(have_visible && have_hidden,
              "flatten carries visible + dashed hidden geometry")) {
    return false;
  }

  // Exact span set for the back box's 10 mm bottom edge (view x 5..15
  // at y=0): dashes at 3/0.75 — [5,8] [8.75,11.75] [12.5,15] (the
  // final dash extends to the corner).
  std::vector<std::array<double, 2>> spans;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "view_geometry" || p.line_class != "hidden" ||
        p.kind != "line") {
      continue;
    }
    double x0 = p.p0[0], x1 = p.p1[0];
    if (x0 > x1) {
      std::swap(x0, x1);
    }
    // Every dash on the back box's bottom edge (y=0, x within 5..15).
    if (near(p.p0[1], 0.0, 1e-4) && near(p.p1[1], 0.0, 1e-4) &&
        x0 >= 5.0 - 1e-4 && x1 <= 15.0 + 1e-4) {
      spans.push_back({x0, x1});
    }
  }
  std::sort(spans.begin(), spans.end());
  const std::vector<std::array<double, 2>> expected = {
      {5.0, 8.0}, {8.75, 11.75}, {12.5, 15.0}};
  if (!expect(spans.size() == expected.size(),
              "10 mm hidden line: 3 dashes")) {
    return false;
  }
  for (size_t i = 0; i < std::min(spans.size(), expected.size()); ++i) {
    if (!expect(near(spans[i][0], expected[i][0], 1e-4) &&
                    near(spans[i][1], expected[i][1], 1e-4),
                "dash span matches the 12d/3d pattern")) {
      return false;
    }
  }
  // The corner rule: the last dash reaches the corner exactly — no
  // trailing gap.
  if (spans.size() == expected.size()) {
    return expect(near(spans.back()[1], 15.0, 1e-4),
                  "the final dash extends to the corner");
  }
  return true;
}

// ── Test 4: cutting-plane chain + coincidence priority ────────────

bool test_chain_and_priority() {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();

  DrawingFixture fixture = make_fixture();
  fixture.manager.add_box_feature({.width = 20.0, .height = 20.0,
                                   .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      fixture.manager.get_document().value(), /*include_meshes=*/false);
  const std::string body_id = bodies.bodies[0].id;

  DrawingView view;
  view.kind = "projection";
  view.standard_view = "front";
  view.source_body_ids = {body_id};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, view);
  const std::string view_id =
      fixture.document.drawing.drawings[0].views[0].view_id;

  // Two sibling sections: one at y=0 (its trace lies exactly ON the
  // visible left edge) and one at y=10 (a unique mid line).
  SectionDefinition edge_section;
  edge_section.cutting_plane_point = {0.0, 0.0, 0.0};
  edge_section.cutting_plane_normal = {0.0, 1.0, 0.0};
  SectionDefinition mid_section = edge_section;
  mid_section.cutting_plane_point = {0.0, 10.0, 0.0};

  ProjectionInput input;
  input.sources.push_back({"body-1", box});
  input.frame = polysmith::core::standard_view_frame("front").value();
  input.section_traces.push_back({edge_section});
  input.section_traces.push_back({mid_section});
  input.source_revision = 1;
  store_projection(fixture.document, view_id, input);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet with traces flattens")) {
    return false;
  }

  // The mid trace survives as chain segments: 10 mm line, pattern
  // 6 / 0.75 / dot 0.25 / 0.75 → dash [0,6], dot [6.75,7], dash
  // [7.75,10] (the final dash extends to the corner).
  std::vector<std::array<double, 2>> chain_spans;
  for (const auto& p : flat->primitives) {
    if (p.purpose != "cutting_plane") {
      continue;
    }
    double y0 = p.p0[1], y1 = p.p1[1];
    if (y0 > y1) {
      std::swap(y0, y1);
    }
    chain_spans.push_back({y0, y1});
  }
  std::sort(chain_spans.begin(), chain_spans.end());
  if (!expect(chain_spans.size() == 3,
              "10 mm chain trace: dash + dot + final dash")) {
    std::cerr << "  got " << chain_spans.size() << " spans\n";
    for (const auto& span : chain_spans) {
      std::cerr << "    " << span[0] << ".." << span[1] << "\n";
    }
    return false;
  }
  if (chain_spans.size() == 3) {
    if (!expect(near(chain_spans[0][0], 0.0, 1e-4) &&
                    near(chain_spans[0][1], 6.0, 1e-4) &&
                    near(chain_spans[1][0], 6.75, 1e-4) &&
                    near(chain_spans[1][1], 7.0, 1e-4) &&
                    near(chain_spans[2][0], 7.75, 1e-4) &&
                    near(chain_spans[2][1], 10.0, 1e-4),
                "chain pattern: 6 dash / 0.75 gap / 0.25 dot")) {
      return false;
    }
  }

  // Coincidence priority: the y=0 trace lies on the visible left
  // edge — the visible line wins, the chain is dropped.  The visible
  // edge exists exactly once with its thick continuous style.
  int edge_count = 0;
  for (const auto& p : flat->primitives) {
    if (p.purpose == "view_geometry" || p.purpose == "cutting_plane") {
      if (near(p.p0[0], 0.0, 1e-4) && near(p.p1[0], 0.0, 1e-4)) {
        ++edge_count;
        if (p.purpose != "view_geometry" ||
            p.style.line_type != "continuous" ||
            !near(p.style.width_mm, 0.5)) {
          std::cerr << "  overlapping geometry kept the wrong line\n";
          return false;
        }
      }
    }
  }
  return expect(edge_count == 1,
                "coincidence priority: visible beats the cutting-plane "
                "trace");
}

// ── Test 5: projection symbol orientation ─────────────────────────

bool test_symbol_orientation() {
  // First vs third angle flip the frustum: the base nearest the
  // circles is tall (first) or short (third).
  const auto near_base_height = [&](const std::string& angle) {
    DrawingFixture fixture = make_fixture("A4", "portrait", angle);
    const auto flat = polysmith::core::flatten_sheet(
        fixture.document, fixture.drawing_id, fixture.sheet_id);
    double height = -1.0;
    for (const auto& p : flat->primitives) {
      if (p.purpose != "projection_symbol" || p.kind != "line") {
        continue;
      }
      if (near(p.p0[0], p.p1[0], 1e-4) && near(p.p0[0], 110.0, 0.01)) {
        height = std::abs(p.p1[1] - p.p0[1]);
      }
    }
    return height;
  };
  const double first = near_base_height("first_angle");
  const double third = near_base_height("third_angle");
  if (!expect(near(first, 10.0) && near(third, 5.0),
              "first: large end nearest circles, third: small end")) {
    std::cerr << "  first " << first << " third " << third << "\n";
    return false;
  }
  return true;
}

// ── Test 6: drawing_sheet_update ──────────────────────────────────

bool test_sheet_update_mutator() {
  DrawingFixture fixture = make_fixture();

  const auto rejects = [&](const std::string& paper_size,
                           const std::string& orientation,
                           const std::string& angle) {
    bool threw = false;
    try {
      fixture.manager.drawing_sheet_update(
          fixture.drawing_id, fixture.sheet_id, paper_size, orientation,
          angle, "Sheet 1");
    } catch (const std::runtime_error&) {
      threw = true;
    }
    return threw;
  };
  if (!expect(rejects("A5", "portrait", "first_angle"),
              "unknown paper size rejected")) {
    return false;
  }
  if (!expect(rejects("A4", "square", "first_angle"),
              "unknown orientation rejected")) {
    return false;
  }
  if (!expect(rejects("A4", "portrait", "second_angle"),
              "unknown projection angle rejected")) {
    return false;
  }
  // Rejected updates leave no undo steps.
  const size_t undo_before =
      fixture.manager.get_document()->undo_step_names.size();

  fixture.document = fixture.manager.drawing_sheet_update(
      fixture.drawing_id, fixture.sheet_id, "A3", "landscape",
      "third_angle", "Sheet A");
  const auto& stored =
      fixture.document.drawing.drawings[0].sheets[0];
  if (!expect(stored.paper_size == "A3" && stored.orientation == "landscape" &&
                  stored.projection_angle == "third_angle" &&
                  stored.name == "Sheet A",
              "drawing_sheet_update replaces the settings")) {
    return false;
  }
  if (!expect(fixture.manager.get_document()->undo_step_names.size() ==
                  undo_before + 1,
              "the update pushes one undo step")) {
    return false;
  }

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value() && near(flat->width_mm, 420.0) &&
                  near(flat->height_mm, 297.0),
              "A3 landscape flattens as 420x297")) {
    return false;
  }
  // A3 grid ticks: long side 8 / short 6 → (8-1)*2 + (6-1)*2 = 24.
  return expect(count_purpose(flat.value(), "grid_ref") == 24,
                "A3 landscape grid: 8x6 divisions → 24 ticks");
}

// ── Test 7: determinism + golden ──────────────────────────────────

bool test_determinism_and_golden() {
  DrawingFixture fixture = make_fixture();
  fixture.manager.add_box_feature({.width = 20.0, .height = 20.0,
                                   .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      fixture.manager.get_document().value(), /*include_meshes=*/false);
  const std::string body_id = bodies.bodies[0].id;

  DrawingView front;
  front.kind = "projection";
  front.standard_view = "front";
  front.source_body_ids = {body_id};
  front.show_hidden = true;
  front.sheet_position = {40.0, 60.0};
  front.scale = 0.5;
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, front);

  DrawingView section_view;
  section_view.kind = "section";
  section_view.source_body_ids = {body_id};
  SectionDefinition section;
  section.cutting_plane_point = {0.0, 10.0, 0.0};
  section.cutting_plane_normal = {0.0, 1.0, 0.0};
  section_view.section = section;
  section_view.scale = 0.5;
  section_view.sheet_position = {160.0, 60.0};
  fixture.document = fixture.manager.drawing_view_create(
      fixture.drawing_id, fixture.sheet_id, section_view);

  const auto flat = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "full sheet flattens")) {
    return false;
  }
  const auto flat_again = polysmith::core::flatten_sheet(
      fixture.document, fixture.drawing_id, fixture.sheet_id);

  std::ostringstream os;
  os << std::fixed;
  os.precision(6);
  const auto dump = [&](const SheetPrimitiveStream& stream) {
    std::ostringstream out;
    out << std::fixed;
    out.precision(6);
    for (const auto& p : stream.primitives) {
      out << p.purpose << "|" << p.kind << "|" << p.line_class << "|"
          << quant(p.style.width_mm) << "|";
      if (p.kind == "line") {
        double x0 = quant(p.p0[0]), y0 = quant(p.p0[1]);
        double x1 = quant(p.p1[0]), y1 = quant(p.p1[1]);
        if (x0 > x1 || (x0 == x1 && y0 > y1)) {
          std::swap(x0, x1);
          std::swap(y0, y1);
        }
        out << x0 << "," << y0 << "|" << x1 << "," << y1;
      } else {
        out << quant(p.p0[0]) << "," << quant(p.p0[1]) << "|"
            << quant(p.p1[0]) << "," << quant(p.p1[1]);
        if (p.center.has_value()) {
          out << "|" << quant(p.center.value()[0]) << ","
              << quant(p.center.value()[1]);
        }
        if (p.radius.has_value()) {
          out << "|r" << quant(p.radius.value());
        }
        out << "|" << quant(p.start_angle) << "," << quant(p.end_angle);
      }
      out << "\n";
    }
    for (const auto& v : stream.views) {
      out << "view|" << v.label << "|" << quant(v.min[0]) << ","
          << quant(v.min[1]) << "|" << quant(v.max[0]) << ","
          << quant(v.max[1]) << "|" << (v.stale ? "stale" : "ok") << "\n";
    }
    return out.str();
  };
  const std::string text = dump(flat.value());
  if (!expect(text == dump(flat_again.value()),
              "flatten is deterministic")) {
    return false;
  }
  return check_golden("drawing_sheet_a4_full", text);
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_sheet_test\n";
  std::cout << "  Test 1: ISO 5457 trimmed sizes... ";
  if (test_paper_sizes()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: furniture... ";
  if (test_furniture()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: view flatten + dash patterns... ";
  if (test_view_flatten_and_dashes()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: chain + coincidence priority... ";
  if (test_chain_and_priority()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: projection symbol orientation... ";
  if (test_symbol_orientation()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: drawing_sheet_update... ";
  if (test_sheet_update_mutator()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: determinism + golden... ";
  if (test_determinism_and_golden()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cad_core_drawing_sheet_test passed\n";
    return 0;
  }
  return 1;
}
